#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace nicels::string_utils {

inline std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline bool equals_ignore_case(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

inline bool is_hidden(std::string_view name) {
    return !name.empty() && name.front() == '.';
}

} // namespace nicels::string_utils
