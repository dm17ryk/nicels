#include "resources.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <system_error>

#include "perf.h"

namespace nls {

class ResourceManager::Impl {
public:
    void initPaths(const char* argv0) {
        if (initialized_) {
            return;
        }
        auto& perf_manager = perf::Manager::Instance();
        const bool perf_enabled = perf_manager.enabled();
        std::optional<perf::Timer> timer;
        if (perf_enabled) {
            timer.emplace("resources::init_paths");
            perf_manager.IncrementCounter("resources::init_paths_calls");
        }
        initialized_ = true;

        directories_.clear();
        user_config_dir_.clear();
        env_override_dir_.clear();

        const auto initial_directories = directories_.size();

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
        if (perf_enabled) {
            perf_manager.IncrementCounter("resources::directories_registered",
                                          directories_.size() - initial_directories);
            if (!user_config_dir_.empty()) {
                perf_manager.IncrementCounter("resources::user_config_available");
            }
            if (!env_override_dir_.empty()) {
                perf_manager.IncrementCounter("resources::env_override_available");
            }
        }
    }

    [[nodiscard]] std::filesystem::path find(const std::string& name) const {
        auto& perf_manager = perf::Manager::Instance();
        const bool perf_enabled = perf_manager.enabled();
        std::optional<perf::Timer> timer;
        if (perf_enabled) {
            timer.emplace("resources::find");
            perf_manager.IncrementCounter("resources::find_calls");
        }
        for (const auto& dir : directories_) {
            std::filesystem::path candidate = dir / name;
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                if (perf_enabled) {
                    perf_manager.IncrementCounter("resources::find_hits");
                }
                return candidate;
            }
        }
        if (perf_enabled) {
            perf_manager.IncrementCounter("resources::find_misses");
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
        auto& perf_manager = perf::Manager::Instance();
        if (perf_manager.enabled()) {
            perf_manager.IncrementCounter("resources::directories_tracked");
        }
    }

    Path userConfigDir() const { return user_config_dir_; }
    Path envOverrideDir() const { return env_override_dir_; }
    Path defaultConfigDir() const
    {
        if (!env_override_dir_.empty()) {
            return env_override_dir_;
        }
        auto colors = find("colors.yaml");
        if (!colors.empty()) {
            return colors.parent_path();
        }
        return {};
    }

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

std::error_code ResourceManager::copyDefaultsToUserConfig(CopyResult& result, bool overwrite_existing)
{
    result.copied.clear();
    result.skipped.clear();

    Impl& impl = instance();

    Path user_dir = impl.userConfigDir();
    if (user_dir.empty()) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }

    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("resources::copy_defaults_to_user_config");
        perf_manager.IncrementCounter("resources::copy_defaults_calls");
    }

    std::error_code ec;
    bool user_exists = std::filesystem::exists(user_dir, ec);
    if (ec) {
        return ec;
    }
    if (!user_exists) {
        std::filesystem::create_directories(user_dir, ec);
        if (ec) {
            return ec;
        }
    } else {
        bool is_dir = std::filesystem::is_directory(user_dir, ec);
        if (ec) {
            return ec;
        }
        if (!is_dir) {
            return std::make_error_code(std::errc::not_a_directory);
        }
    }

    Path source_dir = impl.defaultConfigDir();
    if (source_dir.empty()) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }

    std::filesystem::directory_iterator iter(source_dir, ec);
    if (ec) {
        return ec;
    }
    const std::filesystem::directory_iterator end;
    for (; iter != end; iter.increment(ec)) {
        if (ec) {
            return ec;
        }
        const auto& entry = *iter;
        bool is_regular = entry.is_regular_file(ec);
        if (ec) {
            return ec;
        }
        if (!is_regular) {
            continue;
        }

        if (entry.path().extension() != ".yaml") {
            continue;
        }

        Path destination = user_dir / entry.path().filename();
        bool destination_exists = std::filesystem::exists(destination, ec);
        if (ec) {
            return ec;
        }
        if (destination_exists) {
            bool same = std::filesystem::equivalent(entry.path(), destination, ec);
            if (ec) {
                return ec;
            }
            if (same) {
                result.skipped.push_back(destination);
                if (perf_enabled) {
                    perf_manager.IncrementCounter("resources::yaml_files_skipped");
                }
                continue;
            }
            if (!overwrite_existing) {
                result.skipped.push_back(destination);
                if (perf_enabled) {
                    perf_manager.IncrementCounter("resources::yaml_files_skipped");
                }
                continue;
            }
        }

        std::filesystem::copy_options options = overwrite_existing
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
        std::filesystem::copy_file(entry.path(), destination, options, ec);
        if (ec) {
            return ec;
        }

        result.copied.push_back(destination);
        if (perf_enabled) {
            perf_manager.IncrementCounter("resources::yaml_files_copied");
        }
    }

    return {};
}

} // namespace nls
