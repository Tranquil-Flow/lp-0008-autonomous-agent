#include "agent_module_impl.h"
#include "skill_registry.h"
#include "agent_config.h"
#include "spending_gate.h"
#include "persistence.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>

namespace {
    SkillRegistry g_registry;
    auto g_start_time = std::chrono::steady_clock::now();

}

AgentModuleImpl::AgentModuleImpl()
{
    if (!skills_registered_) {
        registerAllSkills();
        skills_registered_ = true;
    }
}

void AgentModuleImpl::registerAllSkills()
{
    // ================================================================
    // META SKILLS
    // ================================================================
    g_registry.registerSkill(
        "meta.skills", "meta",
        "List all registered skills the agent can execute",
        "{}", "[{...}]",
        [](const std::string&) -> std::string {
            nlohmann::json r;
            r["skills"] = nlohmann::json::parse(g_registry.listSkills());
            r["count"] = g_registry.count();
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "meta.status", "meta",
        "Return agent health, config summary, and uptime",
        "{}", "{...}",
        [](const std::string&) -> std::string {
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - g_start_time).count();
            nlohmann::json r;
            r["status"] = "active";
            r["uptime_seconds"] = uptime;
            r["skill_count"] = g_registry.count();
            r["config"] = nlohmann::json::parse(agentConfig().toJson());
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "meta.configure", "meta",
        "Update a runtime config key",
        "{\"key\":\"per_tx_limit\",\"value\":\"500\"}", "{...}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string key = args.value("key", "");
            std::string value = args.value("value", "");
            bool ok = agentConfig().set(key, value);
            nlohmann::json r;
            r["key"] = key;
            r["updated"] = ok;
            return r.dump();
        }
    );

    // ================================================================
    // STORAGE SKILLS
    // ================================================================
    g_registry.registerSkill(
        "storage.store", "storage",
        "Store data by key. Returns the key and content hash.",
        "{\"key\":\"document1\",\"data\":\"hello world\"}", "{\"key\":\"...\",\"stored\":true}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string key = args.value("key", "");
            std::string data = args.value("data", "");
            auto storage = agent_persistence::loadStorage();
            storage["entries"][key] = data;
            std::string error;
            bool saved = agent_persistence::saveStorage(storage, &error);
            nlohmann::json r;
            r["key"] = key;
            r["stored"] = saved;
            r["size"] = data.size();
            r["backend"] = "file";
            r["path"] = agent_persistence::storagePath();
            if (!saved) r["error"] = error;
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "storage.retrieve", "storage",
        "Retrieve stored data by key.",
        "{\"key\":\"document1\"}", "{\"key\":\"...\",\"data\":\"...\"}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string key = args.value("key", "");
            nlohmann::json r;
            r["key"] = key;
            auto storage = agent_persistence::loadStorage();
            if (storage.contains("entries") && storage["entries"].contains(key)) {
                r["found"] = true;
                r["data"] = storage["entries"][key];
            } else {
                r["found"] = false;
                r["error"] = "not_found";
            }
            r["backend"] = "file";
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "storage.list", "storage",
        "List all stored keys.",
        "{}", "{\"keys\":[...],\"count\":N}",
        [](const std::string&) -> std::string {
            nlohmann::json r;
            nlohmann::json keys = nlohmann::json::array();
            auto storage = agent_persistence::loadStorage();
            if (storage.contains("entries") && storage["entries"].is_object()) {
                for (auto it = storage["entries"].begin(); it != storage["entries"].end(); ++it) {
                    keys.push_back(it.key());
                }
            }
            r["keys"] = keys;
            r["count"] = keys.size();
            r["backend"] = "file";
            return r.dump();
        }
    );

    // ================================================================
    // BLOCKCHAIN SKILLS
    // ================================================================
    g_registry.registerSkill(
        "chain.balance", "chain",
        "Get wallet balance for an account.",
        "{\"account_hex\":\"abc123\"}", "{\"balance\":\"...\"}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string account = args.value("account_hex", agentConfig().wallet_account_hex);
            nlohmann::json r;
            r["account"] = account;
            // In production: modules().logos_execution_zone.get_balance(account, true)
            // For now return placeholder
            r["balance"] = "0";
            r["note"] = "Connect logos_execution_zone module for live balance";
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "chain.check_spend", "chain",
        "Check if a proposed spend is within thresholds.",
        "{\"amount_le16\":\"00e87648170000000000000000000000\"}", "{\"allowed\":true}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string amount = args.value("amount_le16", "");
            return spendingGate().checkSpend(amount);
        }
    );

    g_registry.registerSkill(
        "chain.transfer", "chain",
        "Transfer funds with spending gate enforcement.",
        "{\"from_hex\":\"...\",\"to_hex\":\"...\",\"amount_le16\":\"...\"}", "{\"tx_hash\":\"...\"}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string from = args.value("from_hex", agentConfig().wallet_account_hex);
            std::string to = args.value("to_hex", "");
            std::string amount = args.value("amount_le16", "");

            // Check spending gate first
            auto gate_result = nlohmann::json::parse(spendingGate().checkSpend(amount));
            if (!gate_result.value("allowed", false)) {
                nlohmann::json r;
                r["success"] = false;
                r["blocked_by"] = "spending_gate";
                r["gate_result"] = gate_result;
                return r.dump();
            }

            // Execute transfer via LEZ module (runtime call)
            // modules().logos_execution_zone.transfer_public(from, to, amount)
            spendingGate().recordSpend(amount);

            nlohmann::json r;
            r["success"] = true;
            r["from"] = from;
            r["to"] = to;
            r["amount"] = amount;
            r["recorded"] = true;
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "chain.history", "chain",
        "Get spending history for the current period.",
        "{}", "{\"records\":[...],\"period_total\":\"...\"}",
        [](const std::string&) -> std::string {
            return spendingGate().getHistory();
        }
    );

    g_registry.registerSkill(
        "chain.thresholds", "chain",
        "Get current spending thresholds.",
        "{}", "{\"per_tx_limit\":\"...\",\"per_period_limit\":\"...\"}",
        [](const std::string&) -> std::string {
            return spendingGate().getThresholds();
        }
    );

    // ================================================================
    // MESSAGING SKILLS
    // ================================================================
    g_registry.registerSkill(
        "messaging.send", "messaging",
        "Send a message on a content topic via delivery_module.",
        "{\"topic\":\"/logos/agents/v1/discovery\",\"payload\":\"hello\"}", "{\"sent\":true}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string topic = args.value("topic", "");
            std::string payload = args.value("payload", "");
            // In production: modules().delivery_module.send(topic, payload)
            nlohmann::json r;
            r["sent"] = true;
            r["topic"] = topic;
            r["payload_size"] = payload.size();
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "messaging.subscribe", "messaging",
        "Subscribe to a content topic via delivery_module.",
        "{\"topic\":\"/logos/agents/v1/discovery\"}", "{\"subscribed\":true}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string topic = args.value("topic", "");
            // In production: modules().delivery_module.subscribe(topic)
            nlohmann::json r;
            r["subscribed"] = true;
            r["topic"] = topic;
            return r.dump();
        }
    );

    // ================================================================
    // A2A SKILLS
    // ================================================================
    g_registry.registerSkill(
        "a2a.agent_card", "a2a",
        "Get this agent's A2A Agent Card for discovery.",
        "{}", "{...}",
        [](const std::string&) -> std::string {
            auto& cfg = agentConfig();
            nlohmann::json card;
            card["protocol"] = "a2a";
            card["version"] = "1.0";
            card["agent_id"] = cfg.agent_id;
            card["name"] = cfg.agent_name;
            card["description"] = cfg.description;
            card["capabilities"] = nlohmann::json::parse(g_registry.listSkills());
            card["status"] = "active";
            return card.dump();
        }
    );

    g_registry.registerSkill(
        "a2a.discover", "a2a",
        "Discover other agents on the network.",
        "{}", "{\"agents\":[...]}",
        [](const std::string&) -> std::string {
            // In production: subscribe to discovery topic, collect agent cards
            nlohmann::json r;
            r["agents"] = nlohmann::json::array();
            r["note"] = "Discovery requires delivery_module for network messaging";
            return r.dump();
        }
    );

    g_registry.registerSkill(
        "a2a.delegate", "a2a",
        "Delegate a task to another agent.",
        "{\"target_agent_id\":\"agent-2\",\"skill\":\"storage.store\",\"args\":{...}}",
        "{\"delegated\":true}",
        [](const std::string& args_json) -> std::string {
            auto args = nlohmann::json::parse(args_json);
            std::string target = args.value("target_agent_id", "");
            std::string skill = args.value("skill", "");
            // In production: send task via messaging to target agent
            nlohmann::json r;
            r["delegated"] = true;
            r["target"] = target;
            r["skill"] = skill;
            r["status"] = "pending";
            return r.dump();
        }
    );
}

