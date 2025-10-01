#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace nls {

class YamlLoader {
public:
    static std::unordered_map<std::string, std::string> LoadSimpleMap(
        const std::filesystem::path& path,
        bool lowercase_keys = true);

private:
    static std::string StripComments(const std::string& line);
    static std::string Unquote(const std::string& value);
};

} // namespace nls
