#include "resources.h"

#include <cstdlib>
#include <filesystem>

namespace nls {

class ResourceManager::Impl {
public:
    void initPaths(const char* argv0) {
        if (initialized_) {
            return;
        }
        initialized_ = true;

        directories_.clear();
        user_config_dir_.clear();
        env_override_dir_.clear();

        if (const char* env = std::getenv("NLS_DATA_DIR")) {
            if (env[0] != '\0') {
                auto normalized = normalize(Path(env));
                addNormalizedDir(normalized);
                env_override_dir_ = std::move(normalized);
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

#ifdef _WIN32
        Path user_dir;
        if (const char* appdata = std::getenv("APPDATA")) {
            if (appdata[0] != '\0') {
                user_dir = Path(appdata) / ".nicels" / "yaml";
            }
        }
        if (user_dir.empty()) {
            if (const char* profile = std::getenv("USERPROFILE")) {
                if (profile[0] != '\0') {
                    user_dir = Path(profile) / ".nicels" / "yaml";
                }
            }
        }
        if (!user_dir.empty()) {
            auto normalized = normalize(user_dir);
            addNormalizedDir(normalized);
            user_config_dir_ = std::move(normalized);
        }
#else
        addDir(Path("/etc/dm17ryk/nicels/yaml"));

        if (const char* home = std::getenv("HOME")) {
            if (home[0] != '\0') {
                Path user_dir = Path(home) / ".nicels" / "yaml";
                auto normalized = normalize(user_dir);
                addNormalizedDir(normalized);
                user_config_dir_ = std::move(normalized);
            }
        }
#endif
    }

    [[nodiscard]] std::filesystem::path find(const std::string& name) const {
        for (const auto& dir : directories_) {
            std::filesystem::path candidate = dir / name;
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
        }
        return {};
    }

    void addDir(const Path& dir) {
        addNormalizedDir(normalize(dir));
    }

    Path normalize(const Path& dir) const {
        if (dir.empty()) return {};
        std::error_code ec;
        Path normalized = std::filesystem::weakly_canonical(dir, ec);
        if (ec) {
            normalized = dir.lexically_normal();
        }
        return normalized;
    }

    void addNormalizedDir(Path normalized) {
        if (normalized.empty()) return;

        for (const auto& existing : directories_) {
            if (existing == normalized) return;
        }
        directories_.push_back(std::move(normalized));
    }

    Path userConfigDir() const { return user_config_dir_; }
    Path envOverrideDir() const { return env_override_dir_; }

private:
    std::vector<Path> directories_{};
    bool initialized_ = false;
    Path user_config_dir_{};
    Path env_override_dir_{};
};

ResourceManager::Impl& ResourceManager::instance() {
    static Impl impl;
    return impl;
}

void ResourceManager::initPaths(const char* argv0) {
    instance().initPaths(argv0);
}

std::filesystem::path ResourceManager::find(const std::string& name) {
    return instance().find(name);
}

void ResourceManager::addDir(const Path& dir) {
    instance().addDir(dir);
}

std::filesystem::path ResourceManager::userConfigDir() {
    return instance().userConfigDir();
}

std::filesystem::path ResourceManager::envOverrideDir() {
    return instance().envOverrideDir();
}

} // namespace nls
