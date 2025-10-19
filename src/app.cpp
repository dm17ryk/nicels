#include "app.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "file_ownership_resolver.h"
#include "git_status.h"
#include "path_processor.h"
#include "perf.h"
#include "platform.h"
#include "resources.h"
#include "string_utils.h"
#include "symlink_resolver.h"
#include "theme.h"

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace nls {

namespace {

struct IconMetadata {
    std::string name;
    std::string icon;
    std::string icon_class;
    std::optional<std::uint32_t> icon_utf16;
    std::optional<std::uint32_t> icon_hex;
    std::string description;
    std::string used_by;
};

struct AliasMetadata {
    std::string alias;
    std::string target_name;
    std::string target_key;
};

struct AliasOutput {
    std::string name;
    std::string alias;
    std::string icon;
    std::string icon_class;
    std::optional<std::uint32_t> icon_utf16;
    std::optional<std::uint32_t> icon_hex;
    std::string description;
    std::string used_by;
    bool missing_target = false;
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
using IconMap = std::unordered_map<std::string, IconMetadata>;
using AliasMap = std::unordered_map<std::string, AliasMetadata>;

std::string PathToUtf8(const std::filesystem::path& path)
{
    auto u8 = path.u8string();
    std::string out;
    out.reserve(u8.size());
    for (char8_t ch : u8) {
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

SqliteDbPtr OpenDatabase(const std::filesystem::path& path)
{
    sqlite3* raw = nullptr;
    const std::string utf8 = PathToUtf8(path);
    int rc = sqlite3_open_v2(utf8.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        if (raw) {
            std::cerr << "nls: warning: failed to open config database '" << path << "': "
                      << sqlite3_errmsg(raw) << "\n";
            sqlite3_close(raw);
        } else {
            std::cerr << "nls: warning: failed to open config database '" << path << "': "
                      << sqlite3_errstr(rc) << "\n";
        }
        return {};
    }
    return SqliteDbPtr(raw);
}

std::string ColumnText(sqlite3_stmt* stmt, int column)
{
    const unsigned char* value = sqlite3_column_text(stmt, column);
    if (!value) {
        return {};
    }
    return reinterpret_cast<const char*>(value);
}

std::optional<std::uint32_t> ColumnCode(sqlite3_stmt* stmt, int column)
{
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    std::uint32_t value = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, column));
    if (value == 0) {
        return std::nullopt;
    }
    return value;
}

bool LoadIconTable(sqlite3* db,
                   [[maybe_unused]] const std::filesystem::path& source,
                   const char* sql,
                   IconMap& target)
{
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    SqliteStmtPtr stmt(stmt_raw);
    sqlite3_stmt* handle = stmt.get();
    bool success = true;
    while ((rc = sqlite3_step(handle)) == SQLITE_ROW) {
        IconMetadata record;
        record.name = ColumnText(handle, 0);
        if (record.name.empty()) {
            continue;
        }
        record.icon = ColumnText(handle, 1);
        record.icon_class = ColumnText(handle, 2);
        record.icon_utf16 = ColumnCode(handle, 3);
        record.icon_hex = ColumnCode(handle, 4);
        record.description = ColumnText(handle, 5);
        record.used_by = ColumnText(handle, 6);

        std::string key = StringUtils::ToLower(record.name);
        target[std::move(key)] = std::move(record);
    }
    if (rc != SQLITE_DONE) {
        success = false;
    }
    return success;
}

bool LoadAliasTable(sqlite3* db,
                    [[maybe_unused]] const std::filesystem::path& source,
                    const char* sql,
                    AliasMap& target)
{
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    SqliteStmtPtr stmt(stmt_raw);
    sqlite3_stmt* handle = stmt.get();
    bool success = true;
    while ((rc = sqlite3_step(handle)) == SQLITE_ROW) {
        std::string alias = ColumnText(handle, 0);
        std::string name = ColumnText(handle, 1);
        if (alias.empty() || name.empty()) {
            continue;
        }
        AliasMetadata record;
        record.alias = alias;
        record.target_name = name;
        record.target_key = StringUtils::ToLower(record.target_name);
        std::string alias_key = StringUtils::ToLower(record.alias);
        target[std::move(alias_key)] = std::move(record);
    }
    if (rc != SQLITE_DONE) {
        success = false;
    }
    return success;
}

bool MergeDatabase(const std::filesystem::path& path,
                   IconMap& files,
                   IconMap& folders,
                   AliasMap& file_aliases,
                   AliasMap& folder_aliases)
{
    auto db = OpenDatabase(path);
    if (!db) {
        return false;
    }
    bool loaded = false;
    bool had_error = false;
    std::string last_error;

    if (LoadIconTable(db.get(), path,
        "SELECT name, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code, description, used_by "
        "FROM Files;",
        files)) {
        loaded = true;
    } else {
        had_error = true;
        last_error = sqlite3_errmsg(db.get());
    }

    if (LoadIconTable(db.get(), path,
        "SELECT name, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code, description, used_by "
        "FROM Folders;",
        folders)) {
        loaded = true;
    } else {
        had_error = true;
        last_error = sqlite3_errmsg(db.get());
    }

    if (LoadAliasTable(db.get(), path,
        "SELECT alias, name FROM File_Aliases;",
        file_aliases)) {
        loaded = true;
    } else {
        had_error = true;
        last_error = sqlite3_errmsg(db.get());
    }

    if (LoadAliasTable(db.get(), path,
        "SELECT alias, name FROM Folder_Aliases;",
        folder_aliases)) {
        loaded = true;
    } else {
        had_error = true;
        last_error = sqlite3_errmsg(db.get());
    }

    if (had_error) {
        std::cerr << "nls: warning: unable to read complete configuration database '" << path
                  << "': " << last_error << "\n";
    }

    return loaded;
}

std::string FormatUtf16(const std::optional<std::uint32_t>& code)
{
    if (!code) {
        return {};
    }
    std::ostringstream oss;
    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (*code & 0xFFFFu);
    return oss.str();
}

std::string FormatHex(const std::optional<std::uint32_t>& code)
{
    if (!code) {
        return {};
    }
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << (*code & 0xFFFFu);
    return oss.str();
}

std::vector<IconMetadata> MakeSortedIcons(const IconMap& source)
{
    std::vector<IconMetadata> rows;
    rows.reserve(source.size());
    for (const auto& [_, value] : source) {
        rows.push_back(value);
    }
    std::sort(rows.begin(), rows.end(), [](const IconMetadata& lhs, const IconMetadata& rhs) {
        return lhs.name < rhs.name;
    });
    return rows;
}

std::vector<AliasOutput> MakeAliasOutput(const AliasMap& aliases, const IconMap& icons)
{
    std::vector<AliasOutput> rows;
    rows.reserve(aliases.size());
    for (const auto& [_, value] : aliases) {
        AliasOutput row;
        row.name = value.target_name;
        row.alias = value.alias;
        auto icon_it = icons.find(value.target_key);
        if (icon_it != icons.end()) {
            row.icon = icon_it->second.icon;
            row.icon_class = icon_it->second.icon_class;
            row.icon_utf16 = icon_it->second.icon_utf16;
            row.icon_hex = icon_it->second.icon_hex;
            row.description = icon_it->second.description;
            row.used_by = icon_it->second.used_by;
        } else {
            row.missing_target = true;
        }
        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(), [](const AliasOutput& lhs, const AliasOutput& rhs) {
        if (lhs.name == rhs.name) {
            return lhs.alias < rhs.alias;
        }
        return lhs.name < rhs.name;
    });
    return rows;
}

void PrintIconRows(const std::vector<IconMetadata>& rows)
{
    std::cout << "name\ticon\ticon_class\tIcon_UTF_16_codes\tIcon_Hex_Code\tdescription\tused_by\n";
    for (const auto& row : rows) {
        std::cout << row.name << '\t'
                  << row.icon << '\t'
                  << row.icon_class << '\t'
                  << FormatUtf16(row.icon_utf16) << '\t'
                  << FormatHex(row.icon_hex) << '\t'
                  << row.description << '\t'
                  << row.used_by << '\n';
    }
}

void PrintAliasRows(const std::vector<AliasOutput>& rows)
{
    std::cout << "name\talias\ticon\ticon_class\tIcon_UTF_16_codes\tIcon_Hex_Code\tdescription\tused_by\n";
    for (const auto& row : rows) {
        if (row.missing_target) {
            std::cerr << "nls: warning: alias '" << row.alias << "' references missing entry '"
                      << row.name << "'\n";
        }
        std::cout << row.name << '\t'
                  << row.alias << '\t'
                  << row.icon << '\t'
                  << row.icon_class << '\t'
                  << FormatUtf16(row.icon_utf16) << '\t'
                  << FormatHex(row.icon_hex) << '\t'
                  << row.description << '\t'
                  << row.used_by << '\n';
    }
}

} // namespace

int App::run(int argc, char** argv) {
    const bool virtual_terminal_enabled = Platform::enableVirtualTerminal();
    ResourceManager::initPaths(argc > 0 ? argv[0] : nullptr);

    config_ = &parser_.Parse(argc, argv);

    if (config_->copy_config_only()) {
        ResourceManager::CopyResult copy_result;
        std::error_code copy_ec = ResourceManager::copyDefaultsToUserConfig(copy_result);
        if (copy_ec) {
            std::cerr << "nls: error: failed to copy configuration files: " << copy_ec.message() << "\n";
            config_ = nullptr;
            return 1;
        }

        if (copy_result.copied.empty() && copy_result.skipped.empty()) {
            std::cout << "nls: no configuration files found to copy\n";
        } else {
            for (const auto& path : copy_result.copied) {
                std::cout << "nls: copied " << path << "\n";
            }
            for (const auto& path : copy_result.skipped) {
                std::cout << "nls: skipped (already exists) " << path << "\n";
            }
        }

        config_ = nullptr;
        return 0;
    }

    if (config_->db_action() != Config::DbAction::None) {
        int db_rc = runDatabaseCommand(config_->db_action());
        config_ = nullptr;
        return db_rc;
    }

    perf::Manager& perf_manager = perf::Manager::Instance();
    perf_manager.set_enabled(config_->perf_logging());
    std::optional<perf::Timer> run_timer;
    if (perf_manager.enabled()) {
        run_timer.emplace("app::run");
    }
    if (!virtual_terminal_enabled) {
        config_->set_no_color(true);
    }

    ColorScheme scheme = ColorScheme::Dark;
    switch (options().color_theme()) {
        case Config::ColorTheme::Light:
            scheme = ColorScheme::Light;
            break;
        case Config::ColorTheme::Dark:
        case Config::ColorTheme::Default:
        default:
            scheme = ColorScheme::Dark;
            break;
    }
    Theme::instance().initialize(scheme, options().theme_name());

    scanner_ = std::make_unique<FileScanner>(options(), ownership_resolver_, symlink_resolver_);
    renderer_ = std::make_unique<Renderer>(options());
    PathProcessor processor{options(), *scanner_, *renderer_, git_status_};

    VisitResult rc = VisitResult::Ok;
    for (const auto& path : options().paths()) {
        VisitResult path_result = VisitResult::Ok;
        try {
            path_result = processor.process(fs::path(path));
        } catch (const std::exception& e) {
            std::cerr << "nls: error: " << e.what() << "\n";
            path_result = VisitResult::Serious;
        }
        rc = VisitResultAggregator::Combine(rc, path_result);
    }

    renderer_.reset();
    scanner_.reset();
    config_ = nullptr;

    if (perf_manager.enabled()) {
        run_timer.reset();
        perf_manager.Report(std::cerr);
    }

    return static_cast<int>(rc);
}

int App::runDatabaseCommand(Config::DbAction action)
{
    std::vector<std::filesystem::path> candidates = ResourceManager::databaseCandidates();
    std::vector<std::filesystem::path> existing;
    existing.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            std::ifstream stream(candidate, std::ios::binary);
            if (!stream.is_open()) {
                continue;
            }
            existing.push_back(candidate);
        }
    }

    if (existing.empty()) {
        std::cerr << "nls: error: configuration database not found\n";
        return 1;
    }

    IconMap files;
    IconMap folders;
    AliasMap file_aliases;
    AliasMap folder_aliases;
    bool loaded_any = false;
    for (auto it = existing.rbegin(); it != existing.rend(); ++it) {
        loaded_any = MergeDatabase(*it, files, folders, file_aliases, folder_aliases) || loaded_any;
    }

    if (!loaded_any) {
        std::cerr << "nls: error: failed to load configuration database entries\n";
        return 1;
    }

    switch (action) {
        case Config::DbAction::ShowFiles: {
            auto rows = MakeSortedIcons(files);
            PrintIconRows(rows);
            break;
        }
        case Config::DbAction::ShowFolders: {
            auto rows = MakeSortedIcons(folders);
            PrintIconRows(rows);
            break;
        }
        case Config::DbAction::ShowFileAliases: {
            auto rows = MakeAliasOutput(file_aliases, files);
            PrintAliasRows(rows);
            break;
        }
        case Config::DbAction::ShowFolderAliases: {
            auto rows = MakeAliasOutput(folder_aliases, folders);
            PrintAliasRows(rows);
            break;
        }
        case Config::DbAction::None:
        default:
            return 0;
    }

    return 0;
}

}  // namespace nls
