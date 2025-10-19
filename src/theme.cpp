#include "theme.h"

#include "resources.h"
#include "string_utils.h"

#include "perf.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sqlite3.h>

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

    static std::string ToUtf8(std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    static std::string MakeAnsiFromRgb(std::uint32_t rgb)
    {
        int r = static_cast<int>((rgb >> 16) & 0xFF);
        int g = static_cast<int>((rgb >> 8) & 0xFF);
        int b = static_cast<int>(rgb & 0xFF);
        return MakeAnsi(r, g, b);
    }

private:
    static std::string MakeAnsi(int r, int g, int b)
    {
        return std::format("\x1b[38;2;{};{};{}m", r, g, b);
    }
};

struct SqliteCloser {
    void operator()(sqlite3* db) const noexcept
    {
        if (db) {
            sqlite3_close(db);
        }
    }
};

struct SqliteStmtFinalizer {
    void operator()(sqlite3_stmt* stmt) const noexcept
    {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

using SqliteDbPtr = std::unique_ptr<sqlite3, SqliteCloser>;
using SqliteStmtPtr = std::unique_ptr<sqlite3_stmt, SqliteStmtFinalizer>;

static std::string PathToUtf8(const std::filesystem::path& path)
{
    auto u8 = path.u8string();
    std::string out;
    out.reserve(u8.size());
    for (char8_t ch : u8) {
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

static SqliteDbPtr OpenConfigDatabase(const std::filesystem::path& path)
{
    sqlite3* raw = nullptr;
    const std::string utf8 = PathToUtf8(path);
    int rc = sqlite3_open_v2(utf8.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        if (raw) {
            std::cerr << "nls: error: failed to open config database '" << path << "': "
                      << sqlite3_errmsg(raw) << '\n';
            sqlite3_close(raw);
        } else {
            std::cerr << "nls: error: failed to open config database '" << path << "': "
                      << sqlite3_errstr(rc) << '\n';
        }
        return {};
    }
    return SqliteDbPtr(raw);
}

static bool LoadThemeColors(sqlite3* db, int theme_id, ThemeColors& target, std::size_t& entries_out)
{
    static constexpr const char* kSql =
        "SELECT element, c.value FROM Theme_colors t "
        "JOIN Colors c ON t.color_id = c.id WHERE t.id = ?1;";
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        entries_out = 0;
        return false;
    }
    SqliteStmtPtr stmt(stmt_raw);
    rc = sqlite3_bind_int(stmt.get(), 1, theme_id);
    if (rc != SQLITE_OK) {
        entries_out = 0;
        return false;
    }

    std::size_t entries = 0;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        const unsigned char* element = sqlite3_column_text(stmt.get(), 0);
        if (!element) {
            continue;
        }
        std::string key = StringUtils::ToLower(reinterpret_cast<const char*>(element));
        std::uint32_t rgb = static_cast<std::uint32_t>(sqlite3_column_int64(stmt.get(), 1)) & 0xFFFFFFu;
        target.set(std::move(key), ThemeSupport::MakeAnsiFromRgb(rgb));
        ++entries;
    }

    entries_out = entries;
    return rc == SQLITE_DONE && entries > 0;
}


static std::optional<int> LookupThemeId(sqlite3* db, std::string_view name)
{
    static constexpr const char* kSql =
        "SELECT id FROM Themes WHERE LOWER(name) = LOWER(?1) LIMIT 1;";
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }
    SqliteStmtPtr stmt(stmt_raw);
    rc = sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    return std::nullopt;
}

struct IconLoadStats {
    std::size_t sources = 0;
    std::size_t entries = 0;
    bool success = true;
};

static IconLoadStats LoadIconsFromDatabase(sqlite3* db, IconTheme& icons)
{
    IconLoadStats stats;

    auto load_map = [&](const std::string& query, auto& target, bool is_alias) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Warning: Failed to prepare query: " << query << "\n";
            stats.success = false;
            return;
        }
        ++stats.sources;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            // Check column count
            int col_count = sqlite3_column_count(stmt);
            if (col_count < 2) {
                std::cerr << "Warning: Query '" << query << "' returned less than 2 columns\n";
                continue;
            }
            const unsigned char* key = sqlite3_column_text(stmt, 0);
            const unsigned char* value = sqlite3_column_text(stmt, 1);
            if (!key || !value) {
                std::cerr << "Warning: Missing key or icon value in query '" << query << "'\n";
                continue;
            }
            std::string key_str(StringUtils::ToLower(reinterpret_cast<const char*>(key)));
            std::string value_str(reinterpret_cast<const char*>(value));
            if (is_alias) {
                value_str = StringUtils::ToLower(value_str);
            }
            // Check for malformed icon value (example: empty string)
            if (value_str.empty()) {
                std::cerr << "Warning: Malformed icon value for key '" << key_str << "' in query '" << query << "'\n";
                continue;
            }
            target[key_str] = value_str;
            ++stats.entries;
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "Warning: Error executing query: " << query << "\n";
            stats.success = false;
        }
        sqlite3_finalize(stmt);
    };

    icons.files.clear();
    icons.folders.clear();
    icons.file_aliases.clear();
    icons.folder_aliases.clear();

    load_map("SELECT name, icon FROM Files;", icons.files, false);
    load_map("SELECT name, icon FROM Folders;", icons.folders, false);
    load_map("SELECT alias, name FROM File_Aliases;", icons.file_aliases, true);
    load_map("SELECT alias, name FROM Folder_Aliases;", icons.folder_aliases, true);

    return stats;
}




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
            std::vector<std::filesystem::path> candidate_paths;
            if (!database_path_.empty()) {
                candidate_paths.push_back(database_path_);
            }
            for (const auto& candidate : ResourceManager::databaseCandidates()) {
                if (candidate.empty()) continue;
                if (std::find(candidate_paths.begin(), candidate_paths.end(), candidate) == candidate_paths.end()) {
                    candidate_paths.push_back(candidate);
                }
            }

            bool loaded_custom = false;
            for (const auto& candidate : candidate_paths) {
                if (candidate.empty()) continue;
                if (auto db = OpenConfigDatabase(candidate)) {
                    auto theme_id = LookupThemeId(db.get(), name);
                    if (!theme_id) {
                        continue;
                    }
                    ThemeColors loaded = fallback_;
                    std::size_t entries = 0;
                    if (!LoadThemeColors(db.get(), *theme_id, loaded, entries)) {
                        continue;
                    }
                    custom_theme_name_ = name;
                    custom_theme_ = std::move(loaded);
                    database_path_ = candidate;
                    loaded_custom = true;
                    break;
                }
            }

            if (!loaded_custom) {
                std::cerr << "nls: error: theme '" << *custom_theme << "' not found\n";
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
    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("theme::ensure_loaded");
        perf_manager.IncrementCounter("theme::ensure_loaded_calls");
    }

    loaded_ = true;
    fallback_ = ThemeSupport::MakeFallbackTheme();
    dark_ = fallback_;
    light_ = fallback_;
    icons_ = ThemeSupport::MakeFallbackIcons();
    database_path_.clear();

    std::size_t theme_sources = 0;
    std::size_t theme_entries = 0;
    IconLoadStats icon_stats{};

    for (const auto& candidate : ResourceManager::databaseCandidates()) {
        if (candidate.empty()) continue;
        if (auto db = OpenConfigDatabase(candidate)) {
            std::size_t dark_entries = 0;
            if (LoadThemeColors(db.get(), 1, dark_, dark_entries)) {
                ++theme_sources;
                theme_entries += dark_entries;
            }
            std::size_t light_entries = 0;
            if (LoadThemeColors(db.get(), 2, light_, light_entries)) {
                ++theme_sources;
                theme_entries += light_entries;
            }
            icon_stats = LoadIconsFromDatabase(db.get(), icons_);
            database_path_ = candidate;
            break;
        }
    }

    if (icons_.files.find("file") == icons_.files.end()) {
        icons_.files["file"] = ThemeSupport::ToUtf8(u8"\uf15b");
    }
    if (icons_.folders.find("folder") == icons_.folders.end()) {
        icons_.folders["folder"] = ThemeSupport::ToUtf8(u8"\uf07b");
    }

    if (perf_enabled) {
        perf_manager.IncrementCounter("theme::theme_sources", theme_sources);
        perf_manager.IncrementCounter("theme::theme_entries_processed", theme_entries);
        perf_manager.IncrementCounter("theme::icon_sources", icon_stats.sources);
        perf_manager.IncrementCounter("theme::icon_entries_processed", icon_stats.entries);
    }
}

IconResult Theme::folder_icon(std::string_view name)
{
    auto find_icon_for_key = [&](std::string_view lookup_key) -> std::optional<IconResult> {
        std::string lookup(lookup_key);

        auto direct = icons_.folders.find(lookup);
        if (direct != icons_.folders.end()) {
            bool recognized = lookup != "folder";
            return IconResult {direct->second, recognized};
        }

        auto alias = icons_.folder_aliases.find(lookup);
        if (alias != icons_.folder_aliases.end()) {
            auto base = icons_.folders.find(alias->second);
            if (base != icons_.folders.end()) {
                bool recognized = alias->second != "folder";
                return IconResult {base->second, recognized};
            }
        }

        return std::nullopt;
    };

    std::string key = StringUtils::ToLower(name);

    if (auto icon = find_icon_for_key(key)) {
        return *icon;
    }

    if (!key.empty() && key.front() == '.') {
        auto non_dot_pos = key.find_first_not_of('.');
        if (non_dot_pos != std::string::npos) {
            auto trimmed = key.substr(non_dot_pos);
            if (auto icon = find_icon_for_key(trimmed)) {
                return *icon;
            }
        }

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
