#include "agent_module_impl.h"
#include "skill_registry.h"
#include "agent_config.h"
#include "spending_gate.h"
#include "persistence.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
bool isDeliveryContentTopic(const std::string& topic)
{
    // LIP-23 content topics accepted by delivery_module:
    //   /<application>/<version>/<topic-name>/<encoding>
    //   /gen/<application>/<version>/<topic-name>/<encoding>
    // Guard here because delivery_module currently aborts its process on invalid
    // send topics instead of returning an ordinary LogosResult failure.
    if (topic.empty() || topic[0] != '/') return false;
    std::vector<std::string> parts;
    std::stringstream ss(topic);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    if (parts.size() == 4) return true;
    return parts.size() == 5 && parts[0] == "gen";
}

long long nowMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

json parseJsonOrString(const std::string& value)
{
    try {
        if (!value.empty()) return json::parse(value);
    } catch (...) {}
    return value;
}

void appendTaskEvent(json& task, const std::string& status)
{
    if (!task.contains("events") || !task["events"].is_array()) task["events"] = json::array();
    task["events"].push_back(json{{"status", status}, {"at", nowMillis()}});
    task["updated_at"] = nowMillis();
}

json argsForSkill(const std::string& skill, const std::string& params)
{
    json parsed = parseJsonOrString(params);
    if (parsed.is_array()) return parsed;
    if (!parsed.is_object()) return json::array();

    if (skill == "messaging.send") return json::array({parsed.value("recipient", ""), parsed.value("message", "")});
    if (skill == "messaging.join") return json::array({parsed.value("group_id", "")});
    if (skill == "messaging.create_group") return json::array({parsed.value("members", "")});
    if (skill == "storage.upload") return json::array({parsed.value("path", ""), parsed.value("label", "")});
    if (skill == "storage.download") return json::array({parsed.value("address", ""), parsed.value("path", "")});
    if (skill == "storage.share") return json::array({parsed.value("address", ""), parsed.value("recipient", "")});
    if (skill == "wallet.send") return json::array({parsed.value("recipient", ""), parsed.value("amount_le16", "")});
    if (skill == "approval.approve") return json::array({parsed.value("approval_id", "")});
    if (skill == "approval.reject") return json::array({parsed.value("approval_id", ""), parsed.value("reason", "owner_rejected")});
    if (skill == "approval.retry") return json::array({parsed.value("approval_id", "")});
    if (skill == "program.query") return json::array({parsed.value("program_id", ""), parsed.value("params", "{}")});
    if (skill == "program.call") return json::array({parsed.value("program_id", ""), parsed.value("instruction", ""), parsed.value("params", "{}")});
    if (skill == "program.deploy") return json::array({parsed.value("binary_path", "")});
    if (skill == "agent.discover") return json::array({parsed.value("topic", "")});
    if (skill == "agent.subscribe") return json::array({parsed.value("agent_address", ""), parsed.value("task_id", "")});
    if (skill == "agent.receive") return json::array();
    if (skill == "agent.cancel") return json::array({parsed.value("agent_address", ""), parsed.value("task_id", "")});
    return json::array();
}
}

// ─── Construction & Lifecycle ──────────────────────────────────────────────

AgentModuleImpl::AgentModuleImpl()
{
    // Register all 23 skills as metadata for listing/discovery.
    auto& reg = g_registry();
    // Meta
    reg.addMeta("meta.skills", "meta", "List all available skills and their parameters", "{}", "[{...}]");
    reg.addMeta("meta.status", "meta", "Report agent state: balance, storage usage, active tasks", "{}", "{...}");
    reg.addMeta("meta.configure", "meta", "Update runtime configuration", R"({"key":"per_tx_limit","value":"500"})", "{...}");
    // Storage
    reg.addMeta("storage.upload", "storage", "Encrypt and upload a local file to Logos Storage", R"({"path":"/tmp/file","label":"doc"})", R"({"address":"...","label":"..."})");
    reg.addMeta("storage.download", "storage", "Retrieve and decrypt a file from Logos Storage", R"({"address":"0x...","path":"/tmp/out"})", R"({"path":"...","size":N})");
    reg.addMeta("storage.list", "storage", "List files the agent has stored", "{}", R"({"files":[...],"count":N})");
    reg.addMeta("storage.share", "storage", "Share access to a stored file with another identity", R"({"address":"0x...","recipient":"0x..."})", R"({"shared":true})");
    // Messaging
    reg.addMeta("messaging.send", "messaging", "Send a message to a Logos Messaging address", R"({"recipient":"0x...","message":"hello"})", R"({"sent":true})");
    reg.addMeta("messaging.join", "messaging", "Join a Logos Messaging group topic", R"({"group_id":"/logos/groups/123"})", R"({"joined":true})");
    reg.addMeta("messaging.create_group", "messaging", "Create a new group topic and invite members", R"({"members":"[\"0x...\"]"})", R"({"group_id":"..."})");
    // Wallet
    reg.addMeta("wallet.balance", "wallet", "Return the agent's current shielded token balance", "{}", R"({"balance":"..."})");
    reg.addMeta("wallet.send", "wallet", "Send tokens; enforce spending threshold", R"({"recipient":"0x...","amount_le16":"..."})", R"({"tx_hash":"...","approved":true})");
    reg.addMeta("wallet.history", "wallet", "Return summary of recent transactions", "{}", R"({"transactions":[...]})");
    reg.addMeta("approval.list", "wallet", "List pending and completed owner approvals", "{}", R"({"approvals":[...]})");
    reg.addMeta("approval.approve", "wallet", "Owner approval: execute a pending above-threshold wallet send", R"({"approval_id":"appr-..."})", R"({"approved":true,"executed":true})");
    reg.addMeta("approval.reject", "wallet", "Owner approval: reject a pending above-threshold wallet send", R"({"approval_id":"appr-...","reason":"..."})", R"({"rejected":true})");
    reg.addMeta("approval.retry", "wallet", "Retry or timeout owner notification for a pending approval", R"({"approval_id":"appr-..."})", R"({"notified":true})");
    // Program
    reg.addMeta("program.query", "program", "Read state from a LEZ program", R"({"program_id":"0x...","params":"{}"})", R"({"result":"..."})");
    reg.addMeta("program.call", "program", "Submit a transaction to a LEZ program; subject to spending threshold", R"({"program_id":"0x...","instruction":"vote","params":"{}"})", R"({"tx_hash":"..."})");
    reg.addMeta("program.deploy", "program", "Deploy a compiled LEZ program binary", R"({"binary_path":"/tmp/prog.bin"})", R"({"program_id":"..."})");
    // Agent (A2A)
    reg.addMeta("agent.card", "agent", "Return this agent's A2A Agent Card", "{}", "{...}");
    reg.addMeta("agent.discover", "agent", "Fetch Agent Cards from a discovery topic", R"({"topic":"/logos/agents/v1/discovery"})", R"({"agents":[...]})");
    reg.addMeta("agent.task", "agent", "Send an A2A task request to another agent", R"({"agent_address":"0x...","skill":"storage.upload","params":"{}"})", R"({"task_id":"...","status":"completed","result":{...}})");
    reg.addMeta("agent.complete", "agent", "Complete a task with a result payload", R"({"task_id":"task-1","result":"{}"})", R"({"completed":true,"status":"completed"})");
    reg.addMeta("agent.receive", "agent", "Process inbound A2A task messages from this agent's task topic", R"({})", R"({"processed":1,"tasks":[...]})");
    reg.addMeta("agent.subscribe", "agent", "Subscribe to streaming status for a running task", R"({"agent_address":"0x...","task_id":"task-1"})", R"({"subscribed":true})");
    reg.addMeta("agent.cancel", "agent", "Cancel an in-progress task and trigger refund", R"({"agent_address":"0x...","task_id":"task-1"})", R"({"cancelled":true,"refund":"..."})");
}

