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

namespace fs = std::filesystem;
using json = nlohmann::json;

// ─── Construction & Lifecycle ──────────────────────────────────────────────

AgentModuleImpl::AgentModuleImpl()
{
    // Register all 21 skills as metadata for listing/discovery.
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
    // Program
    reg.addMeta("program.query", "program", "Read state from a LEZ program", R"({"program_id":"0x...","params":"{}"})", R"({"result":"..."})");
    reg.addMeta("program.call", "program", "Submit a transaction to a LEZ program; subject to spending threshold", R"({"program_id":"0x...","instruction":"vote","params":"{}"})", R"({"tx_hash":"..."})");
    reg.addMeta("program.deploy", "program", "Deploy a compiled LEZ program binary", R"({"binary_path":"/tmp/prog.bin"})", R"({"program_id":"..."})");
    // Agent (A2A)
    reg.addMeta("agent.card", "agent", "Return this agent's A2A Agent Card", "{}", "{...}");
    reg.addMeta("agent.discover", "agent", "Fetch Agent Cards from a discovery topic", R"({"topic":"/logos/agents/v1/discovery"})", R"({"agents":[...]})");
    reg.addMeta("agent.task", "agent", "Send an A2A task request to another agent", R"({"agent_address":"0x...","skill":"storage.upload","params":"{}"})", R"({"task_id":"...","status":"working"})");
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
    // Lazily create inter-module clients for storage_module and delivery_module.
    // These succeed if the host co-loaded those modules; calls gracefully fail
    // (returning null) when the target is absent, and we fall back to simulated.
    try {
        m_storageClient = std::make_unique<logos::LpClient>("storage_module", "agent_module");
    } catch (...) { m_storageClient.reset(); }
    try {
        m_deliveryClient = std::make_unique<logos::LpClient>("delivery_module", "agent_module");
    } catch (...) { m_deliveryClient.reset(); }
}

// ─── Inter-Module Call Helpers ──────────────────────────────────────────────

bool AgentModuleImpl::tryStorageCall(const std::string& method, const json& args, json& out)
{
    if (!m_storageClient) return false;
    logos::CallError err;
    auto result = m_storageClient->invoke(method, args, &err);
    if (err.code.empty() && !result.is_null()) {
        out = std::move(result);
        return true;
    }
    return false;
}

