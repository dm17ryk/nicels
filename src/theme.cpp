#include "theme.h"

#include "resources.h"
#include "string_utils.h"
#include "yaml_loader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <optional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace nls {
namespace {

class ThemeSupport final {
public:
    static ThemeColors MakeFallbackTheme()
    {
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
        theme.set("owned", "");
        theme.set("group", "");
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

    static IconTheme MakeFallbackIcons()
    {
        IconTheme theme;
        theme.files["file"] = ToUtf8(u8"\uf15b");
        theme.files["exe"] = ToUtf8(u8"\uf144");
        theme.files["sh"] = ToUtf8(u8"\uf489");
        theme.files["txt"] = ToUtf8(u8"\uf15c");
        theme.files["png"] = ToUtf8(u8"\uf1c5");
        theme.files["jpg"] = ToUtf8(u8"\uf1c5");
        theme.files["jpeg"] = ToUtf8(u8"\uf1c5");
        theme.files["gif"] = ToUtf8(u8"\uf1c5");
        theme.files["svg"] = ToUtf8(u8"\uf1c5");
        theme.files["zip"] = ToUtf8(u8"\uf1c6");
        theme.files["gz"] = ToUtf8(u8"\uf1c6");
        theme.files["7z"] = ToUtf8(u8"\uf1c6");
        theme.files["pdf"] = ToUtf8(u8"\uf1c1");
        theme.files["cpp"] = ToUtf8(u8"\ue61d");
        theme.files["cc"] = ToUtf8(u8"\ue61d");
        theme.files["c"] = ToUtf8(u8"\uf0fd");
        theme.files["h"] = ToUtf8(u8"\uf0fd");
        theme.files["hpp"] = ToUtf8(u8"\uf0fd");
        theme.files["py"] = ToUtf8(u8"\ue235");
        theme.files["rb"] = ToUtf8(u8"\ue21e");
        theme.files["js"] = ToUtf8(u8"\ue74e");
        theme.files["ts"] = ToUtf8(u8"\ue628");
        theme.files["json"] = ToUtf8(u8"\ue60b");
        theme.files["md"] = ToUtf8(u8"\uf48a");
        theme.folders["folder"] = ToUtf8(u8"\uf07b");
        theme.folders["hidden"] = ToUtf8(u8"\uf19fc");
        return theme;
    }

    static void MergeMap(std::unordered_map<std::string, std::string>& dest,
                         const std::unordered_map<std::string, std::string>& src,
                         bool lowercase_values = false)
    {
        for (const auto& kv : src) {
            std::string value = kv.second;
            if (lowercase_values) value = StringUtils::ToLower(value);
            dest[StringUtils::ToLower(kv.first)] = std::move(value);
        }
    }

    static std::optional<std::string> ParseColorName(std::string_view name)
    {
        std::string trimmed = StringUtils::Trim(name);
        std::erase_if(trimmed, [](unsigned char ch) { return std::isspace(ch) != 0; });
        if (trimmed.empty()) return std::string{};
        std::string lower = StringUtils::ToLower(trimmed);
        if (lower == "none" || lower == "default") return std::string{};
        if (!lower.empty() && lower[0] == '#') {
            auto rgb = ParseHexTriplet(std::string_view(lower).substr(1));
            if (!rgb) return std::nullopt;
            return MakeAnsi((*rgb)[0], (*rgb)[1], (*rgb)[2]);
        }
        if (lower.size() > 1 && lower[0] == '0' && (lower[1] == 'x' || lower[1] == 'X')) {
            auto rgb = ParseHexTriplet(std::string_view(lower).substr(2));
            if (!rgb) return std::nullopt;
            return MakeAnsi((*rgb)[0], (*rgb)[1], (*rgb)[2]);
        }
        auto it = ColorMap().find(lower);
        if (it != ColorMap().end()) {
            return MakeAnsi(it->second[0], it->second[1], it->second[2]);
        }
        return std::nullopt;
    }

    static std::string ToUtf8(std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    static bool IsPathWithin(const std::filesystem::path& path, const std::filesystem::path& base)
    {
        if (path.empty() || base.empty()) return false;
        auto normalized_path = path.lexically_normal();
        auto normalized_base = base.lexically_normal();

        auto path_it = normalized_path.begin();
        for (auto base_it = normalized_base.begin(); base_it != normalized_base.end(); ++base_it, ++path_it) {
            if (path_it == normalized_path.end() || *path_it != *base_it) {
                return false;
            }
        }
        return true;
    }

private:
    static std::string MakeAnsi(int r, int g, int b)
    {
        return std::format("\x1b[38;2;{};{};{}m", r, g, b);
    }

    static constexpr int HexValue(char ch) noexcept
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    }

    static constexpr std::optional<std::array<int, 3>> ParseHexTriplet(std::string_view hex)
    {
        if (hex.size() == 3) {
            int r = HexValue(hex[0]);
            int g = HexValue(hex[1]);
            int b = HexValue(hex[2]);
            if (r < 0 || g < 0 || b < 0) return std::nullopt;
            return std::array<int, 3>{r * 17, g * 17, b * 17};
        }
        if (hex.size() == 6) {
            int vals[6];
            for (size_t i = 0; i < 6; ++i) {
                vals[i] = HexValue(hex[i]);
                if (vals[i] < 0) return std::nullopt;
            }
            int r = vals[0] * 16 + vals[1];
            int g = vals[2] * 16 + vals[3];
            int b = vals[4] * 16 + vals[5];
            return std::array<int, 3>{r, g, b};
        }
        return std::nullopt;
    }

    static std::optional<std::array<int, 3>> ParseColorTriplet(const std::string& value)
    {
        std::string trimmed = StringUtils::Trim(value);
        if (trimmed.empty()) return std::nullopt;

        std::string lower = StringUtils::ToLower(trimmed);
        std::string_view view(lower);
        if (!view.empty() && view.front() == '#') {
            view.remove_prefix(1);
        } else if (view.size() > 2 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X')) {
            view.remove_prefix(2);
        }

        if (auto rgb = ParseHexTriplet(view)) {
            return rgb;
        }

        std::string normalized = trimmed;
        std::replace_if(normalized.begin(), normalized.end(), [](char ch) {
            return ch == ',' || ch == ';';
        }, ' ');

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

    static std::unordered_map<std::string, std::array<int, 3>> MakeDefaultColorMap()
    {
        static constexpr std::array<std::pair<std::string_view, std::array<int, 3>>, 44> kDefaults{{
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
        }};

        std::unordered_map<std::string, std::array<int, 3>> map;
        map.reserve(kDefaults.size());
        for (const auto& [name, rgb] : kDefaults) {
            map.emplace(name, rgb);
        }
        return map;
    }

    static std::unordered_map<std::string, std::array<int, 3>> LoadColorMapFromYaml()
    {
        auto result = MakeDefaultColorMap();
        auto apply = [&](const std::filesystem::path& path) {
            if (path.empty()) return;
            auto yaml_map = YamlLoader::LoadSimpleMap(path, true);
            for (auto& kv : yaml_map) {
                auto rgb = ParseColorTriplet(kv.second);
                if (rgb) {
                    auto lower = StringUtils::ToLower(kv.first);
                    auto it = result.find(lower);
                    if (it != result.end()) {
                        it->second = *rgb;
                    }
                }
            }
        };

        auto primary_path = ResourceManager::find("colors.yaml");
        apply(primary_path);

        const auto user_dir = ResourceManager::userConfigDir();
        const auto env_dir = ResourceManager::envOverrideDir();
        const bool primary_from_env = ThemeSupport::IsPathWithin(primary_path, env_dir);
        if (!user_dir.empty() && (!primary_from_env || primary_path.empty())) {
            std::filesystem::path user_path = user_dir / "colors.yaml";
            if (user_path != primary_path) {
                std::error_code ec;
                if (std::filesystem::exists(user_path, ec) && !ec) {
                    apply(user_path);
                }
            }
        }

        return result;
    }

    static const std::unordered_map<std::string, std::array<int, 3>>& ColorMap()
    {
        static const std::unordered_map<std::string, std::array<int, 3>> map = LoadColorMapFromYaml();
        return map;
    }
};

} // namespace

void ThemeColors::set(std::string key, std::string value)
{
    values[std::move(key)] = std::move(value);
}

const std::string& ThemeColors::get(std::string_view key) const
{
    static const std::string empty;
    auto it = values.find(std::string(key));
    if (it == values.end()) return empty;
    return it->second;
}

std::string ThemeColors::color_or(std::string_view key, std::string_view fallback) const
{
    const std::string& val = get(key);
    if (!val.empty()) return val;
    return std::string(fallback);
}

Theme& Theme::instance()
{
    static Theme instance;
    return instance;
}

void Theme::initialize(ColorScheme scheme, std::optional<std::string> custom_theme)
{
    ensure_loaded();

    custom_theme_name_.reset();

    if (custom_theme && !custom_theme->empty()) {
        std::string name = StringUtils::Trim(*custom_theme);
        auto ends_with = [](std::string_view value, std::string_view suffix) {
            return value.size() >= suffix.size()
                && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        };

        if (ends_with(name, ".yaml")) {
            name.erase(name.size() - 5);
        }
        if (ends_with(name, "_theme")) {
            name.erase(name.size() - 6);
        }

        bool has_separator = name.find('/') != std::string::npos || name.find('\\') != std::string::npos;
        if (name.empty() || has_separator) {
            std::cerr << "nls: error: theme '" << *custom_theme << "' not found\n";
        } else {
            bool found = false;
            ThemeColors loaded = load_theme_file(name + "_theme.yaml", &found);
            if (!found) {
                std::cerr << "nls: error: theme '" << *custom_theme << "' not found\n";
            } else {
                custom_theme_name_ = std::move(name);
                custom_theme_ = std::move(loaded);
            }
        }
    }

    set_active_scheme(scheme);
}

void Theme::set_active_scheme(ColorScheme scheme)
{
    ensure_loaded();
    active_scheme_ = scheme;
}

ColorScheme Theme::active_scheme() const
{
    return active_scheme_;
}

const ThemeColors& Theme::colors()
{
    ensure_loaded();
    if (custom_theme_name_) {
        return custom_theme_;
    }
    return active_scheme_ == ColorScheme::Light ? light_ : dark_;
}

const ThemeColors& Theme::colors(ColorScheme scheme)
{
    ensure_loaded();
    if (custom_theme_name_) {
        return custom_theme_;
    }
    return scheme == ColorScheme::Light ? light_ : dark_;
}

std::string Theme::color_or(std::string_view key, std::string_view fallback)
{
    return colors().color_or(key, fallback);
}

const std::string& Theme::color(std::string_view key)
{
    return colors().get(key);
}

IconResult Theme::get_file_icon(std::string_view filename, bool is_executable)
{
    ensure_loaded();
    return file_icon(filename, is_executable);
}

IconResult Theme::get_folder_icon(std::string_view folder_name)
{
    ensure_loaded();
    return folder_icon(folder_name);
}

IconResult Theme::get_icon(std::string_view name, bool is_dir, bool is_executable)
{
    ensure_loaded();
    if (is_dir) {
        return folder_icon(name);
    }
    return file_icon(name, is_executable);
}

void Theme::ensure_loaded()
{
    if (loaded_) return;
    loaded_ = true;
    fallback_ = ThemeSupport::MakeFallbackTheme();
    dark_ = load_theme_file("dark_theme.yaml");
    light_ = load_theme_file("light_theme.yaml");
    load_icons();
}

ThemeColors Theme::load_theme_file(const std::string& filename, bool* found)
{
    ThemeColors theme = fallback_;
    bool loaded_any = false;
    auto apply = [&](const std::filesystem::path& path) {
        if (path.empty()) return;
        loaded_any = true;
        auto map = YamlLoader::LoadSimpleMap(path, true);
        for (auto& kv : map) {
            if (theme.values.find(kv.first) == theme.values.end()) {
                continue;
            }
            auto parsed = ThemeSupport::ParseColorName(kv.second);
            if (parsed) {
                theme.set(kv.first, *parsed);
            }
        }
    };

    auto primary_path = ResourceManager::find(filename);
    apply(primary_path);

    const auto user_dir = ResourceManager::userConfigDir();
    const auto env_dir = ResourceManager::envOverrideDir();
    const bool primary_from_env = ThemeSupport::IsPathWithin(primary_path, env_dir);
    if (!user_dir.empty() && (!primary_from_env || primary_path.empty())) {
        std::filesystem::path user_path = user_dir / filename;
        if (user_path != primary_path) {
            std::error_code ec;
            if (std::filesystem::exists(user_path, ec) && !ec) {
                apply(user_path);
            }
        }
    }
    if (found) {
        *found = loaded_any;
    }
    return theme;
}

void Theme::load_icons()
{
    icons_ = ThemeSupport::MakeFallbackIcons();

    const auto user_dir = ResourceManager::userConfigDir();
    const auto env_dir = ResourceManager::envOverrideDir();

    auto merge_with_overrides = [&](const std::string& name,
                                    auto& target,
                                    bool lowercase_values) {
        auto primary_path = ResourceManager::find(name);
        if (!primary_path.empty()) {
            ThemeSupport::MergeMap(target, YamlLoader::LoadSimpleMap(primary_path, true), lowercase_values);
        }

        const bool primary_from_env = ThemeSupport::IsPathWithin(primary_path, env_dir);
        if (!user_dir.empty() && (!primary_from_env || primary_path.empty())) {
            std::filesystem::path user_path = user_dir / name;
            if (user_path != primary_path) {
                std::error_code ec;
                if (std::filesystem::exists(user_path, ec) && !ec) {
                    ThemeSupport::MergeMap(target, YamlLoader::LoadSimpleMap(user_path, true), lowercase_values);
                }
            }
        }
    };

    merge_with_overrides("files.yaml", icons_.files, false);
    merge_with_overrides("file_aliases.yaml", icons_.file_aliases, true);
    merge_with_overrides("folders.yaml", icons_.folders, false);
    merge_with_overrides("folder_aliases.yaml", icons_.folder_aliases, true);

    if (icons_.files.find("file") == icons_.files.end()) {
        icons_.files["file"] = ThemeSupport::ToUtf8(u8"\uf15b");
    }
    if (icons_.folders.find("folder") == icons_.folders.end()) {
        icons_.folders["folder"] = ThemeSupport::ToUtf8(u8"\uf07b");
    }
}

IconResult Theme::folder_icon(std::string_view name)
{
    std::string key = StringUtils::ToLower(name);
    auto direct = icons_.folders.find(key);
    if (direct != icons_.folders.end()) {
        bool recognized = key != "folder";
        return {direct->second, recognized};
    }
    auto alias = icons_.folder_aliases.find(key);
    if (alias != icons_.folder_aliases.end()) {
        auto base = icons_.folders.find(alias->second);
        if (base != icons_.folders.end()) {
            bool recognized = alias->second != "folder";
            return {base->second, recognized};
        }
    }
    if (!key.empty() && key.front() == '.') {
        auto hidden = icons_.folders.find("hidden");
        if (hidden != icons_.folders.end()) {
            return {hidden->second, true};
        }
    }
    auto fallback = icons_.folders.find("folder");
    if (fallback != icons_.folders.end()) {
        return {fallback->second, false};
    }
    return {ThemeSupport::ToUtf8(u8"\uf07b"), false};
}

IconResult Theme::file_icon(std::string_view name, bool is_exec)
{
    std::string key = StringUtils::ToLower(name);

    auto direct = icons_.files.find(key);
    if (direct != icons_.files.end()) {
        bool recognized = key != "file";
        return {direct->second, recognized};
    }
    auto alias = icons_.file_aliases.find(key);
    if (alias != icons_.file_aliases.end()) {
        auto base = icons_.files.find(alias->second);
        if (base != icons_.files.end()) {
            bool recognized = alias->second != "file";
            return {base->second, recognized};
        }
    }

    auto dot = key.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < key.size()) {
        std::string ext = key.substr(dot + 1);
        auto ext_icon = icons_.files.find(ext);
        if (ext_icon != icons_.files.end()) {
            bool recognized = ext != "file";
            return {ext_icon->second, recognized};
        }
        auto ext_alias = icons_.file_aliases.find(ext);
        if (ext_alias != icons_.file_aliases.end()) {
            auto base = icons_.files.find(ext_alias->second);
            if (base != icons_.files.end()) {
                bool recognized = ext_alias->second != "file";
                return {base->second, recognized};
            }
        }
    }

    if (is_exec) {
        auto exec_icon = icons_.files.find("exe");
        if (exec_icon != icons_.files.end()) {
            return {exec_icon->second, true};
        }
    }

    auto fallback = icons_.files.find("file");
    if (fallback != icons_.files.end()) {
        return {fallback->second, false};
    }
    return {ThemeSupport::ToUtf8(u8"\uf15b"), false};
}

std::string Theme::ApplyColor(const std::string& color,
                              std::string_view text,
                              const ThemeColors& theme,
                              bool no_color)
{
    if (no_color || color.empty()) return std::string(text);
    std::string out;
    out.reserve(color.size() + text.size() + theme.reset.size());
    out += color;
    out.append(text.begin(), text.end());
    out += theme.reset;
    return out;
}

} // namespace nls
