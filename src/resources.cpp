#include "resources.h"

#include <cstdlib>
#include <vector>

namespace nls {
namespace {

using Path = std::filesystem::path;

std::vector<Path>& resource_dirs() {
    static std::vector<Path> dirs;
    return dirs;
}

bool& resources_initialized() {
    static bool initialized = false;
    return initialized;
}

void add_dir(const Path& dir) {
    if (dir.empty()) return;
    Path normalized;
    std::error_code ec;
    normalized = std::filesystem::weakly_canonical(dir, ec);
    if (ec) {
        normalized = dir.lexically_normal();
    }
    if (normalized.empty()) return;
    auto& dirs = resource_dirs();
    for (const auto& existing : dirs) {
        if (existing == normalized) return;
    }
    dirs.push_back(std::move(normalized));
}

} // namespace

void init_resource_paths(const char* argv0) {
    if (resources_initialized()) return;
    resources_initialized() = true;

    if (const char* env = std::getenv("NLS_DATA_DIR")) {
        if (env[0] != '\0') {
            add_dir(Path(env));
        }
    }

    std::error_code ec;
    Path cwd = std::filesystem::current_path(ec);
    if (!ec) {
        add_dir(cwd / "yaml");
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
            add_dir(exe_dir / "yaml");
            add_dir(exe_dir.parent_path() / "yaml");
        }
    }
}

std::filesystem::path find_resource(const std::string& name) {
    auto& dirs = resource_dirs();
    for (const auto& dir : dirs) {
        std::filesystem::path candidate = dir / name;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

} // namespace nls
