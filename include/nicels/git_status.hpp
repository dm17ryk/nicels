#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nicels {

class GitStatusCache {
public:
    explicit GitStatusCache(bool enabled);

    [[nodiscard]] std::string status_for(const std::filesystem::path& path);

private:
    bool enabled_{false};
    std::mutex mutex_;
    std::unordered_map<std::filesystem::path, std::unordered_map<std::string, std::string>> cache_;

    void populate_cache(const std::filesystem::path& repo_root, std::unordered_map<std::string, std::string>& out_cache);
    [[nodiscard]] std::filesystem::path find_repo_root(const std::filesystem::path& path);
    [[nodiscard]] static std::string run_git(const std::filesystem::path& cwd, const std::string& args);
};

} // namespace nicels