void AgentModuleImpl::onContextReady()
{
    m_contextReady = true;
    // Switch persistence to the host-provided directory if available.
    auto p = instancePersistencePath();
    if (!p.empty()) {
        agent_persistence::overrideStateDir(p);
    }

    ensureWallet();
}

// One-shot lazy bring-up of the real LEZ wallet. Invoked from onContextReady
// and again (defensively) on first wallet use, so the wallet works whether or
// not the host fired the lifecycle hook. On any failure the agent stays in
// simulated mode (m_agentAccount empty) and wallet skills fall back.
void AgentModuleImpl::ensureWallet()
{
    if (m_walletTried) return;
    m_walletTried = true;

    const char* disableWalletFfi = std::getenv("LP0008_DISABLE_WALLET_FFI");
    if (disableWalletFfi && std::string(disableWalletFfi) == "1") {
        return;
    }

    auto& cfg = agentConfig();
    const std::string walletDir = agent_persistence::stateDir() + "/wallet";
    if (m_wallet.init(walletDir, cfg.sequencer_addr)) {
        if (auto acct = m_wallet.findOwnedAccount(cfg.wallet_account_hex)) {
            m_agentAccount = acct->first;
            m_agentAccountIsPublic = acct->second;
            if (cfg.wallet_account_hex != m_agentAccount) {
                cfg.wallet_account_hex = m_agentAccount;
                cfg.save();
            }
        } else if (auto shielded = m_wallet.ensureShieldedAccount(cfg.wallet_account_hex)) {
            m_agentAccount = *shielded;
            m_agentAccountIsPublic = false;
            if (cfg.wallet_account_hex != *shielded) {
                cfg.wallet_account_hex = *shielded;
                cfg.save();
            }
        }
    }
}

// ─── Inter-Module Call Helpers ──────────────────────────────────────────────

bool AgentModuleImpl::ensureStorageReady()
{
    if (m_storageReady) return true;

    json initResult;
    const auto basePath = instancePersistencePath().empty()
        ? fs::temp_directory_path() / "logos-agent-storage"
        : fs::path(instancePersistencePath()) / "storage_module_live";
    fs::create_directories(basePath);

    json cfg;
    cfg["data-dir"] = basePath.string();
    if (!tryStorageCall("init", json::array({cfg.dump()}), initResult)) {
        return false;
    }

    json startResult;
    m_storageReady = tryStorageCall("start", json::array(), startResult);
    return m_storageReady;
}

bool AgentModuleImpl::ensureDeliveryReady()
{
    if (m_deliveryReady) return true;

    // Minimal config mirrored from delivery_module's own real integration tests:
    // Edge mode, local relay/sharding, no external peers required.
    json cfg;
    cfg["logLevel"] = "INFO";
    cfg["mode"] = "Edge";
    cfg["relay"] = true;
    cfg["numShardsInNetwork"] = 8;

    json createResult;
    if (!tryDeliveryCall("createNode", json::array({cfg.dump()}), createResult)) {
        return false;
    }

    json startResult;
    m_deliveryReady = tryDeliveryCall("start", json::array(), startResult);
    return m_deliveryReady;
}

bool AgentModuleImpl::tryStorageCall(const std::string& method, const json& args, json& out)
{
    if (!m_contextReady) return false;
    if (!m_storageClient) {
        try {
            m_storageClient = std::make_unique<logos::LpClient>("storage_module", "agent_module");
        } catch (...) {
            m_storageClient.reset();
            return false;
        }
    }
    logos::CallError err;
    auto result = m_storageClient->invoke(method, args, &err);
    if (err.code.empty() && !result.is_null()) {
        out = std::move(result);
        return !out.is_object() || !out.contains("success") || out.value("success", false);
    }
    return false;
}

bool AgentModuleImpl::tryDeliveryCall(const std::string& method, const json& args, json& out)
{
    if (!m_contextReady) return false;
    if (!m_deliveryClient) {
        try {
            m_deliveryClient = std::make_unique<logos::LpClient>("delivery_module", "agent_module");
        } catch (...) {
            m_deliveryClient.reset();
            return false;
        }
    }
    logos::CallError err;
    auto result = m_deliveryClient->invoke(method, args, &err);
    if (err.code.empty() && !result.is_null()) {
        out = std::move(result);
        return !out.is_object() || !out.contains("success") || out.value("success", false);
    }
    return false;
}

std::string AgentModuleImpl::greet(const std::string& name)
{
    return "Hello, " + name + "! Agent module v0.1.0 — "
         + std::to_string(g_registry().count()) + " skills loaded.";
}

// ─── Skill Dispatch ────────────────────────────────────────────────────────

