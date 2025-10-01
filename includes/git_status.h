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

    const std::set<std::string>* ModesFor(const std::string& rel_path) const;
    std::string FormatPrefixFor(const std::string& rel_path,
                                bool is_dir,
                                bool is_empty_dir,
                                bool no_color) const;

private:
    std::string FormatPrefix(const std::set<std::string>* modes,
                             bool is_dir,
                             bool is_empty_dir,
                             bool no_color) const;
};

class GitStatusImpl;

class GitStatus {
public:
    GitStatus();
    ~GitStatus();

    GitStatus(const GitStatus&) = delete;
    GitStatus& operator=(const GitStatus&) = delete;

    GitStatusResult GetStatus(const std::filesystem::path& dir, bool recursive = false);

private:
    std::unique_ptr<GitStatusImpl> impl_;
};

} // namespace nls