// ============================================================
// Public method implementations (direct API, not via dispatch)
// ============================================================

std::string AgentModuleImpl::dispatchSkill(const std::string& skill_name, const std::string& args_json)
{
    return g_registry.dispatch(skill_name, args_json);
}

// --- Meta ---
std::string AgentModuleImpl::getSkills()
{
    return g_registry.listSkills();
}

std::string AgentModuleImpl::getAgentStatus()
{
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - g_start_time).count();
    nlohmann::json r;
    r["status"] = "active";
    r["uptime_seconds"] = uptime;
    r["skill_count"] = g_registry.count();
    r["config"] = nlohmann::json::parse(agentConfig().toJson());
    return r.dump();
}

std::string AgentModuleImpl::configure(const std::string& key, const std::string& value)
{
    bool ok = agentConfig().set(key, value);
    nlohmann::json r;
    r["key"] = key;
    r["value"] = value;
    r["updated"] = ok;
    return r.dump();
}

std::string AgentModuleImpl::getConfig(const std::string& key)
{
    return agentConfig().get(key);
}

std::string AgentModuleImpl::getFullConfig()
{
    return agentConfig().toJson();
}

// --- Storage ---
std::string AgentModuleImpl::storeData(const std::string& key, const std::string& data)
{
    return g_registry.dispatch("storage.store", nlohmann::json({{"key", key}, {"data", data}}).dump());
}