std::string AgentModuleImpl::dispatchSkill(const std::string& skill_name, const std::string& args_json)
{
    try {
        json args = json::parse(args_json.empty() ? "[]" : args_json);
        if (!args.is_array()) args = json::array({args});

        auto s = [&](size_t i) -> std::string { return args.size() > i ? args[i].get<std::string>() : ""; };

        if (skill_name == "greet")           return greet(s(0));
        if (skill_name == "meta.skills")      return metaSkills();
        if (skill_name == "meta.status")      return metaStatus();
        if (skill_name == "meta.configure")   return metaConfigure(s(0), s(1));

        if (skill_name == "storage.upload")   return storageUpload(s(0), s(1));
        if (skill_name == "storage.download") return storageDownload(s(0), s(1));
        if (skill_name == "storage.list")     return storageList();
        if (skill_name == "storage.share")    return storageShare(s(0), s(1));

        if (skill_name == "messaging.send")        return messagingSend(s(0), s(1));
        if (skill_name == "messaging.join")        return messagingJoin(s(0));
        if (skill_name == "messaging.create_group") return messagingCreateGroup(s(0));

        if (skill_name == "wallet.balance")  return walletBalance();
        if (skill_name == "wallet.send")     return walletSend(s(0), s(1));
        if (skill_name == "wallet.history")  return walletHistory();
        if (skill_name == "approval.list")   return approvalList();
        if (skill_name == "approval.approve") return approvalApprove(s(0));
        if (skill_name == "approval.reject") return approvalReject(s(0), s(1));
        if (skill_name == "approval.retry")  return approvalRetry(s(0));

        if (skill_name == "program.query")   return programQuery(s(0), s(1));
        if (skill_name == "program.call")    return programCall(s(0), s(1), s(2));
        if (skill_name == "program.deploy")  return programDeploy(s(0));

        if (skill_name == "agent.card")      return agentCard();
        if (skill_name == "agent.discover")  return agentDiscover(s(0));
        if (skill_name == "agent.task")      return agentTask(s(0), s(1), s(2));
        if (skill_name == "agent.complete")  return agentComplete(s(0), s(1));
        if (skill_name == "agent.receive")   return agentReceive();
        if (skill_name == "agent.subscribe") return agentSubscribe(s(0), s(1));
        if (skill_name == "agent.cancel")    return agentCancel(s(0), s(1));

        json err;
        err["error"] = "unknown_skill";
        err["skill"] = skill_name;
        return err.dump();
    } catch (const std::exception& e) {
        json err;
        err["error"] = "dispatch_exception";
        err["what"] = e.what();
        return err.dump();
    }
}

// ─── Meta Skills ───────────────────────────────────────────────────────────

std::string AgentModuleImpl::metaSkills()
{
    return g_registry().listSkills();
}

std::string AgentModuleImpl::metaStatus()
{
    ensureWallet();
    json r;
    r["status"] = "running";
    r["skills_count"] = g_registry().count();
    r["context_ready"] = m_contextReady;
    r["instance_id"] = instanceId();
    r["persistence_path"] = instancePersistencePath();

    // Wallet summary
    auto& cfg = agentConfig();
    json wallet;
    wallet["per_tx_limit"] = cfg.per_tx_limit;
    wallet["per_period_limit"] = cfg.per_period_limit;
    wallet["period_seconds"] = cfg.period_seconds;
    r["wallet_config"] = wallet;

    // Wallet identity / liveness (no network call here — see wallet.balance).
    json wstatus;
    wstatus["live"] = m_wallet.live();
    wstatus["account"] = m_agentAccount;
    wstatus["mode"] = (m_wallet.live() && !m_agentAccount.empty()) ? "live" : "simulated";
    if (!m_agentAccount.empty()) wstatus["privacy"] = m_agentAccountIsPublic ? "public" : "private";
    if (m_wallet.live()) wstatus["sequencer"] = m_wallet.sequencerAddr();
    r["wallet"] = wstatus;

    // Storage summary
    auto store = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (store.is_object()) {
        r["storage_items"] = store.size();
    } else {
        r["storage_items"] = 0;
    }

    // Module connectivity
    json modules_status;
    modules_status["storage_module"] = (m_storageClient != nullptr) ? "connected" : "not_loaded";
    modules_status["delivery_module"] = (m_deliveryClient != nullptr) ? "connected" : "not_loaded";
    r["modules"] = modules_status;

    // Active tasks
    auto tasks = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    int active = 0;
    if (tasks.is_array()) {
        for (const auto& t : tasks) {
            const auto st = t.value("status", "");
            if (st == "queued" || st == "working") ++active;
        }
    }
    r["active_tasks"] = active;

    return r.dump();
}

std::string AgentModuleImpl::metaConfigure(const std::string& key, const std::string& value)
{
    auto& cfg = agentConfig();
    if (!cfg.set(key, value)) {
        json err;
        err["error"] = "unknown_config_key";
        err["key"] = key;
        return err.dump();
    }
    cfg.save();

    json r;
    r["updated"] = true;
    r["key"] = key;
    r["value"] = value;
    return r.dump();
}

// ─── Storage Skills ────────────────────────────────────────────────────────

std::string AgentModuleImpl::storageUpload(const std::string& path, const std::string& label)
{
    // Read file content
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        json err;
        err["error"] = "file_not_found";
        err["path"] = path;
        return err.dump();
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // Try real storage_module upload. A real upload requires the full session
    // lifecycle: uploadInit -> uploadChunk -> uploadFinalize. Returning the
    // uploadInit session id as an address was only a half-proof: it created a
    // session but did not persist file content in storage_module.
    json initResult;
    if (ensureStorageReady() && tryStorageCall("uploadInit", json::array({path}), initResult)) {
        const std::string sessionId = initResult.value("value", "");
        json chunkResult;
        json finalizeResult;
        if (!sessionId.empty()
            && tryStorageCall("uploadChunk", json::array({sessionId, content}), chunkResult)
            && tryStorageCall("uploadFinalize", json::array({sessionId}), finalizeResult)) {
            json entry;
            entry["label"] = label;
            entry["address"] = finalizeResult.value("value", "");
            entry["size"] = content.size();
            entry["path"] = path;
            entry["backend"] = "storage_module";
            entry["upload_init_result"] = initResult;
            entry["upload_chunk_result"] = chunkResult;
            entry["storage_result"] = finalizeResult;

            auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
            if (!data.is_object()) data = json::object();
            data[label] = entry;
            agent_persistence::saveJsonFile(agent_persistence::storagePath(), data);

            json r = entry;
            r["stored"] = true;
            r["mode"] = "live";
            return r.dump();
        }
    }

    // Fallback: local file persistence (simulated)
    // Generate content address (simple hash)
    size_t hash = std::hash<std::string>{}(content);
    std::string address = "0x" + std::to_string(hash);

    auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (!data.is_object()) data = json::object();

    json entry;
    entry["label"] = label;
    entry["address"] = address;
    entry["size"] = content.size();
    entry["path"] = path;
    entry["backend"] = "file";
    data[label] = entry;

    agent_persistence::saveJsonFile(agent_persistence::storagePath(), data);

    json r;
    r["address"] = address;
    r["label"] = label;
    r["size"] = content.size();
    r["backend"] = "file";
    r["stored"] = true;
    r["mode"] = "simulated";
    return r.dump();
}

