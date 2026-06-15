#include "skill_registry.h"

void SkillRegistry::registerSkill(const std::string& name,
                                   const std::string& category,
                                   const std::string& description,
                                   const std::string& input_schema,
                                   const std::string& output_schema,
                                   SkillHandler handler)
{
    skills_[name] = Entry{
        Skill{name, category, description, input_schema, output_schema},
        std::move(handler)
    };
}

std::string SkillRegistry::dispatch(const std::string& skill_name, const std::string& args_json) const
{
    auto it = skills_.find(skill_name);
    if (it == skills_.end()) {
        nlohmann::json err;
        err["error"] = "unknown_skill";
        err["skill"] = skill_name;
        err["available"] = nlohmann::json::array();
        for (const auto& [k, v] : skills_) {
            err["available"].push_back(k);
        }
        return err.dump();
    }
    try {
        return it->second.handler(args_json);
    } catch (const std::exception& e) {
        nlohmann::json err;
        err["error"] = "skill_failed";
        err["skill"] = skill_name;
        err["message"] = e.what();
        return err.dump();
    }
}

std::string SkillRegistry::listSkills() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [name, entry] : skills_) {
        nlohmann::json obj;
        obj["name"] = entry.skill.name;
        obj["category"] = entry.skill.category;
        obj["description"] = entry.skill.description;
        obj["input"] = nlohmann::json::parse(entry.skill.input_schema.empty() ? "{}" : entry.skill.input_schema);
        obj["output"] = nlohmann::json::parse(entry.skill.output_schema.empty() ? "{}" : entry.skill.output_schema);
        arr.push_back(obj);
    }
    return arr.dump();
}

bool SkillRegistry::hasSkill(const std::string& name) const
{
    return skills_.count(name) > 0;
}

size_t SkillRegistry::count() const
{
    return skills_.size();
}
