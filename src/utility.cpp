#include "nicels/utility.hpp"

#include <cctype>

namespace nicels {

bool wildcard_match(std::string_view pattern, std::string_view text) {
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t match = 0;

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = ++p;
            match = t;
        } else if (star != std::string::npos) {
            p = star;
            t = ++match;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }

    return p == pattern.size();
}

std::string sanitize_control_chars(std::string_view value, bool hide) {
    if (!hide) {
        return std::string{value};
    }
    std::string result;
    result.reserve(value.size());
    for (unsigned char c : value) {
        result.push_back(std::isprint(c) ? static_cast<char>(c) : '?');
    }
    return result;
}

} // namespace nicels
