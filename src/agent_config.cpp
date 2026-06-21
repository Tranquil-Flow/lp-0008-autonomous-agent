#include "agent_config.h"
#include <nlohmann/json.hpp>
#include "persistence.h"

namespace {

void loadConfigFromDisk(AgentConfig& cfg)
{
    auto j = agent_persistence::loadJsonFile(agent_persistence::configPath());
    if (!j.is_object()) return;

    if (j.contains("agent_id")) cfg.agent_id = j.value("agent_id", cfg.agent_id);
    if (j.contains("agent_name")) cfg.agent_name = j.value("agent_name", cfg.agent_name);
    if (j.contains("description")) cfg.description = j.value("description", cfg.description);
    if (j.contains("per_tx_limit")) cfg.per_tx_limit = j.value("per_tx_limit", cfg.per_tx_limit);
    if (j.contains("per_period_limit")) cfg.per_period_limit = j.value("per_period_limit", cfg.per_period_limit);
    if (j.contains("period_seconds")) cfg.period_seconds = j.value("period_seconds", cfg.period_seconds);
    if (j.contains("discovery_topic")) cfg.discovery_topic = j.value("discovery_topic", cfg.discovery_topic);
    if (j.contains("owner_topic")) cfg.owner_topic = j.value("owner_topic", cfg.owner_topic);
    if (j.contains("task_topic")) cfg.task_topic = j.value("task_topic", cfg.task_topic);
    if (j.contains("wallet_account_hex")) cfg.wallet_account_hex = j.value("wallet_account_hex", cfg.wallet_account_hex);
    if (j.contains("sequencer_addr")) cfg.sequencer_addr = j.value("sequencer_addr", cfg.sequencer_addr);
    if (j.contains("a2a_payment_recipient")) cfg.a2a_payment_recipient = j.value("a2a_payment_recipient", cfg.a2a_payment_recipient);
    if (j.contains("a2a_payment_amount_le16")) cfg.a2a_payment_amount_le16 = j.value("a2a_payment_amount_le16", cfg.a2a_payment_amount_le16);
    if (j.contains("heartbeat_interval")) cfg.heartbeat_interval = j.value("heartbeat_interval", cfg.heartbeat_interval);
    if (j.contains("enabled_skills") && j["enabled_skills"].is_array()) {
        cfg.enabled_skills = j["enabled_skills"].get<std::vector<std::string>>();
    }
}

} // namespace

AgentConfig& agentConfig()
{
    static AgentConfig cfg;
    static bool loaded = false;
    if (!loaded) {
        loadConfigFromDisk(cfg);
        loaded = true;
    }
    return cfg;
}

std::string AgentConfig::toJson() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["agent_id"] = agent_id;
    j["agent_name"] = agent_name;
    j["description"] = description;
    j["per_tx_limit"] = per_tx_limit;
    j["per_period_limit"] = per_period_limit;
    j["period_seconds"] = period_seconds;
    j["discovery_topic"] = discovery_topic;
    j["owner_topic"] = owner_topic;
    j["task_topic"] = task_topic;
    j["wallet_account_hex"] = wallet_account_hex;
    j["sequencer_addr"] = sequencer_addr;
    j["a2a_payment_recipient"] = a2a_payment_recipient;
    j["a2a_payment_amount_le16"] = a2a_payment_amount_le16;
    j["heartbeat_interval"] = heartbeat_interval;
    j["enabled_skills"] = enabled_skills;
    return j.dump();
}

bool AgentConfig::set(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (key == "agent_id")         { agent_id = value; return true; }
    if (key == "agent_name")       { agent_name = value; return true; }
    if (key == "description")      { description = value; return true; }
    if (key == "per_tx_limit")     { per_tx_limit = value; return true; }
    if (key == "per_period_limit") { per_period_limit = value; return true; }
    if (key == "period_seconds")   { period_seconds = value; return true; }
    if (key == "discovery_topic")  { discovery_topic = value; return true; }
    if (key == "owner_topic")      { owner_topic = value; return true; }
    if (key == "task_topic")       { task_topic = value; return true; }
    if (key == "wallet_account_hex"){ wallet_account_hex = value; return true; }
    if (key == "sequencer_addr")    { sequencer_addr = value; return true; }
    if (key == "a2a_payment_recipient") { a2a_payment_recipient = value; return true; }
    if (key == "a2a_payment_amount_le16") { a2a_payment_amount_le16 = value; return true; }
    if (key == "heartbeat_interval"){ try { heartbeat_interval = std::stoi(value); return true; } catch (...) { return false; } }
    return false;
}

std::string AgentConfig::get(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (key == "agent_id")         return agent_id;
    if (key == "agent_name")       return agent_name;
    if (key == "description")      return description;
    if (key == "per_tx_limit")     return per_tx_limit;
    if (key == "per_period_limit") return per_period_limit;
    if (key == "period_seconds")   return period_seconds;
    if (key == "discovery_topic")  return discovery_topic;
    if (key == "owner_topic")      return owner_topic;
    if (key == "task_topic")       return task_topic;
    if (key == "wallet_account_hex") return wallet_account_hex;
    if (key == "sequencer_addr")     return sequencer_addr;
    if (key == "a2a_payment_recipient") return a2a_payment_recipient;
    if (key == "a2a_payment_amount_le16") return a2a_payment_amount_le16;
    if (key == "heartbeat_interval") return std::to_string(heartbeat_interval);
    return "";
}

void AgentConfig::save() const
{
    agent_persistence::saveJsonFile(agent_persistence::configPath(),
                                    nlohmann::json::parse(toJson()));
}
