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
    // Initialize resource search paths (idempotent).
    static void initPaths(const char* argv0);

    // Locate a resource file under the configured search paths.
    // Returns empty path if not found.
    static std::filesystem::path find(const std::string& name);

private:
    using Path = std::filesystem::path;

    static void addDir(const Path& dir);

    static std::vector<Path> directories_;
    static bool initialized_;
};

} // namespace nls
