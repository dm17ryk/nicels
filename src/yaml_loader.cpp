#include "yaml_loader.h"

#include "util.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace nls {
namespace {

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string strip_comments(const std::string& line) {
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

std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

} // namespace

std::unordered_map<std::string, std::string> load_simple_yaml_map(const std::filesystem::path& path,
                                                                  bool lowercase_keys) {
    std::unordered_map<std::string, std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        return result;
    }

    std::string raw;
    while (std::getline(file, raw)) {
        std::string line = strip_comments(raw);
        line = trim_copy(line);
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim_copy(line.substr(0, colon));
        std::string value = trim_copy(line.substr(colon + 1));
        if (key.empty() || value.empty()) continue;
        value = unquote(value);
        if (lowercase_keys) key = to_lower(std::move(key));
        result[std::move(key)] = std::move(value);
    }
    return result;
}

} // namespace nls
