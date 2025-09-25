#include "colors.h"

#include "resources.h"
#include "util.h"
#include "yaml_loader.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace nls {
namespace {

std::string make_ansi(int r, int g, int b) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm", r, g, b);
    return buf;
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

std::optional<std::array<int, 3>> parse_hex_triplet(std::string_view hex) {
    if (hex.size() == 3) {
        int r = hex_value(hex[0]);
        int g = hex_value(hex[1]);
        int b = hex_value(hex[2]);
        if (r < 0 || g < 0 || b < 0) return std::nullopt;
        return std::array<int, 3>{r * 17, g * 17, b * 17};
    }
    if (hex.size() == 6) {
        int vals[6];
        for (size_t i = 0; i < 6; ++i) {
            vals[i] = hex_value(hex[i]);
            if (vals[i] < 0) return std::nullopt;
        }
        int r = vals[0] * 16 + vals[1];
        int g = vals[2] * 16 + vals[3];
        int b = vals[4] * 16 + vals[5];
        return std::array<int, 3>{r, g, b};
    }
    return std::nullopt;
}

std::string trim_copy(std::string s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::optional<std::array<int, 3>> parse_color_triplet(const std::string& value) {
    std::string trimmed = trim_copy(value);
    if (trimmed.empty()) return std::nullopt;

    std::string lower = to_lower(trimmed);
    std::string_view view(lower);
    if (!view.empty() && view.front() == '#') {
        view.remove_prefix(1);
    } else if (view.size() > 2 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X')) {
        view.remove_prefix(2);
    }

    if (auto rgb = parse_hex_triplet(view)) {
        return rgb;
    }

    std::string normalized = trimmed;
    for (char& ch : normalized) {
        if (ch == ',' || ch == ';') ch = ' ';
    }

    std::istringstream iss(normalized);
    int r = 0;
    int g = 0;
    int b = 0;
    if (!(iss >> r >> g >> b)) {
        return std::nullopt;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return std::nullopt;
    }
    return std::array<int, 3>{r, g, b};
}

std::unordered_map<std::string, std::array<int, 3>> make_default_color_map() {
    return {
        {"black", {0, 0, 0}},
        {"white", {255, 255, 255}},
        {"red", {255, 0, 0}},
        {"green", {0, 128, 0}},
        {"lime", {0, 255, 0}},
        {"limegreen", {50, 205, 50}},
        {"seagreen", {46, 139, 87}},
        {"mediumspringgreen", {0, 250, 154}},
        {"chartreuse", {127, 255, 0}},
        {"darkred", {139, 0, 0}},
        {"darkorange", {255, 140, 0}},
        {"forestgreen", {34, 139, 34}},
        {"darkgreen", {0, 100, 0}},
        {"navy", {0, 0, 128}},
        {"navyblue", {0, 0, 128}},
        {"darkblue", {0, 0, 139}},
        {"blue", {0, 0, 255}},
        {"cyan", {0, 255, 255}},
        {"aqua", {0, 255, 255}},
        {"dodgerblue", {30, 144, 255}},
        {"orange", {255, 165, 0}},
        {"gold", {255, 215, 0}},
        {"yellow", {255, 255, 0}},
        {"peachpuff", {255, 218, 185}},
        {"moccasin", {255, 228, 181}},
        {"slategray", {112, 128, 144}},
        {"slategrey", {112, 128, 144}},
        {"burlywood", {222, 184, 135}},
        {"indianred", {205, 92, 92}},
        {"royalblue", {65, 105, 225}},
        {"saddlebrown", {139, 69, 19}},
        {"sienna", {160, 82, 45}},
        {"darkkhaki", {189, 183, 107}},
        {"darkgray", {169, 169, 169}},
        {"darkgrey", {169, 169, 169}},
        {"gray", {128, 128, 128}},
        {"grey", {128, 128, 128}},
        {"lightgray", {211, 211, 211}},
        {"lightgrey", {211, 211, 211}},
        {"silver", {192, 192, 192}},
        {"brown", {165, 42, 42}},
        {"magenta", {255, 0, 255}},
        {"purple", {128, 0, 128}},
        {"pink", {255, 192, 203}}
    };
}

std::unordered_map<std::string, std::array<int, 3>> load_color_map_from_yaml() {
    std::unordered_map<std::string, std::array<int, 3>> result;
    auto path = find_resource("colors.yaml");
    if (path.empty()) {
        return result;
    }
    auto yaml_map = load_simple_yaml_map(path, true);
    for (auto& kv : yaml_map) {
        auto rgb = parse_color_triplet(kv.second);
        if (rgb) {
            result.emplace(kv.first, *rgb);
        }
    }
    return result;
}

const std::unordered_map<std::string, std::array<int, 3>>& color_map() {
    static const std::unordered_map<std::string, std::array<int, 3>> map = [] {
        auto loaded = load_color_map_from_yaml();
        if (!loaded.empty()) {
            return loaded;
        }
        return make_default_color_map();
    }();
    return map;
}

std::optional<std::string> parse_color_name(std::string_view name) {
    std::string trimmed;
    trimmed.reserve(name.size());
    for (char ch : name) {
        if (!std::isspace(static_cast<unsigned char>(ch))) trimmed.push_back(ch);
    }
    if (trimmed.empty()) return std::string{};
    std::string lower = to_lower(trimmed);
    if (lower == "none" || lower == "default") return std::string{};
    if (!lower.empty() && lower[0] == '#') {
        auto rgb = parse_hex_triplet(std::string_view(lower).substr(1));
        if (!rgb) return std::nullopt;
        return make_ansi((*rgb)[0], (*rgb)[1], (*rgb)[2]);
    }
    if (lower.size() > 1 && lower[0] == '0' && (lower[1] == 'x' || lower[1] == 'X')) {
        auto rgb = parse_hex_triplet(std::string_view(lower).substr(2));
        if (!rgb) return std::nullopt;
        return make_ansi((*rgb)[0], (*rgb)[1], (*rgb)[2]);
    }
    auto it = color_map().find(lower);
    if (it != color_map().end()) {
        return make_ansi(it->second[0], it->second[1], it->second[2]);
    }
    return std::nullopt;
}

ThemeColors make_fallback_theme() {
    ThemeColors theme;
    theme.set("dir", "\x1b[34m");
    theme.set("link", "\x1b[36m");
    theme.set("dead_link", "\x1b[31m");
    theme.set("recognized_file", "\x1b[37m");
    theme.set("unrecognized_file", "\x1b[37m");
    theme.set("executable_file", "\x1b[32m");
    theme.set("socket", "\x1b[32m");
    theme.set("blockdev", "\x1b[32m");
    theme.set("chardev", "\x1b[32m");
    theme.set("hidden", "\x1b[37m");
    theme.set("hidden_dir", "\x1b[34m");
    theme.set("write", "\x1b[31m");
    theme.set("read", "\x1b[32m");
    theme.set("exec", "\x1b[33m");
    theme.set("no_access", "\x1b[31m");
    theme.set("day_old", "");
    theme.set("hour_old", "");
    theme.set("no_modifier", "");
    theme.set("file_large", "");
    theme.set("file_medium", "");
    theme.set("file_small", "");
    theme.set("report", "");
    theme.set("user", "");
    theme.set("tree", "\x1b[36m");
    theme.set("empty", "\x1b[33m");
    theme.set("error", "\x1b[31m");
    theme.set("normal", "");
    theme.set("inode", "");
    theme.set("header_directory", "\x1b[36m");
    theme.set("header_names", "\x1b[37m");
    theme.set("addition", "\x1b[32m");
    theme.set("modification", "\x1b[33m");
    theme.set("deletion", "\x1b[31m");
    theme.set("untracked", "\x1b[35m");
    theme.set("unchanged", "\x1b[32m");
    theme.set("help_usage_label", "\x1b[33m");
    theme.set("help_usage_command", "\x1b[33m");
    theme.set("help_option_group", "\x1b[36m");
    theme.set("help_option_name", "\x1b[33m");
    theme.set("help_option_opts", "\x1b[34m");
    theme.set("help_option_desc", "\x1b[32m");
    theme.set("help_footer", "\x1b[35m");
    theme.set("help_description", "\x1b[35m");
    return theme;
}

ThemeColors load_theme(const std::string& filename, const ThemeColors& fallback) {
    ThemeColors theme = fallback;
    auto path = find_resource(filename);
    if (path.empty()) {
        return theme;
    }
    auto map = load_simple_yaml_map(path, true);
    for (auto& kv : map) {
        auto parsed = parse_color_name(kv.second);
        if (parsed) {
            theme.set(kv.first, *parsed);
        }
    }
    return theme;
}

ThemeColors g_dark;
ThemeColors g_light;
bool g_loaded = false;
const ThemeColors* g_active = nullptr;

void ensure_loaded() {
    if (g_loaded) return;
    g_loaded = true;
    ThemeColors fallback = make_fallback_theme();
    g_dark = load_theme("dark_colors.yaml", fallback);
    g_light = load_theme("light_colors.yaml", fallback);
    if (!g_active) {
        g_active = &g_dark;
    }
}

} // namespace

void ThemeColors::set(std::string key, std::string value) {
    values[std::move(key)] = std::move(value);
}

const std::string& ThemeColors::get(std::string_view key) const {
    static const std::string empty;
    auto it = values.find(std::string(key));
    if (it == values.end()) return empty;
    return it->second;
}

std::string ThemeColors::color_or(std::string_view key, std::string_view fallback) const {
    const std::string& val = get(key);
    if (!val.empty()) return val;
    return std::string(fallback);
}

void load_color_themes() {
    ensure_loaded();
}

void set_active_theme(ColorScheme scheme) {
    ensure_loaded();
    switch (scheme) {
        case ColorScheme::Light:
            g_active = &g_light;
            break;
        case ColorScheme::Dark:
        default:
            g_active = &g_dark;
            break;
    }
}

const ThemeColors& active_theme() {
    ensure_loaded();
    if (!g_active) g_active = &g_dark;
    return *g_active;
}

const ThemeColors& theme_for(ColorScheme scheme) {
    ensure_loaded();
    switch (scheme) {
        case ColorScheme::Light: return g_light;
        case ColorScheme::Dark:
        default: return g_dark;
    }
}

std::string apply_color(const std::string& color,
                        std::string_view text,
                        const ThemeColors& theme,
                        bool no_color) {
    if (no_color || color.empty()) return std::string(text);
    std::string out;
    out.reserve(color.size() + text.size() + theme.reset.size());
    out += color;
    out.append(text.begin(), text.end());
    out += theme.reset;
    return out;
}

} // namespace nls
