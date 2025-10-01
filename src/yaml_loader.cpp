#include "yaml_loader.h"

#include "string_utils.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace nls {
std::string YamlLoader::StripComments(const std::string& line) {
    std::string out;
    out.reserve(line.size());
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            out.push_back(ch);
            continue;
        }
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            out.push_back(ch);
            continue;
        }
        if (ch == '#' && !in_single && !in_double) {
            break;
        }
        out.push_back(ch);
    }
    return out;
}

std::string YamlLoader::Unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::unordered_map<std::string, std::string> YamlLoader::LoadSimpleMap(
    const std::filesystem::path& path,
    bool lowercase_keys) {
    std::unordered_map<std::string, std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        return result;
    }

    std::string raw;
    while (std::getline(file, raw)) {
        std::string line = StripComments(raw);
        std::string trimmed_line = StringUtils::Trim(line);
        if (trimmed_line.empty()) continue;
        auto colon = trimmed_line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = StringUtils::Trim(std::string_view(trimmed_line).substr(0, colon));
        std::string value = StringUtils::Trim(std::string_view(trimmed_line).substr(colon + 1));
        if (key.empty() || value.empty()) continue;
        value = Unquote(value);
        if (lowercase_keys) key = StringUtils::ToLower(key);
        result[std::move(key)] = std::move(value);
    }
    return result;
}

} // namespace nls
