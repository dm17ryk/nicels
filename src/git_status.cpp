#include "nicels/git_status.hpp"

#include "nicels/logger.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace nicels {

GitStatusCache::GitStatusCache(bool enabled) : enabled_{enabled} {}

std::string GitStatusCache::status_for(const std::filesystem::path& path) {
    if (!enabled_) {
        return {};
    }

    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return {};
    }

    const auto repo_root = find_repo_root(canonical);
    if (repo_root.empty()) {
        return {};
    }

    const auto relative = std::filesystem::relative(canonical, repo_root, ec);
    if (ec) {
        return {};
    }

    std::scoped_lock lock(mutex_);
    auto& repo_cache = cache_[repo_root];
    if (repo_cache.empty()) {
        populate_cache(repo_root, repo_cache);
    }

    const auto rel = relative.generic_string();
    if (const auto it = repo_cache.find(rel); it != repo_cache.end()) {
        std::string status = it->second;
        status.erase(std::remove(status.begin(), status.end(), ' '), status.end());
        return status;
    }
    return {};
}

void GitStatusCache::populate_cache(const std::filesystem::path& repo_root, std::unordered_map<std::string, std::string>& out_cache) {
    const auto output = run_git(repo_root, "status --porcelain=1 -z");
    if (output.empty()) {
        return;
    }
    std::size_t pos = 0;
    while (pos < output.size()) {
        const auto next = output.find('\0', pos);
        if (next == std::string::npos) {
            break;
        }
        const auto token = output.substr(pos, next - pos);
        if (token.size() >= 3) {
            const auto status = token.substr(0, 2);
            auto path = token.substr(3);
            if (const auto arrow = path.find(" -> "); arrow != std::string::npos) {
                path = path.substr(arrow + 4);
            }
            out_cache[path] = status;
        }
        pos = next + 1;
    }
}

std::filesystem::path GitStatusCache::find_repo_root(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return {};
    }
    auto current = std::filesystem::is_directory(path, ec) ? path : path.parent_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / ".git", ec)) {
            return current;
        }
        if (auto parent = current.parent_path(); parent == current) {
            break;
        } else {
            current = parent;
        }
    }
    // fallback to git command
    const auto command_cwd = std::filesystem::is_directory(path, ec) ? path : path.parent_path();
    const auto output = run_git(command_cwd, "rev-parse --show-toplevel");
    if (!output.empty()) {
        return std::filesystem::path(output);
    }
    return {};
}

std::string GitStatusCache::run_git(const std::filesystem::path& cwd, const std::string& args) {
    std::filesystem::path actual_cwd = cwd.empty() ? std::filesystem::current_path() : cwd;
    std::string command = "git -C \"" + actual_cwd.string() + "\" " + args;
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        return {};
    }
    std::string output;
    std::array<char, 256> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output.append(buffer.data());
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    // trim trailing newlines
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    return output;
}

} // namespace nicels
