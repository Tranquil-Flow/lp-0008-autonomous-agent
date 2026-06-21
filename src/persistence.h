#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace agent_persistence {

// Override the state directory (set by onContextReady from host)
void overrideStateDir(const std::string& dir);

std::string stateDir();

// Per-category persistence paths
std::string storagePath();
std::string spendHistoryPath();
std::string walletPath();
std::string messagesPath();
std::string groupsPath();
std::string discoveryPath();
std::string tasksPath();
std::string approvalsPath();
std::string configPath();

// JSON file helpers
nlohmann::json loadJsonFile(const std::string& path);
void saveJsonFile(const std::string& path, const nlohmann::json& data);

} // namespace agent_persistence
