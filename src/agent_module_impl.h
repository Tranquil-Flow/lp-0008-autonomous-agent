#pragma once

#include <string>
#include "logos_module_context.h"

/**
 * @brief Autonomous AI agent module for Logos Core (LP-0008).
 *
 * A universal Logos module that provides:
 *   - Skill dispatch system (18 skills across 5 categories)
 *   - Spending threshold gate with owner approval
 *   - A2A protocol for multi-agent coordination
 *   - Owner channel for human-agent communication
 *
 * Public methods are the module API, callable from CLI (logoscore -c)
 * and from other modules via modules().agent_module.*.
 */
class AgentModuleImpl
{
public:
    AgentModuleImpl();

    // === Skill Dispatch ===

    /// Dispatch a skill by name. Returns JSON result.
    std::string dispatchSkill(const std::string& skill_name, const std::string& args_json);

    // === Meta Skills ===

    /// List all registered skills as JSON array.
    std::string getSkills();

    /// Returns agent status: health, config, thresholds, uptime.
    std::string getAgentStatus();

    /// Update a config key. Returns JSON confirmation.
    std::string configure(const std::string& key, const std::string& value);

    /// Get a config value by key.
    std::string getConfig(const std::string& key);

    /// Get the full config as JSON.
    std::string getFullConfig();

    // === Module Info ===

    /// Returns a greeting and module version info.
    std::string greet(const std::string& name);

    /// Returns a short status string.
    std::string getStatus();

    // === A2A ===

    /// Returns the agent's A2A Agent Card as JSON.
    std::string getAgentCard();

private:
    void registerMetaSkills();
    bool meta_registered_ = false;
};
