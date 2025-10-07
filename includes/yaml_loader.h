#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <string_view>

namespace nls {

class YamlLoader {
public:
    static std::unordered_map<std::string, std::string> LoadSimpleMap(
        const std::filesystem::path& path,
        bool lowercase_keys = true);

private:
    static std::string StripComments(const std::string& line);
    static std::string Unquote(const std::string& value);
    static std::string DecodeEscapes(std::string_view text);
    static void AppendUtf8(char32_t codepoint, std::string& out);
    static int HexValue(char ch) noexcept;
};

} // namespace nls
