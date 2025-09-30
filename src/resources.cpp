#include "resources.h"

#include <cstdlib>

namespace nls {

std::vector<ResourceManager::Path> ResourceManager::directories_{};
bool ResourceManager::initialized_ = false;

void ResourceManager::initPaths(const char* argv0) {
    if (initialized_) return;
    initialized_ = true;

    directories_.clear();

    if (const char* env = std::getenv("NLS_DATA_DIR")) {
        if (env[0] != '\0') {
            addDir(Path(env));
        }
    }

    std::error_code ec;
    Path cwd = std::filesystem::current_path(ec);
    if (!ec) {
        addDir(cwd / "yaml");
    }

    if (argv0 && argv0[0] != '\0') {
        Path exe_path(argv0);
        if (!exe_path.is_absolute()) {
            if (!cwd.empty()) exe_path = cwd / exe_path;
        }
        exe_path = std::filesystem::weakly_canonical(exe_path, ec);
        if (ec) {
            exe_path = exe_path.lexically_normal();
        }
        Path exe_dir = exe_path.parent_path();
        if (!exe_dir.empty()) {
            addDir(exe_dir / "yaml");
            addDir(exe_dir.parent_path() / "yaml");
        }
    }
}

std::filesystem::path ResourceManager::find(const std::string& name) {
    for (const auto& dir : directories_) {
        std::filesystem::path candidate = dir / name;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void ResourceManager::addDir(const Path& dir) {
    if (dir.empty()) return;
    Path normalized;
    std::error_code ec;
    normalized = std::filesystem::weakly_canonical(dir, ec);
    if (ec) {
        normalized = dir.lexically_normal();
    }
    if (normalized.empty()) return;

    for (const auto& existing : directories_) {
        if (existing == normalized) return;
    }
    directories_.push_back(std::move(normalized));
}

} // namespace nls
