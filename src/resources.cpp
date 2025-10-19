#include "resources.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <system_error>

#include "perf.h"

namespace nls {
namespace {
constexpr const char* kDatabaseFilename = "NLS.sqlite3";
}

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

        auto register_directory = [&](const Path& dir) {
            if (dir.empty()) return;
            auto normalized = normalize(dir);
            addNormalizedDir(std::move(normalized));
        };

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
            register_directory(cwd / "DB");
            register_directory(cwd / "yaml"); // compatibility with legacy layouts
            register_directory(cwd);
        }

        Path exe_dir;
        if (argv0 && argv0[0] != '\0') {
            Path exe_path(argv0);
            if (!exe_path.is_absolute()) {
                if (!cwd.empty()) exe_path = cwd / exe_path;
            }
            exe_path = std::filesystem::weakly_canonical(exe_path, ec);
            if (ec) {
                exe_path = exe_path.lexically_normal();
            }
            exe_dir = exe_path.parent_path();
            if (!exe_dir.empty()) {
                register_directory(exe_dir / "DB");
                register_directory(exe_dir / "yaml");
                register_directory(exe_dir);
                Path exe_parent = exe_dir.parent_path();
                if (!exe_parent.empty()) {
                    register_directory(exe_parent / "DB");
                    register_directory(exe_parent / "yaml");
                    register_directory(exe_parent);
                }
            }
        }

#ifdef _WIN32
        Path primary_user_dir;
        if (const char* appdata = std::getenv("APPDATA")) {
            if (appdata[0] != '\0') {
                Path base(appdata);
                primary_user_dir = base / "nicels" / "DB";
                register_directory(primary_user_dir);
                register_directory(base / "nicels");
                register_directory(base / ".nicels" / "yaml"); // legacy
            }
        }
        if (primary_user_dir.empty()) {
            if (const char* profile = std::getenv("USERPROFILE")) {
                if (profile[0] != '\0') {
                    Path base(profile);
                    primary_user_dir = base / "nicels" / "DB";
                    register_directory(primary_user_dir);
                    register_directory(base / "nicels");
                    register_directory(base / ".nicels" / "yaml"); // legacy
                }
            }
        }
        if (!primary_user_dir.empty()) {
            auto normalized = normalize(primary_user_dir);
            addNormalizedDir(normalized);
            user_config_dir_ = std::move(normalized);
        }
#else
        register_directory(Path("/etc/dm17ryk/nicels/DB"));
        register_directory(Path("/etc/dm17ryk/nicels"));
        register_directory(Path("/etc/dm17ryk/nicels/yaml")); // legacy

        if (const char* home = std::getenv("HOME")) {
            if (home[0] != '\0') {
                Path base(home);
                Path user_dir = base / ".nicels" / "DB";
                register_directory(user_dir);
                register_directory(base / ".nicels");
                register_directory(base / ".nicels" / "yaml"); // legacy
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

        if (name.empty() || name == kDatabaseFilename) {
            Path database = findDatabase();
            if (!database.empty()) {
                if (perf_enabled) {
                    perf_manager.IncrementCounter("resources::find_hits");
                }
                return database;
            }
            if (perf_enabled) {
                perf_manager.IncrementCounter("resources::find_misses");
            }
            return {};
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

    [[nodiscard]] std::vector<Path> databaseCandidates() const {
        std::vector<Path> candidates;
        auto add_candidate = [&](const Path& dir, bool allow_missing) {
            if (dir.empty()) return;
            std::filesystem::path candidate = dir / kDatabaseFilename;
            std::error_code ec;
            bool exists = std::filesystem::exists(candidate, ec);
            if (!exists || ec) {
                if (!allow_missing) return;
            }
            if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
                candidates.push_back(std::move(candidate));
            }
        };

        if (!env_override_dir_.empty()) {
            add_candidate(env_override_dir_, true);
        }

        if (!user_config_dir_.empty()) {
            add_candidate(user_config_dir_, false);
        }

        for (const auto& dir : directories_) {
            if (!env_override_dir_.empty() && dir == env_override_dir_) {
                continue;
            }
            if (!user_config_dir_.empty() && dir == user_config_dir_) {
                continue;
            }
            add_candidate(dir, false);
        }

        return candidates;
    }

    [[nodiscard]] Path findDatabase() const {
        auto candidates = databaseCandidates();
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) && !ec) {
                bool is_file = std::filesystem::is_regular_file(candidate, ec);
                if (!ec && is_file) {
                    return candidate;
                }
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
        auto& perf_manager = perf::Manager::Instance();
        if (perf_manager.enabled()) {
            perf_manager.IncrementCounter("resources::directories_tracked");
        }
    }

    Path userConfigDir() const { return user_config_dir_; }
    Path envOverrideDir() const { return env_override_dir_; }
    Path defaultConfigDir() const
    {
        if (auto database = findDatabase(); !database.empty()) {
            return database.parent_path();
        }
        if (!env_override_dir_.empty()) {
            return env_override_dir_;
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

std::filesystem::path ResourceManager::findDatabase() {
    return instance().findDatabase();
}

std::vector<std::filesystem::path> ResourceManager::databaseCandidates() {
    return instance().databaseCandidates();
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

    Path destination = user_dir / kDatabaseFilename;
    bool destination_exists = std::filesystem::exists(destination, ec);
    if (ec) {
        return ec;
    }
    if (destination_exists && !overwrite_existing) {
        result.skipped.push_back(destination);
        if (perf_enabled) {
            perf_manager.IncrementCounter("resources::db_file_skipped");
        }
        return {};
    }

    Path source_file;
    for (const auto& candidate : impl.databaseCandidates()) {
        std::error_code candidate_ec;
        if (candidate.empty()) continue;
        if (!std::filesystem::exists(candidate, candidate_ec) || candidate_ec) {
            continue;
        }
        if (candidate.parent_path() == user_dir) {
            continue;
        }
        source_file = candidate;
        break;
    }
    if (source_file.empty()) {
        for (const auto& candidate : impl.databaseCandidates()) {
            std::error_code candidate_ec;
            if (candidate.empty()) continue;
            if (!std::filesystem::exists(candidate, candidate_ec) || candidate_ec) {
                continue;
            }
            source_file = candidate;
            break;
        }
    }

    if (source_file.empty()) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }

    bool same_file = false;
    if (destination_exists) {
        same_file = std::filesystem::equivalent(source_file, destination, ec);
        if (ec) {
            return ec;
        }
    }
    if (same_file) {
        result.skipped.push_back(destination);
        if (perf_enabled) {
            perf_manager.IncrementCounter("resources::db_file_skipped");
        }
        return {};
    }

    std::filesystem::copy_options options = overwrite_existing
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;
    std::filesystem::copy_file(source_file, destination, options, ec);
    if (ec) {
        return ec;
    }

    result.copied.push_back(destination);
    if (perf_enabled) {
        perf_manager.IncrementCounter("resources::db_file_copied");
    }

    return {};
}

} // namespace nls