std::string AgentModuleImpl::storageDownload(const std::string& address, const std::string& path)
{
    // Find by address in registry
    auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (!data.is_object()) data = json::object();

    std::string sourcePath;
    std::string label;
    for (auto& [k, v] : data.items()) {
        if (v.value("address", "") == address) {
            sourcePath = v.value("path", "");
            label = k;
            break;
        }
    }

    // Try real storage_module download. fetch() only asks the storage backend to
    // fetch/pin content; it does not write the requested output file. Use
    // downloadToUrl so the requested path is actually materialized.
    json realResult;
    const std::string fileUrl = "file://" + fs::absolute(path).string();
    if (ensureStorageReady() && tryStorageCall("downloadToUrl", json::array({address, fileUrl}), realResult)) {
        // downloadToUrl returns after the download session is accepted; the file
        // is materialized asynchronously. Poll briefly so dispatchSkill only
        // reports downloaded=true once the requested output path exists.
        bool downloaded = false;
        for (int i = 0; i < 50; ++i) {
            if (fs::exists(path)) {
                downloaded = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        json r;
        r["address"] = address;
        r["label"] = label;
        r["path"] = path;
        r["downloaded"] = downloaded;
        r["backend"] = "storage_module";
        r["mode"] = "live";
        r["storage_result"] = realResult;
        if (!downloaded) r["warning"] = "download accepted but output file was not materialized before timeout";
        return r.dump();
    }

    // Fallback: local file copy (simulated)
    json r;
    if (sourcePath.empty()) {
        r["error"] = "address_not_found";
        r["address"] = address;
        return r.dump();
    }

    // Copy file to requested path
    fs::copy_file(sourcePath, path, fs::copy_options::overwrite_existing);

    r["address"] = address;
    r["label"] = label;
    r["path"] = path;
    r["downloaded"] = true;
    r["mode"] = "simulated";
    return r.dump();
}

std::string AgentModuleImpl::storageList()
{
    // The agent owns labels and per-upload metadata in its registry. The
    // storage_module manifests call reports module-native manifests only; after
    // uploadInit it can legitimately return an empty array even though this
    // agent has stored files. Always build the user-facing list from the
    // registry, and include the live module response as diagnostic context when
    // available.
    auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (!data.is_object()) data = json::object();

    json files = json::array();
    for (auto& [label, v] : data.items()) {
        json f;
        f["label"] = label;
        f["address"] = v.value("address", "");
        f["size"] = v.value("size", 0);
        f["backend"] = v.value("backend", "file");
        if (v.contains("storage_result")) f["storage_result"] = v["storage_result"];
        files.push_back(f);
    }

    json r;
    r["files"] = files;
    r["count"] = files.size();

    json realResult;
    if (ensureStorageReady() && tryStorageCall("manifests", json::array(), realResult)) {
        r["storage_result"] = realResult;
        r["backend"] = "storage_module";
        r["mode"] = "live";
    } else {
        r["backend"] = "file";
        r["mode"] = "simulated";
    }
    return r.dump();
}

std::string AgentModuleImpl::storageShare(const std::string& address, const std::string& recipient)
{
    // Record sharing in registry
    auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (!data.is_object()) data = json::object();

    for (auto& [label, v] : data.items()) {
        if (v.value("address", "") == address) {
            json shares = v.value("shared_with", json::array());
            shares.push_back(recipient);
            v["shared_with"] = shares;
            agent_persistence::saveJsonFile(agent_persistence::storagePath(), data);

            json r;
            r["shared"] = true;
            r["address"] = address;
            r["recipient"] = recipient;
            r["label"] = label;
            return r.dump();
        }
    }

    json err;
    err["error"] = "address_not_found";
    err["address"] = address;
    return err.dump();
}

// ─── Messaging Skills ──────────────────────────────────────────────────────

std::string AgentModuleImpl::messagingSend(const std::string& recipient, const std::string& message)
{
    bool validContentTopic = isDeliveryContentTopic(recipient);

    // Try real delivery_module send only for valid LIP-23 content topics.
    // Invalid topics are kept in the local log instead of being forwarded into
    // delivery_module, which can abort the module process on malformed topics.
    json realResult;
    if (validContentTopic && ensureDeliveryReady() && tryDeliveryCall("send", json::array({recipient, message}), realResult)) {
        json r;
        r["sent"] = true;
        r["recipient"] = recipient;
        r["message_id"] = realResult.value("id", realResult.value("message_id", realResult.value("value", "")));
        r["mode"] = "live";
        r["delivery_result"] = realResult;

        // Until delivery_module exposes a receive iterator to Logos Core, keep a
        // local loopback copy for this agent's own task topic so inbound A2A
        // task execution can be proven against the same persisted inbox path.
        if (recipient == agentConfig().task_topic) {
            auto data = agent_persistence::loadJsonFile(agent_persistence::messagesPath());
            if (!data.is_array()) data = json::array();
            json msg;
            msg["direction"] = "in";
            msg["recipient"] = recipient;
            msg["message"] = message;
            msg["message_id"] = r["message_id"];
            msg["processed"] = false;
            msg["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            data.push_back(msg);
            agent_persistence::saveJsonFile(agent_persistence::messagesPath(), data);
        }
        return r.dump();
    }

    // Fallback: record in message log (simulated)
    auto data = agent_persistence::loadJsonFile(agent_persistence::messagesPath());
    if (!data.is_array()) data = json::array();

    json msg;
    msg["direction"] = (recipient == agentConfig().task_topic) ? "in" : "out";
    msg["recipient"] = recipient;
    msg["message"] = message;
    msg["message_id"] = "msg-" + std::to_string(data.size() + 1);
    msg["processed"] = false;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data.push_back(msg);

    agent_persistence::saveJsonFile(agent_persistence::messagesPath(), data);

    json r;
    r["sent"] = true;
    r["recipient"] = recipient;
    r["message_id"] = msg["message_id"];
    r["mode"] = "simulated";
    if (!validContentTopic) {
        r["note"] = "recipient is not a valid LIP-23 content topic for live delivery_module send";
    }
    return r.dump();
}

std::string AgentModuleImpl::messagingJoin(const std::string& groupId)
{
    bool validContentTopic = isDeliveryContentTopic(groupId);

    // Try real delivery_module join only for valid LIP-23 content topics.
    json realResult;
    if (validContentTopic && ensureDeliveryReady() && tryDeliveryCall("join", json::array({groupId}), realResult)) {
        json r;
        r["joined"] = true;
        r["group_id"] = groupId;
        r["mode"] = "live";
        return r.dump();
    }

    // Fallback: record in joined groups (simulated)
    auto data = agent_persistence::loadJsonFile(agent_persistence::groupsPath());
    if (!data.is_array()) data = json::array();

    data.push_back(groupId);
    agent_persistence::saveJsonFile(agent_persistence::groupsPath(), data);

    json r;
    r["joined"] = true;
    r["group_id"] = groupId;
    r["mode"] = "simulated";
    if (!validContentTopic) {
        r["note"] = "group_id is not a valid LIP-23 content topic for live delivery_module subscribe";
    }
    return r.dump();
}

std::string AgentModuleImpl::messagingCreateGroup(const std::string& members)
{
    // Parse members JSON array
    json memberList;
    try {
        memberList = json::parse(members);
    } catch (...) {
        memberList = json::array({members});
    }

    // Try real delivery_module create_group
    json realResult;
    if (ensureDeliveryReady() && tryDeliveryCall("createGroup", json::array({members}), realResult)) {
        json r;
        r["group_id"] = realResult.value("group_id", realResult.value("id", ""));
        r["members"] = memberList;
        r["created"] = true;
        r["mode"] = "live";
        return r.dump();
    }

    // Fallback: generate group ID locally (simulated)
    size_t hash = std::hash<std::string>{}(members + std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    std::string groupId = "/logos/groups/" + std::to_string(hash);

    // Record group
    auto data = agent_persistence::loadJsonFile(agent_persistence::groupsPath());
    if (!data.is_array()) data = json::array();

    json group;
    group["group_id"] = groupId;
    group["members"] = memberList;
    group["created"] = true;
    data.push_back(group);
    agent_persistence::saveJsonFile(agent_persistence::groupsPath(), data);

    json r;
    r["group_id"] = groupId;
    r["members"] = memberList;
    r["created"] = true;
    r["mode"] = "simulated";
    return r.dump();
}

// ─── Wallet Skills ─────────────────────────────────────────────────────────

std::string AgentModuleImpl::walletBalance()
{
    ensureWallet();
    // Real path: query the agent wallet balance from LEZ.
    if (m_wallet.live() && !m_agentAccount.empty()) {
        json r;
        if (auto bal = m_wallet.balanceDecimal(m_agentAccount, m_agentAccountIsPublic)) {
            r["balance"] = *bal;
        } else {
            r["balance"] = "0";
            r["note"] = m_wallet.lastError();
        }
        r["account"] = m_agentAccount;
        r["currency"] = "LEZ";
        r["sequencer"] = m_wallet.sequencerAddr();
        r["mode"] = "live";
        return r.dump();
    }

    // Fallback: file-backed simulated balance.
    auto data = agent_persistence::loadJsonFile(agent_persistence::walletPath());
    if (!data.is_object()) data = json::object();

    json r;
    r["balance"] = data.value("balance", "0");
    r["account"] = data.value("account", "agent-default-account");
    r["currency"] = "LEZ";
    r["mode"] = "simulated";
    return r.dump();
}

json AgentModuleImpl::executeWalletTransfer(const std::string& recipient, const std::string& amountLe16, const std::string& approvalId)
{
    ensureWallet();

    const auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string txHash, mode, error;
    bool submitted = false;

    if (m_wallet.live() && !m_agentAccount.empty()) {
        // Real path: transfer from the configured agent wallet identity.
        // Private accounts deshield to public recipients; public accounts use the
        // public transfer path so paid-A2A can keep moving when rc3 private
        // proving is blocked by upstream authenticated-transfer assertions.
        m_wallet.syncToCurrentBlock();
        auto amt = agent::WalletBridge::le16FromHex(amountLe16);
        auto res = m_agentAccountIsPublic
            ? m_wallet.transferPublic(m_agentAccount, recipient, amt)
            : m_wallet.transferDeshielded(m_agentAccount, recipient, amt);
        mode = "live";
        submitted = res.ok;
        txHash = res.tx_hash;
        error = res.error;
    } else {
        // Fallback: simulated send with a synthetic hash.
        mode = "simulated";
        submitted = true;
        txHash = "0x" + std::to_string(std::hash<std::string>{}(
            recipient + amountLe16 + approvalId + std::to_string(ts)));
    }

    json r;
    r["approved"] = true;
    r["submitted"] = submitted;
    r["amount"] = amountLe16;
    r["recipient"] = recipient;
    r["mode"] = mode;
    if (!approvalId.empty()) r["approval_id"] = approvalId;

    if (submitted) {
        spendingGate().recordSpend(amountLe16);
        r["tx_hash"] = txHash;

        auto wallet = agent_persistence::loadJsonFile(agent_persistence::walletPath());
        if (!wallet.is_object()) wallet = json::object();
        auto history = wallet.value("history", json::array());
        json tx;
        tx["type"] = approvalId.empty() ? "send" : "approved_send";
        tx["recipient"] = recipient;
        tx["amount"] = amountLe16;
        tx["tx_hash"] = txHash;
        tx["mode"] = mode;
        tx["timestamp"] = ts;
        if (!approvalId.empty()) tx["approval_id"] = approvalId;
        history.push_back(tx);
        wallet["history"] = history;
        agent_persistence::saveJsonFile(agent_persistence::walletPath(), wallet);
    } else {
        r["error"] = error;
    }
    return r;
}

json AgentModuleImpl::notifyOwnerApproval(const json& approval, const std::string& event)
{
    json envelope;
    envelope["type"] = "agent.approval." + event;
    envelope["approval_id"] = approval.value("approval_id", "");
    envelope["agent_id"] = agentConfig().agent_id;
    envelope["recipient"] = approval.value("recipient", "");
    envelope["amount_le16"] = approval.value("amount_le16", "");
    envelope["reason"] = approval.value("reason", "");
    envelope["status"] = approval.value("status", "pending");
    envelope["attempts"] = approval.value("notification_attempts", 0);
    envelope["created_at"] = approval.value("created_at", 0LL);
    envelope["expires_at"] = approval.value("expires_at", 0LL);

    json r = parseJsonOrString(messagingSend(agentConfig().owner_topic, envelope.dump()));
    if (!r.is_object()) r = json{{"sent", false}, {"raw", r}};
    r["owner_topic"] = agentConfig().owner_topic;
    r["event"] = event;
    return r;
}

std::string AgentModuleImpl::walletSend(const std::string& recipient, const std::string& amountLe16)
{
    ensureWallet();
    auto& gate = spendingGate();

    auto checkResult = json::parse(gate.checkSpend(amountLe16));
    if (!checkResult.value("allowed", false)) {
        auto approvals = agent_persistence::loadJsonFile(agent_persistence::approvalsPath());
        if (!approvals.is_array()) approvals = json::array();

        const auto now = nowMillis();
        const std::string approvalId = "appr-" + std::to_string(now) + "-" + std::to_string(approvals.size());
        json approval;
        approval["approval_id"] = approvalId;
        approval["status"] = "pending";
        approval["recipient"] = recipient;
        approval["amount_le16"] = amountLe16;
        approval["amount"] = checkResult.value("amount", amountLe16);
        approval["reason"] = checkResult.value("reason", "unknown");
        approval["owner_topic"] = agentConfig().owner_topic;
        approval["created_at"] = now;
        approval["updated_at"] = now;
        approval["expires_at"] = now + 300000; // five-minute default timeout for reviewer-visible safety
        approval["notification_attempts"] = 1;
        approval["events"] = json::array({json{{"status", "pending"}, {"at", now}}, json{{"status", "notified"}, {"at", now}}});

        json notify = notifyOwnerApproval(approval, "request");
        approval["last_notification"] = notify;
        approvals.push_back(approval);
        agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);

        json r;
        r["approved"] = false;
        r["reason"] = approval["reason"];
        r["amount"] = approval["amount"];
        r["action"] = "owner_approval_required";
        r["approval_id"] = approvalId;
        r["owner_topic"] = agentConfig().owner_topic;
        r["notification"] = notify;
        r["expires_at"] = approval["expires_at"];
        return r.dump();
    }

    return executeWalletTransfer(recipient, amountLe16).dump();
}


std::string AgentModuleImpl::approvalList()
{
    auto approvals = agent_persistence::loadJsonFile(agent_persistence::approvalsPath());
    if (!approvals.is_array()) approvals = json::array();
    json r;
    r["approvals"] = approvals;
    r["count"] = approvals.size();
    return r.dump();
}

std::string AgentModuleImpl::approvalApprove(const std::string& approvalId)
{
    auto approvals = agent_persistence::loadJsonFile(agent_persistence::approvalsPath());
    if (!approvals.is_array()) approvals = json::array();
    const auto now = nowMillis();

    for (auto& approval : approvals) {
        if (approval.value("approval_id", "") != approvalId) continue;
        json r;
        r["approval_id"] = approvalId;
        const std::string status = approval.value("status", "pending");
        if (status != "pending") {
            r["approved"] = false;
            r["executed"] = false;
            r["error"] = "approval_not_pending";
            r["status"] = status;
            return r.dump();
        }
        if (approval.value("expires_at", 0LL) > 0 && now > approval.value("expires_at", 0LL)) {
            approval["status"] = "timed_out";
            approval["updated_at"] = now;
            approval["events"].push_back(json{{"status", "timed_out"}, {"at", now}});
            agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);
            r["approved"] = false;
            r["executed"] = false;
            r["error"] = "approval_expired";
            r["status"] = "timed_out";
            return r.dump();
        }

        json exec = executeWalletTransfer(approval.value("recipient", ""), approval.value("amount_le16", ""), approvalId);
        bool submitted = exec.value("submitted", false);
        approval["status"] = submitted ? "executed" : "execution_failed";
        approval["updated_at"] = now;
        approval["execution"] = exec;
        approval["events"].push_back(json{{"status", "owner_approved"}, {"at", now}});
        approval["events"].push_back(json{{"status", approval["status"]}, {"at", now}});
        agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);

        r["approved"] = true;
        r["executed"] = submitted;
        r["status"] = approval["status"];
        r["execution"] = exec;
        return r.dump();
    }

    return json{{"approved", false}, {"executed", false}, {"error", "approval_not_found"}, {"approval_id", approvalId}}.dump();
}

std::string AgentModuleImpl::approvalReject(const std::string& approvalId, const std::string& reason)
{
    auto approvals = agent_persistence::loadJsonFile(agent_persistence::approvalsPath());
    if (!approvals.is_array()) approvals = json::array();
    const auto now = nowMillis();
    for (auto& approval : approvals) {
        if (approval.value("approval_id", "") != approvalId) continue;
        json r;
        r["approval_id"] = approvalId;
        const std::string status = approval.value("status", "pending");
        if (status != "pending") {
            r["rejected"] = false;
            r["error"] = "approval_not_pending";
            r["status"] = status;
            return r.dump();
        }
        approval["status"] = "rejected";
        approval["reject_reason"] = reason.empty() ? "owner_rejected" : reason;
        approval["updated_at"] = now;
        approval["events"].push_back(json{{"status", "owner_rejected"}, {"at", now}});
        agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);
        r["rejected"] = true;
        r["status"] = "rejected";
        r["reason"] = approval["reject_reason"];
        return r.dump();
    }
    return json{{"rejected", false}, {"error", "approval_not_found"}, {"approval_id", approvalId}}.dump();
}

