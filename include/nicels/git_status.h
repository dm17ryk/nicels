#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace nicels {

class GitStatusCache {
public:
    explicit GitStatusCache(bool enabled);

    std::string status_for(const std::filesystem::path& path);

private:
    struct Repository {
        std::filesystem::path root;
        std::unordered_map<std::string, std::string> entries;
        bool loaded = false;
    };

    std::optional<std::filesystem::path> find_repository_root(std::filesystem::path path) const;
    Repository& load_repository(const std::filesystem::path& root);
    void refresh_repository(Repository& repo);

    bool enabled_;
    std::unordered_map<std::filesystem::path, Repository> cache_;
};

} // namespace nicels
