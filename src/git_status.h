#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

namespace nls {

struct GitStatusResult {
    std::unordered_map<std::string, std::set<std::string>> entries;
    std::set<std::string> default_modes;
};

GitStatusResult get_git_status_for_dir(const std::filesystem::path& dir);

} // namespace nls