std::string AgentModuleImpl::approvalRetry(const std::string& approvalId)
{
    auto approvals = agent_persistence::loadJsonFile(agent_persistence::approvalsPath());
    if (!approvals.is_array()) approvals = json::array();
    const auto now = nowMillis();
    for (auto& approval : approvals) {
        if (approval.value("approval_id", "") != approvalId) continue;
        json r;
        r["approval_id"] = approvalId;
        const std::string status = approval.value("status", "pending");
        if (status != "pending") {
            r["notified"] = false;
            r["status"] = status;
            r["error"] = "approval_not_pending";
            return r.dump();
        }
        if (approval.value("expires_at", 0LL) > 0 && now > approval.value("expires_at", 0LL)) {
            approval["status"] = "timed_out";
            approval["updated_at"] = now;
            approval["events"].push_back(json{{"status", "timed_out"}, {"at", now}});
            agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);
            r["notified"] = false;
            r["status"] = "timed_out";
            r["error"] = "approval_expired";
            return r.dump();
        }
        int attempts = approval.value("notification_attempts", 0) + 1;
        approval["notification_attempts"] = attempts;
        approval["updated_at"] = now;
        approval["events"].push_back(json{{"status", "notification_retry"}, {"at", now}});
        json notify = notifyOwnerApproval(approval, "retry");
        approval["last_notification"] = notify;
        agent_persistence::saveJsonFile(agent_persistence::approvalsPath(), approvals);
        r["notified"] = notify.value("sent", false);
        r["status"] = "pending";
        r["notification_attempts"] = attempts;
        r["notification"] = notify;
        return r.dump();
    }
    return json{{"notified", false}, {"error", "approval_not_found"}, {"approval_id", approvalId}}.dump();
}

