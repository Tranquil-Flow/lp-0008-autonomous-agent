#include "agent_module_impl.h"
#include "skill_registry.h"
#include "agent_config.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace {
    SkillRegistry g_registry;
    auto g_start_time = std::chrono::steady_clock::now();
}

AgentModuleImpl::AgentModuleImpl()
{
    if (!meta_registered_) {
        registerMetaSkills();
        meta_registered_ = true;
    }
}

void AgentModuleImpl::registerMetaSkills()
{
    // meta.skills
    g_registry.registerSkill(
        "meta.skills", "meta",
        "List all registered skills the agent can execute",
        "{}",
        "[{\"name\":\"...\",\"category\":\"...\",\"description\":\"...\"}]",
        [](const std::string&) -> std::string {
            nlohmann::json r;
            r["skills"] = nlohmann::json::parse(g_registry.listSkills());
            r["count"] = g_registry.count();
            return r.dump();
        }
    );

    // meta.status
    g_registry.registerSkill(
        "meta.status", "meta",
        "Return agent health, config summary, and uptime",
        "{}",
        "{\"status\":\"...\",\"uptime_seconds\":N,\"config\":{...}}",
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

    // meta.configure
    g_registry.registerSkill(
        "meta.configure", "meta",
        "Update a runtime config key",
        "{\"key\":\"per_tx_limit\",\"value\":\"500\"}",
        "{\"key\":\"...\",\"updated\":true}",
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
}

// === Skill Dispatch ===

std::string AgentModuleImpl::dispatchSkill(const std::string& skill_name, const std::string& args_json)
{
    return g_registry.dispatch(skill_name, args_json);
}

// === Meta Skills ===

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

// === Module Info ===

std::string AgentModuleImpl::greet(const std::string& name)
{
    return "Hello, " + name + "! Agent module v0.1.0 is running.";
}

std::string AgentModuleImpl::getStatus()
{
    return "Agent module is running. Skills: " + std::to_string(g_registry.count());
}

// === A2A ===

std::string AgentModuleImpl::getAgentCard()
{
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
