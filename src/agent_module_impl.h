#pragma once

#include <string>

/**
 * @brief Autonomous AI agent module for Logos Core (LP-0008).
 *
 * Skills across 5 categories:
 *   meta     — introspection, config, status
 *   storage  — file store/retrieve via storage_module
 *   chain    — wallet operations via logos_execution_zone
 *   messaging— send/receive via delivery_module
 *   a2a      — agent-to-agent protocol
 */
class AgentModuleImpl
{
public:
    AgentModuleImpl();

    // === Skill Dispatch ===
    std::string dispatchSkill(const std::string& skill_name, const std::string& args_json);

    // === Meta Skills (direct) ===
    std::string getSkills();
    std::string getAgentStatus();
    std::string configure(const std::string& key, const std::string& value);
    std::string getConfig(const std::string& key);
    std::string getFullConfig();

    // === Storage Skills ===
    std::string storeData(const std::string& key, const std::string& data);
    std::string retrieveData(const std::string& key);
    std::string listStored();

    // === Blockchain Skills ===
    std::string getBalance(const std::string& account_hex);
    std::string checkSpend(const std::string& amount_le16);
    std::string transfer(const std::string& from_hex, const std::string& to_hex, const std::string& amount_le16);
    std::string getSpendingHistory();
    std::string getThresholds();

    // === Messaging Skills ===
    std::string sendMessage(const std::string& topic, const std::string& payload);
    std::string subscribeTopic(const std::string& topic);

    // === A2A Skills ===
    std::string getAgentCard();
    std::string discoverAgents();
    std::string delegateTask(const std::string& target_agent_id, const std::string& skill_name, const std::string& args_json);

    // === Module Info ===
    std::string greet(const std::string& name);
    std::string getStatus();

private:
    void registerAllSkills();
    bool skills_registered_ = false;
};