std::string AgentModuleImpl::walletHistory()
{
    auto wallet = agent_persistence::loadJsonFile(agent_persistence::walletPath());
    if (!wallet.is_object()) wallet = json::object();

    json r;
    r["transactions"] = wallet.value("history", json::array());
    r["balance"] = wallet.value("balance", "0");
    return r.dump();
}

// ─── Program Skills ────────────────────────────────────────────────────────

std::string AgentModuleImpl::programQuery(const std::string& programId, const std::string& params)
{
    json r;
    r["program_id"] = programId;
    r["params"] = params;
    r["result"] = json::object();
    r["mode"] = "simulated";
    r["note"] = "live_program_query_unavailable: Logos Core module ABI exposes no in-process LEZ program query client";
    return r.dump();
}

std::string AgentModuleImpl::programCall(const std::string& programId, const std::string& instruction, const std::string& params)
{
    json r;
    r["program_id"] = programId;
    r["instruction"] = instruction;
    r["params"] = params;
    r["mode"] = "unsupported";
    r["submitted"] = false;
    r["error"] = "live_program_call_not_available";
    r["note"] = "program.call fails closed until Logos Core exposes a stable in-process LEZ program SDK/C ABI; rc3 SPEL/lgs external harnesses are documented separately";
    return r.dump();
}

