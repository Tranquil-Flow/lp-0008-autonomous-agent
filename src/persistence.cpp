#include "persistence.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace agent_persistence {

std::string stateDir()
{
    const char* env = std::getenv("AGENT_MODULE_STATE_DIR");
    if (env && *env) return std::string(env);
    return "/tmp/logos-agent-module-state";
}

std::string storagePath()
{
    return stateDir() + "/storage.json";
}

std::string spendHistoryPath()
{
    return stateDir() + "/spend_history.json";
}

nlohmann::json loadJsonFile(const std::string& path, const nlohmann::json& fallback)
{
    std::ifstream in(path);
    if (!in.good()) return fallback;

    nlohmann::json parsed = nlohmann::json::parse(in, nullptr, false);
    if (parsed.is_discarded()) return fallback;
    return parsed;
}

bool saveJsonFile(const std::string& path, const nlohmann::json& value, std::string* error)
{
    try {
        fs::create_directories(fs::path(path).parent_path());
        const std::string tmp = path + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out.good()) {
                if (error) *error = "failed_to_open_temp_file";
                return false;
            }
            out << value.dump(2) << "\n";
        }
        fs::rename(tmp, path);
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

nlohmann::json loadStorage()
{
    nlohmann::json fallback;
    fallback["entries"] = nlohmann::json::object();
    return loadJsonFile(storagePath(), fallback);
}

bool saveStorage(const nlohmann::json& storage, std::string* error)
{
    return saveJsonFile(storagePath(), storage, error);
}

void resetDemoState()
{
    std::error_code ec;
    fs::remove(storagePath(), ec);
    fs::remove(spendHistoryPath(), ec);
}

} // namespace agent_persistence
