#include "persistence.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
std::string approvalsPath()   { return ensureDir() + "/approvals.json"; }
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
    // Atomic write: write to temp file on the same filesystem, then rename.
    // Prevents corruption if the process crashes mid-write.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream ofs(tmp);
        if (!ofs.is_open()) return;  // fail closed
        ofs << data.dump(2);
        ofs.flush();
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        // rename failed (cross-filesystem?); fall back to direct write
        std::ofstream ofs(path);
        ofs << data.dump(2);
        fs::remove(tmp, ec);
    }
}

std::string validatePath(const std::string& path)
{
    namespace fs2 = std::filesystem;
    std::error_code ec;

    // Reject empty paths
    if (path.empty()) return {};

    // Canonicalize the requested path
    fs2::path resolved;
    if (path[0] == '/' || path[0] == '~') {
        resolved = fs2::absolute(path, ec);
    } else {
        resolved = fs2::absolute(fs2::current_path() / path, ec);
    }
    if (ec) return {};

    // Canonicalize lexically (works even if file doesn't exist yet)
    resolved = resolved.lexically_normal();
    auto resolved_str = resolved.string();

    // Hard-deny known-sensitive system paths
    static const std::vector<std::string> blockedPrefixes = {
        "/etc/", "/var/", "/usr/", "/bin/", "/sbin/", "/boot/", "/dev/",
        "/proc/", "/sys/", "/root/", "/.ssh/", "/.gnupg/",
        "/Library/Keychains/", "/System/"
    };
    for (const auto& prefix : blockedPrefixes) {
        if (resolved_str.find(prefix) != std::string::npos) return {};
    }

    // Allow paths under the agent state directory (trusted root)
    auto sd = stateDir();
    auto sdResolved = fs2::absolute(sd, ec).lexically_normal().string();
    if (resolved_str.rfind(sdResolved, 0) == 0) return resolved_str;

    // Allow paths under the current working directory (controlled by host)
    auto cwd = fs2::current_path().string();
    if (resolved_str.rfind(cwd, 0) == 0) return resolved_str;

    // Allow absolute paths that exist (owner-provided files for upload)
    // but reject relative paths outside state dir / cwd
    if (fs2::exists(resolved, ec) && path[0] == '/') return resolved_str;

    // For non-existent paths (download destinations), allow under home but
    // reject if they look like they're trying to overwrite system files
    const char* home = std::getenv("HOME");
    if (home) {
        auto homeResolved = fs2::absolute(home, ec).lexically_normal().string();
        if (resolved_str.rfind(homeResolved, 0) == 0) return resolved_str;
    }

    return resolved_str;  // permissive for prototype: allow other paths
}

} // namespace agent_persistence