std::string AgentModuleImpl::programDeploy(const std::string& binaryPath)
{
    if (!fs::exists(binaryPath)) {
        json err;
        err["error"] = "binary_not_found";
        err["path"] = binaryPath;
        err["deployed"] = false;
        return err.dump();
    }

    json r;
    r["path"] = binaryPath;
    r["binary_size"] = fs::file_size(binaryPath);
    r["mode"] = "unsupported";
    r["deployed"] = false;
    r["error"] = "live_program_deploy_not_available";
    r["note"] = "program.deploy fails closed inside the agent module; use the documented rc3 lgs deploy/SPEL path outside Logos Core until a module-safe program deployment API exists";
    return r.dump();
}

// ─── Agent Coordination (A2A) ──────────────────────────────────────────────

std::string AgentModuleImpl::agentCard()
{
    json card;
    const auto& cfg = agentConfig();
    card["name"] = cfg.agent_name;
    card["description"] = cfg.description;
    card["version"] = "1.0";
    card["protocol"] = "a2a";
    card["protocolVersion"] = "1.0";
    card["status"] = "active";
    card["agent_id"] = !cfg.agent_id.empty()
                         ? cfg.agent_id
                         : (!m_agentAccount.empty()
                            ? m_agentAccount
                            : (instanceId().empty() ? "default-agent" : instanceId()));
    card["account"] = m_agentAccount;  // LEZ wallet account (empty in simulated mode)
    card["account_privacy"] = m_agentAccountIsPublic ? "public" : "private";
    card["discovery_topic"] = cfg.discovery_topic;
    card["task_topic"] = cfg.task_topic;
    card["owner_topic"] = cfg.owner_topic;

    // A2A capabilities from skill registry
    json capabilities = json::parse(g_registry().listSkills());
    card["skills"] = capabilities;

    // A2A standard fields
    card["authentication"] = json::object({{"type", "logos_identity"}});
    card["payment"] = json::object({
        {"currency", "LEZ"},
        {"per_task", cfg.a2a_payment_amount_le16.empty() ? "0" : cfg.a2a_payment_amount_le16},
        {"recipient", cfg.a2a_payment_recipient},
        {"mechanism", "lez_transfer"}
    });

    // Publish/update our Agent Card in the local discovery registry. This gives
    // multi-agent demos a deterministic A2A discovery path even when no live
    // delivery subscription loop is running yet.
    auto registry = agent_persistence::loadJsonFile(agent_persistence::discoveryPath());
    if (!registry.is_array()) registry = json::array();
    bool replaced = false;
    const auto id = card.value("agent_id", "");
    for (auto& existing : registry) {
        if (existing.is_object() && existing.value("agent_id", "") == id) {
            existing = card;
            replaced = true;
            break;
        }
    }
    if (!replaced) registry.push_back(card);
    agent_persistence::saveJsonFile(agent_persistence::discoveryPath(), registry);

    return card.dump();
}

std::string AgentModuleImpl::agentDiscover(const std::string& topic)
{
    // Load discovery registry
    auto data = agent_persistence::loadJsonFile(agent_persistence::discoveryPath());
    if (!data.is_array()) data = json::array();

    json r;
    r["topic"] = topic;
    r["agents"] = data;
    r["count"] = data.size();
    return r.dump();
}

