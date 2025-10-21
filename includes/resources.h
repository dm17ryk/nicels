#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace nls {

// ResourceManager encapsulates the search path management for configuration
// resources. Call initPaths() early during application startup (App::run does
// this) so that subsequent calls to findDatabase() can locate the bundled or
// user-provided SQLite database.
class ResourceManager {
public:
    static void initPaths(const char* argv0);
    static std::filesystem::path find(const std::string& name);
    static std::filesystem::path findDatabase();
    static std::vector<std::filesystem::path> databaseCandidates();
    static std::filesystem::path userConfigDir();
    static std::filesystem::path envOverrideDir();

private:
    using Path = std::filesystem::path;

    class Impl;

    static Impl& instance();
    static void addDir(const Path& dir);
};

} // namespace nls