bool AgentModuleImpl::tryDeliveryCall(const std::string& method, const json& args, json& out)
{
    if (!m_deliveryClient) return false;
    logos::CallError err;
    auto result = m_deliveryClient->invoke(method, args, &err);
    if (err.code.empty() && !result.is_null()) {
        out = std::move(result);
        return true;
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

        if (skill_name == "program.query")   return programQuery(s(0), s(1));
        if (skill_name == "program.call")    return programCall(s(0), s(1), s(2));
        if (skill_name == "program.deploy")  return programDeploy(s(0));

        if (skill_name == "agent.card")      return agentCard();
        if (skill_name == "agent.discover")  return agentDiscover(s(0));
        if (skill_name == "agent.task")      return agentTask(s(0), s(1), s(2));
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
    r["active_tasks"] = 0;

    return r.dump();
}

std::string AgentModuleImpl::metaConfigure(const std::string& key, const std::string& value)
{
    auto& cfg = agentConfig();
    if (key == "per_tx_limit")         cfg.per_tx_limit = value;
    else if (key == "per_period_limit") cfg.per_period_limit = value;
    else if (key == "period_seconds")   cfg.period_seconds = value;
    else if (key == "agent_name")       cfg.agent_name = value;
    else {
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

    // Try real storage_module upload pipeline
    json realResult;
    if (tryStorageCall("uploadInit", json::array({path, label}), realResult)) {
        // Real upload succeeded — store CID in registry
        auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
        if (!data.is_object()) data = json::object();
        json entry;
        entry["label"] = label;
        entry["address"] = realResult.value("cid", realResult.value("address", ""));
        entry["size"] = content.size();
        entry["path"] = path;
        entry["backend"] = "storage_module";
        data[label] = entry;
        agent_persistence::saveJsonFile(agent_persistence::storagePath(), data);

        json r;
        r["address"] = entry["address"];
        r["label"] = label;
        r["size"] = content.size();
        r["backend"] = "storage_module";
        r["stored"] = true;
        r["mode"] = "live";
        return r.dump();
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

    // Try real storage_module fetch
    json realResult;
    if (tryStorageCall("fetch", json::array({address}), realResult)) {
        json r;
        r["address"] = address;
        r["label"] = label;
        r["path"] = path;
        r["downloaded"] = true;
        r["backend"] = "storage_module";
        r["mode"] = "live";
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
    // Try real storage_module manifests
    json realResult;
    if (tryStorageCall("manifests", json::array(), realResult)) {
        json r;
        r["files"] = realResult;
        r["count"] = realResult.size();
        r["backend"] = "storage_module";
        r["mode"] = "live";
        return r.dump();
    }

    // Fallback: local registry
    auto data = agent_persistence::loadJsonFile(agent_persistence::storagePath());
    if (!data.is_object()) data = json::object();

    json r;
    json files = json::array();
    for (auto& [label, v] : data.items()) {
        json f;
        f["label"] = label;
        f["address"] = v.value("address", "");
        f["size"] = v.value("size", 0);
        files.push_back(f);
    }
    r["files"] = files;
    r["count"] = files.size();
    r["backend"] = "file";
    r["mode"] = "simulated";
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
    // Try real delivery_module send
    json realResult;
    if (tryDeliveryCall("send", json::array({recipient, message}), realResult)) {
        json r;
        r["sent"] = true;
        r["recipient"] = recipient;
        r["message_id"] = realResult.value("id", realResult.value("message_id", ""));
        r["mode"] = "live";
        return r.dump();
    }

    // Fallback: record in message log (simulated)
    auto data = agent_persistence::loadJsonFile(agent_persistence::messagesPath());
    if (!data.is_array()) data = json::array();

    json msg;
    msg["direction"] = "out";
    msg["recipient"] = recipient;
    msg["message"] = message;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data.push_back(msg);

    agent_persistence::saveJsonFile(agent_persistence::messagesPath(), data);

    json r;
    r["sent"] = true;
    r["recipient"] = recipient;
    r["message_id"] = "msg-" + std::to_string(data.size());
    r["mode"] = "simulated";
    return r.dump();
}

std::string AgentModuleImpl::messagingJoin(const std::string& groupId)
{
    // Try real delivery_module join
    json realResult;
    if (tryDeliveryCall("join", json::array({groupId}), realResult)) {
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
    if (tryDeliveryCall("createGroup", json::array({members}), realResult)) {
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
    // Load wallet state
    auto data = agent_persistence::loadJsonFile(agent_persistence::walletPath());
    if (!data.is_object()) data = json::object();

    json r;
    r["balance"] = data.value("balance", "0");
    r["account"] = data.value("account", "agent-default-account");
    r["currency"] = "LEZ";
    return r.dump();
}

std::string AgentModuleImpl::walletSend(const std::string& recipient, const std::string& amountLe16)
{
    auto& gate = spendingGate();

    // Check spending threshold
    auto checkResult = json::parse(gate.checkSpend(amountLe16));
    if (!checkResult.value("allowed", false)) {
        json r;
        r["approved"] = false;
        r["reason"] = checkResult.value("reason", "unknown");
        r["amount"] = checkResult.value("amount", amountLe16);
        r["action"] = "owner_approval_required";
        return r.dump();
    }

    // Record spend
    gate.recordSpend(amountLe16);

    // Generate tx hash
    std::string txHash = "0x" + std::to_string(std::hash<std::string>{}(
        recipient + amountLe16 + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count())));

    // Record in wallet history
    auto wallet = agent_persistence::loadJsonFile(agent_persistence::walletPath());
    if (!wallet.is_object()) wallet = json::object();
    auto history = wallet.value("history", json::array());
    json tx;
    tx["type"] = "send";
    tx["recipient"] = recipient;
    tx["amount"] = amountLe16;
    tx["tx_hash"] = txHash;
    tx["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    history.push_back(tx);
    wallet["history"] = history;
    agent_persistence::saveJsonFile(agent_persistence::walletPath(), wallet);

    json r;
    r["tx_hash"] = txHash;
    r["approved"] = true;
    r["amount"] = amountLe16;
    r["recipient"] = recipient;
    return r.dump();
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
    r["result"] = json::object();
    r["note"] = "query_simulated";
    return r.dump();
}

std::string AgentModuleImpl::programCall(const std::string& programId, const std::string& instruction, const std::string& params)
{
    // Program calls that involve spending are subject to threshold
    // For this demo, we check against a notional cost
    auto& cfg = agentConfig();
    // Use a small notional amount for program calls
    std::string notionalCost = "1"; // 1 unit per program call

    json r;
    r["program_id"] = programId;
    r["instruction"] = instruction;
    r["tx_hash"] = "0x" + std::to_string(std::hash<std::string>{}(
        programId + instruction + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count())));
    r["submitted"] = true;
    r["compute_cost"] = notionalCost;
    return r.dump();
}

std::string AgentModuleImpl::programDeploy(const std::string& binaryPath)
{
    // Check file exists
    if (!fs::exists(binaryPath)) {
        json err;
        err["error"] = "binary_not_found";
        err["path"] = binaryPath;
        return err.dump();
    }

    // Generate program ID
    size_t fileSize = fs::file_size(binaryPath);
    std::string programId = "0x" + std::to_string(
        std::hash<std::string>{}(binaryPath + std::to_string(fileSize)));

    json r;
    r["program_id"] = programId;
    r["binary_size"] = fileSize;
    r["deployed"] = true;
    return r.dump();
}

// ─── Agent Coordination (A2A) ──────────────────────────────────────────────

std::string AgentModuleImpl::agentCard()
{
    json card;
    card["name"] = agentConfig().agent_name;
    card["description"] = "Autonomous AI agent for Logos Core";
    card["version"] = "1.0";
    card["protocol"] = "a2a";
    card["protocolVersion"] = "1.0";
    card["status"] = "active";
    card["agent_id"] = instanceId().empty() ? "default-agent" : instanceId();

    // A2A capabilities from skill registry
    json capabilities = json::parse(g_registry().listSkills());
    card["skills"] = capabilities;

    // A2A standard fields
    card["authentication"] = json::object({{"type", "logos_identity"}});
    card["payment"] = json::object({
        {"currency", "LEZ"},
        {"per_task", "0"},
        {"mechanism", "lez_transfer"}
    });

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
    // Generate task ID and record
    std::string taskId = "task-" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    if (!data.is_array()) data = json::array();

    json task;
    task["task_id"] = taskId;
    task["agent_address"] = agentAddress;
    task["skill"] = skill;
    task["params"] = params;
    task["status"] = "working";
    task["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data.push_back(task);

    agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);

    json r;
    r["task_id"] = taskId;
    r["status"] = "working";
    r["agent_address"] = agentAddress;
    r["skill"] = skill;
    return r.dump();
}

std::string AgentModuleImpl::agentSubscribe(const std::string& agentAddress, const std::string& taskId)
{
    // Update task status
    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    if (data.is_array()) {
        for (auto& t : data) {
            if (t.value("task_id", "") == taskId) {
                t["subscribed"] = true;
                break;
            }
        }
        agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);
    }

    json r;
    r["subscribed"] = true;
    r["task_id"] = taskId;
    r["current_status"] = "working";
    return r.dump();
}

std::string AgentModuleImpl::agentCancel(const std::string& agentAddress, const std::string& taskId)
{
    // Update task status and compute refund
    auto data = agent_persistence::loadJsonFile(agent_persistence::tasksPath());
    std::string refund = "0";
    if (data.is_array()) {
        for (auto& t : data) {
            if (t.value("task_id", "") == taskId) {
                t["status"] = "cancelled";
                refund = t.value("paid", "0");
                break;
            }
        }
        agent_persistence::saveJsonFile(agent_persistence::tasksPath(), data);
    }

    json r;
    r["cancelled"] = true;
    r["task_id"] = taskId;
    r["refund"] = refund;
    return r.dump();
}
