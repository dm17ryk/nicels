#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace nls {

std::unordered_map<std::string, std::string> load_simple_yaml_map(
    const std::filesystem::path& path,
    bool lowercase_keys = true);

} // namespace nls
