#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/// A skill is a named capability the agent can execute on demand.
/// Skills are organized into categories (storage, messaging, chain, a2a, meta).
struct Skill {
    std::string name;
    std::string category;
    std::string description;
    std::string input_schema;   // JSON example of expected input
    std::string output_schema;  // JSON example of expected output
};

/// Handler function type: takes JSON args string, returns JSON result string.
using SkillHandler = std::function<std::string(const std::string& args_json)>;

/// Registry of all skills the agent can dispatch.
/// Skills are registered at construction time by the impl class.
class SkillRegistry {
public:
    /// Register a skill with its handler.
    void registerSkill(const std::string& name,
                       const std::string& category,
                       const std::string& description,
                       const std::string& input_schema,
                       const std::string& output_schema,
                       SkillHandler handler);

    /// Dispatch a skill by name. Returns JSON result or error JSON.
    std::string dispatch(const std::string& skill_name, const std::string& args_json) const;

    /// List all registered skills as JSON array.
    std::string listSkills() const;

    /// Check if a skill is registered.
    bool hasSkill(const std::string& name) const;

    /// Get count of registered skills.
    size_t count() const;

private:
    struct Entry {
        Skill skill;
        SkillHandler handler;
    };
    std::map<std::string, Entry> skills_;
};
