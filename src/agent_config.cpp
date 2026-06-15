#include "agent_config.h"
#include <nlohmann/json.hpp>
#include "persistence.h"

AgentConfig& agentConfig()
{
    static AgentConfig cfg;
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
    if (key == "heartbeat_interval") return std::to_string(heartbeat_interval);
    return "";
}

void AgentConfig::save() const
{
    agent_persistence::saveJsonFile(agent_persistence::configPath(),
                                    nlohmann::json::parse(toJson()));
}
