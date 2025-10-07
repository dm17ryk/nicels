#include "yaml_loader.h"

#include "string_utils.h"

#include "perf.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <optional>

namespace nls {
int YamlLoader::HexValue(char ch) noexcept {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

void YamlLoader::AppendUtf8(char32_t codepoint, std::string& out) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::string YamlLoader::DecodeEscapes(std::string_view text) {
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch != '\\' || i + 1 >= text.size()) {
            result.push_back(ch);
            continue;
        }

        char esc = text[i + 1];
        switch (esc) {
        case '\\':
            result.push_back('\\');
            i += 1;
            continue;
        case '"':
            result.push_back('"');
            i += 1;
            continue;
        case '\'':
            result.push_back('\'');
            i += 1;
            continue;
        case 'n':
            result.push_back('\n');
            i += 1;
            continue;
        case 'r':
            result.push_back('\r');
            i += 1;
            continue;
        case 't':
            result.push_back('\t');
            i += 1;
            continue;
        case 'b':
            result.push_back('\b');
            i += 1;
            continue;
        case 'f':
            result.push_back('\f');
            i += 1;
            continue;
        case 'u':
            if (i + 5 < text.size()) {
                char32_t codepoint = 0;
                bool valid = true;
                for (size_t j = 0; j < 4; ++j) {
                    int value = HexValue(text[i + 2 + j]);
                    if (value < 0) {
                        valid = false;
                        break;
                    }
                    codepoint = static_cast<char32_t>((codepoint << 4) | value);
                }
                if (valid) {
                    AppendUtf8(codepoint, result);
                    i += 5;
                    continue;
                }
            }
            break;
        case 'U':
            if (i + 9 < text.size()) {
                char32_t codepoint = 0;
                bool valid = true;
                for (size_t j = 0; j < 8; ++j) {
                    int value = HexValue(text[i + 2 + j]);
                    if (value < 0) {
                        valid = false;
                        break;
                    }
                    codepoint = static_cast<char32_t>((codepoint << 4) | value);
                }
                if (valid) {
                    AppendUtf8(codepoint, result);
                    i += 9;
                    continue;
                }
            }
            break;
        default:
            break;
        }

        result.push_back('\\');
    }

    return result;
}

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
        return DecodeEscapes(std::string_view(value).substr(1, value.size() - 2));
    }
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::unordered_map<std::string, std::string> YamlLoader::LoadSimpleMap(
    const std::filesystem::path& path,
    bool lowercase_keys) {
    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("yaml_loader::load_simple_map");
    }

    std::unordered_map<std::string, std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        return result;
    }

    std::size_t lines_read = 0;
    std::size_t entries_loaded = 0;
    std::string raw;
    while (std::getline(file, raw)) {
        ++lines_read;
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
        ++entries_loaded;
    }
    if (perf_enabled) {
        perf_manager.IncrementCounter("yaml_loader::lines_read", lines_read);
        perf_manager.IncrementCounter("yaml_loader::entries_loaded", entries_loaded);
    }
    return result;
}

} // namespace nls