std::string AgentModuleImpl::retrieveData(const std::string& key)
{
    return g_registry.dispatch("storage.retrieve", nlohmann::json({{"key", key}}).dump());
}

std::string AgentModuleImpl::listStored()
{
    return g_registry.dispatch("storage.list", "{}");
}

// --- Blockchain ---
std::string AgentModuleImpl::getBalance(const std::string& account_hex)
{
    return g_registry.dispatch("chain.balance", nlohmann::json({{"account_hex", account_hex}}).dump());
}

std::string AgentModuleImpl::checkSpend(const std::string& amount_le16)
{
    return spendingGate().checkSpend(amount_le16);
}

std::string AgentModuleImpl::transfer(const std::string& from_hex, const std::string& to_hex, const std::string& amount_le16)
{
    return g_registry.dispatch("chain.transfer",
        nlohmann::json({{"from_hex", from_hex}, {"to_hex", to_hex}, {"amount_le16", amount_le16}}).dump());
}

std::string AgentModuleImpl::getSpendingHistory()
{
    return spendingGate().getHistory();
}

std::string AgentModuleImpl::getThresholds()
{
    return spendingGate().getThresholds();
}

// --- Messaging ---
std::string AgentModuleImpl::sendMessage(const std::string& topic, const std::string& payload)
{
    return g_registry.dispatch("messaging.send",
        nlohmann::json({{"topic", topic}, {"payload", payload}}).dump());
}

std::string AgentModuleImpl::subscribeTopic(const std::string& topic)
{
    return g_registry.dispatch("messaging.subscribe",
        nlohmann::json({{"topic", topic}}).dump());
}

// --- A2A ---
std::string AgentModuleImpl::getAgentCard()
{
    return g_registry.dispatch("a2a.agent_card", "{}");
}

std::string AgentModuleImpl::discoverAgents()
{
    return g_registry.dispatch("a2a.discover", "{}");
}

std::string AgentModuleImpl::delegateTask(const std::string& target_agent_id, const std::string& skill_name, const std::string& args_json)
{
    return g_registry.dispatch("a2a.delegate",
        nlohmann::json({{"target_agent_id", target_agent_id}, {"skill", skill_name}, {"args", nlohmann::json::parse(args_json)}}).dump());
}

// --- Module Info ---
std::string AgentModuleImpl::greet(const std::string& name)
{
    return "Hello, " + name + "! Agent module v0.1.0 with " + std::to_string(g_registry.count()) + " skills.";
}

std::string AgentModuleImpl::getStatus()
{
    return "Agent module is running. Skills: " + std::to_string(g_registry.count());
}
