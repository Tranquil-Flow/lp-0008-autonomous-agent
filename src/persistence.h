#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace agent_persistence {

std::string stateDir();
std::string storagePath();
std::string spendHistoryPath();

nlohmann::json loadJsonFile(const std::string& path, const nlohmann::json& fallback);
bool saveJsonFile(const std::string& path, const nlohmann::json& value, std::string* error = nullptr);

nlohmann::json loadStorage();
bool saveStorage(const nlohmann::json& storage, std::string* error = nullptr);
void resetDemoState();

} // namespace agent_persistence