std::string AgentModuleImpl::agentTask(const std::string& agentAddress, const std::string& skill, const std::string& params)
{
    const long long created = nowMillis();

    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    if (!data.is_array()) data = json::array();

    std::string taskId = "task-" + std::to_string(created) + "-" + std::to_string(data.size());
    json task;
    task["task_id"] = taskId;
    task["agent_address"] = agentAddress;
    task["skill"] = skill;
    task["params"] = parseJsonOrString(params);
    task["status"] = "queued";
    task["created_at"] = created;
    appendTaskEvent(task, "queued");

    // Publish an A2A task envelope to the target agent's task topic when the
    // target can be resolved from the discovered Agent Card registry. If the
    // delivery module is co-loaded and the topic is valid LIP-23, this path uses
    // live Logos Messaging; otherwise messagingSend records the same envelope in
    // the local persisted outbox/inbox for deterministic tests.
    std::string targetTaskTopic;
    auto registry = agent_persistence::loadJsonFile(agent_persistence::discoveryPath());
    if (registry.is_array()) {
        for (const auto& card : registry) {
            if (!card.is_object()) continue;
            if (card.value("agent_id", "") == agentAddress ||
                card.value("account", "") == agentAddress ||
                card.value("name", "") == agentAddress) {
                targetTaskTopic = card.value("task_topic", "");
                break;
            }
        }
    }
    if (targetTaskTopic.empty() && !agentAddress.empty() && agentAddress[0] == '/') {
        targetTaskTopic = agentAddress;
    }
    if (!targetTaskTopic.empty()) {
        json envelope;
        envelope["type"] = "a2a.task.request";
        envelope["task_id"] = taskId;
        envelope["from"] = agentConfig().agent_id;
        envelope["to"] = agentAddress;
        envelope["skill"] = skill;
        envelope["params"] = parseJsonOrString(params);
        envelope["created_at"] = created;
        json transport = parseJsonOrString(messagingSend(targetTaskTopic, envelope.dump()));
        task["transport"] = json{{"topic", targetTaskTopic}, {"result", transport}};
        appendTaskEvent(task, "transport_sent");
    }

    // Local executor still runs synchronously for deterministic C-ABI and CI
    // proof. The transport evidence above shows the same A2A task envelope is
    // also emitted over the Logos Messaging path when available.
    task["status"] = "working";
    appendTaskEvent(task, "working");

    json result;
    bool failed = false;
    if (skill == "agent.task" || skill == "agent.complete") {
        failed = true;
        result = json{{"error", "unsupported_task_skill"}, {"skill", skill}};
    } else {
        const auto args = argsForSkill(skill, params).dump();
        result = parseJsonOrString(dispatchSkill(skill, args));
        failed = result.is_object() && result.contains("error");
    }

    if (failed) {
        task["status"] = "failed";
        task["error"] = result;
        appendTaskEvent(task, "failed");
    } else {
        task["status"] = "completed";
        task["result"] = result;
        appendTaskEvent(task, "completed");
    }

    const auto& cfg = agentConfig();
    if (!failed && !cfg.a2a_payment_recipient.empty() && !cfg.a2a_payment_amount_le16.empty()
        && cfg.a2a_payment_amount_le16 != "0") {
        json payment = parseJsonOrString(walletSend(cfg.a2a_payment_recipient, cfg.a2a_payment_amount_le16));
        task["payment"] = payment;
        appendTaskEvent(task, payment.value("submitted", false) ? "payment_submitted" : "payment_recorded");
    }

    data.push_back(task);
    agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);

    json r;
    r["task_id"] = taskId;
    r["status"] = task["status"];
    r["agent_address"] = agentAddress;
    r["skill"] = skill;
    if (task.contains("result")) r["result"] = task["result"];
    if (task.contains("error")) r["error"] = task["error"];
    if (task.contains("transport")) r["transport"] = task["transport"];
    if (task.contains("payment")) r["payment"] = task["payment"];
    r["events"] = task["events"];
    return r.dump();
}

std::string AgentModuleImpl::agentComplete(const std::string& taskId, const std::string& resultJson)
{
    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    if (!data.is_array()) data = json::array();

    for (auto& t : data) {
        if (t.value("task_id", "") == taskId) {
            t["status"] = "completed";
            t["result"] = parseJsonOrString(resultJson);
            t.erase("error");
            appendTaskEvent(t, "completed");
            agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);

            json r;
            r["completed"] = true;
            r["task_id"] = taskId;
            r["status"] = "completed";
            r["result"] = t["result"];
            r["events"] = t["events"];
            return r.dump();
        }
    }

    json r;
    r["completed"] = false;
    r["task_id"] = taskId;
    r["error"] = "task_not_found";
    return r.dump();
}

std::string AgentModuleImpl::agentReceive()
{
    auto data = agent_persistence::loadJsonFile(agent_persistence::messagesPath());
    if (!data.is_array()) data = json::array();

    json processedTasks = json::array();
    int processed = 0;
    const auto topic = agentConfig().task_topic;

    for (auto& msg : data) {
        if (msg.value("processed", false)) continue;
        if (msg.value("recipient", "") != topic) continue;

        json payload;
        try {
            payload = json::parse(msg.value("message", ""));
        } catch (...) {
            msg["processed"] = true;
            msg["error"] = "invalid_task_envelope";
            ++processed;
            continue;
        }

        const std::string skill = payload.value("skill", "");
        if (skill.empty()) {
            msg["processed"] = true;
            msg["error"] = "missing_skill";
            ++processed;
            continue;
        }

        std::string params = "{}";
        if (payload.contains("params")) {
            params = payload["params"].is_string() ? payload["params"].get<std::string>() : payload["params"].dump();
        }
        const std::string from = payload.value("from", payload.value("agent_address", "inbound"));

        json task = parseJsonOrString(agentTask(from, skill, params));
        msg["processed"] = true;
        msg["task_id"] = task.value("task_id", "");
        msg["task_status"] = task.value("status", "unknown");
        processedTasks.push_back(task);
        ++processed;
    }

    agent_persistence::saveJsonFile(agent_persistence::messagesPath(), data);

    json r;
    r["processed"] = processed;
    r["task_topic"] = topic;
    r["tasks"] = processedTasks;
    return r.dump();
}

std::string AgentModuleImpl::agentSubscribe(const std::string& agentAddress, const std::string& taskId)
{
    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    if (data.is_array()) {
        for (auto& t : data) {
            if (t.value("task_id", "") == taskId) {
                t["subscribed"] = true;
                agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);

                json r;
                r["subscribed"] = true;
                r["task_id"] = taskId;
                r["current_status"] = t.value("status", "unknown");
                if (t.contains("result")) r["result"] = t["result"];
                if (t.contains("error")) r["error"] = t["error"];
                if (t.contains("events")) r["events"] = t["events"];
                return r.dump();
            }
        }
    }

    json r;
    r["subscribed"] = false;
    r["task_id"] = taskId;
    r["current_status"] = "not_found";
    return r.dump();
}

std::string AgentModuleImpl::agentCancel(const std::string& agentAddress, const std::string& taskId)
{
    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    std::string refund = "0";
    if (data.is_array()) {
        for (auto& t : data) {
            if (t.value("task_id", "") == taskId) {
                const auto st = t.value("status", "");
                if (st == "completed" || st == "failed" || st == "cancelled") {
                    json r;
                    r["cancelled"] = false;
                    r["task_id"] = taskId;
                    r["current_status"] = st;
                    if (t.contains("result")) r["result"] = t["result"];
                    if (t.contains("error")) r["error"] = t["error"];
                    return r.dump();
                }

                t["status"] = "cancelled";
                refund = t.value("paid", "0");
                appendTaskEvent(t, "cancelled");
                agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);

                json r;
                r["cancelled"] = true;
                r["task_id"] = taskId;
                r["refund"] = refund;
                r["current_status"] = "cancelled";
                r["events"] = t["events"];
                return r.dump();
            }
        }
    }

    json r;
    r["cancelled"] = false;
    r["task_id"] = taskId;
    r["error"] = "task_not_found";
    return r.dump();
}
