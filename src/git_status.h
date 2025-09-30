#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace nls {

struct GitStatusResult {
    std::unordered_map<std::string, std::set<std::string>> entries;
    std::set<std::string> default_modes;
    bool repository_found = false;
};

class GitStatusImpl;

class GitStatus {
public:
    GitStatus();
    ~GitStatus();

    GitStatus(const GitStatus&) = delete;
    GitStatus& operator=(const GitStatus&) = delete;

    GitStatusResult GetStatus(const std::filesystem::path& dir);

private:
    std::unique_ptr<GitStatusImpl> impl_;
};

} // namespace nls

