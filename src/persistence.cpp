#include "persistence.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace agent_persistence {

static std::string g_overrideDir;

void overrideStateDir(const std::string& dir)
{
    g_overrideDir = dir;
}

std::string stateDir()
{
    if (!g_overrideDir.empty()) return g_overrideDir;
    const char* env = std::getenv("AGENT_MODULE_STATE_DIR");
    if (env && *env) return env;
    return fs::current_path().string() + "/.agent-state";
}

static std::string ensureDir()
{
    auto dir = stateDir();
    fs::create_directories(dir);
    return dir;
}

std::string storagePath()     { return ensureDir() + "/storage.json"; }
std::string spendHistoryPath(){ return ensureDir() + "/spend_history.json"; }
std::string walletPath()      { return ensureDir() + "/wallet.json"; }
std::string messagesPath()    { return ensureDir() + "/messages.json"; }
std::string groupsPath()      { return ensureDir() + "/groups.json"; }
std::string discoveryPath()   { return ensureDir() + "/discovery.json"; }
std::string tasksPath()       { return ensureDir() + "/tasks.json"; }
std::string configPath()      { return ensureDir() + "/config.json"; }

nlohmann::json loadJsonFile(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return nlohmann::json{};
    try {
        nlohmann::json j;
        ifs >> j;
        return j;
    } catch (...) {
        return nlohmann::json{};
    }
}

void saveJsonFile(const std::string& path, const nlohmann::json& data)
{
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream ofs(path);
    ofs << data.dump(2);
}

} // namespace agent_persistence
