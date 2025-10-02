#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nls {

// ResourceManager encapsulates the search path management for YAML resources.
// Call initPaths() early during application startup (App::run does this) so
// that subsequent calls to find() can locate bundled or user-provided files.
class ResourceManager {
public:
    static void initPaths(const char* argv0);
    static std::filesystem::path find(const std::string& name);
    static std::filesystem::path userConfigDir();
    static std::filesystem::path envOverrideDir();

private:
    using Path = std::filesystem::path;

    class Impl;

    static Impl& instance();
    static void addDir(const Path& dir);
};

} // namespace nls
