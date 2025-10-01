#pragma once

#include <string>
#include <string_view>

namespace nls {

class StringUtils {
public:
    static constexpr bool IsHidden(std::string_view name) noexcept
    {
        return !name.empty() && name.front() == '.';
    }

    static constexpr bool EqualsIgnoreCase(char a, char b) noexcept
    {
        return ascii_to_lower(static_cast<unsigned char>(a)) ==
               ascii_to_lower(static_cast<unsigned char>(b));
    }

    static std::string ToLower(std::string_view value);
    static std::string Trim(std::string_view value);

private:
    static constexpr unsigned char ascii_to_lower(unsigned char ch) noexcept
    {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<unsigned char>(ch - 'A' + 'a') : ch;
    }
};

} // namespace nls
