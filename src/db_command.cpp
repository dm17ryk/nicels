#include "db_command.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sstream>

#include "resources.h"
#include "string_utils.h"

namespace nls {

namespace {

void CloseSqlite(sqlite3* db)
{
    if (db) {
        sqlite3_close(db);
    }
}

void FinalizeSqlite(sqlite3_stmt* stmt)
{
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

using SqliteDbPtr = std::unique_ptr<sqlite3, decltype(&CloseSqlite)>;
using SqliteStmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&FinalizeSqlite)>;

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

}  // namespace

DatabaseInspector DatabaseInspector::CreateFromResourceManager()
{
    return DatabaseInspector(FilterReadable(ResourceManager::databaseCandidates()));
}

DatabaseInspector::DatabaseInspector(std::vector<std::filesystem::path> candidates)
    : candidates_(std::move(candidates))
{}

int DatabaseInspector::Execute(Config::DbAction action)
{
    if (action == Config::DbAction::None) {
        return 0;
    }

    if (!EnsureLoaded()) {
        if (!had_error_) {
            std::cerr << "nls: error: configuration database not found\n";
        } else if (!last_error_.empty()) {
            std::cerr << last_error_ << '\n';
        } else {
            std::cerr << "nls: error: failed to load configuration database entries\n";
        }
        return 1;
    }

    switch (action) {
        case Config::DbAction::ShowFiles:
            PrintIcons(files_);
            break;
        case Config::DbAction::ShowFolders:
            PrintIcons(folders_);
            break;
        case Config::DbAction::ShowFileAliases:
            PrintAliases(file_aliases_, files_);
            break;
        case Config::DbAction::ShowFolderAliases:
            PrintAliases(folder_aliases_, folders_);
            break;
        case Config::DbAction::None:
        default:
            break;
    }
    return 0;
}

bool DatabaseInspector::EnsureLoaded()
{
    if (loaded_) {
        return true;
    }
    if (candidates_.empty()) {
        had_error_ = false;
        return false;
    }
    loaded_ = LoadAllSources();
    return loaded_;
}

bool DatabaseInspector::LoadAllSources()
{
    bool any_loaded = false;

    for (auto it = candidates_.rbegin(); it != candidates_.rend(); ++it) {
        any_loaded = LoadSingleSource(*it) || any_loaded;
    }

    return any_loaded;
}

bool DatabaseInspector::LoadSingleSource(const std::filesystem::path& path)
{
    std::string open_error;
    SqliteDbPtr db(OpenDatabase(path, open_error));
    if (!db) {
        had_error_ = true;
        last_error_ = std::move(open_error);
        if (!last_error_.empty()) {
            std::cerr << last_error_ << '\n';
        }
        return false;
    }

    bool loaded = false;
    bool path_had_error = false;
    std::string path_error_message;

    auto handle_result = [&](std::optional<std::string> result, std::string_view section) {
        if (!result) {
            loaded = true;
            return;
        }
        if (!path_had_error) {
            std::ostringstream oss;
            oss << section << " - " << *result;
            path_error_message = oss.str();
        }
        path_had_error = true;
    };

    handle_result(LoadIconTable(db.get(),
        "SELECT name, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code, description, used_by "
        "FROM Files;",
        files_), "Files table");
    handle_result(LoadIconTable(db.get(),
        "SELECT name, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code, description, used_by "
        "FROM Folders;",
        folders_), "Folders table");
    handle_result(LoadAliasTable(db.get(),
        "SELECT alias, name FROM File_Aliases;",
        file_aliases_), "File_Aliases table");
    handle_result(LoadAliasTable(db.get(),
        "SELECT alias, name FROM Folder_Aliases;",
        folder_aliases_), "Folder_Aliases table");

    if (path_had_error) {
        had_error_ = true;
        std::ostringstream oss;
        oss << "nls: warning: unable to read complete configuration database '" << path
            << "': " << path_error_message;
        last_error_ = oss.str();
        std::cerr << last_error_ << '\n';
    }

    return loaded;
}

std::vector<std::filesystem::path> DatabaseInspector::FilterReadable(std::vector<std::filesystem::path> paths)
{
    std::vector<std::filesystem::path> readable;
    readable.reserve(paths.size());
    for (const auto& path : paths) {
        if (path.empty()) {
            continue;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            continue;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            continue;
        }
        readable.push_back(path);
    }
    return readable;
}

std::unique_ptr<sqlite3, void(*)(sqlite3*)> DatabaseInspector::OpenDatabase(const std::filesystem::path& path, std::string& error)
{
    sqlite3* raw = nullptr;
    const std::string utf8 = PathToUtf8(path);
    int rc = sqlite3_open_v2(utf8.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        if (raw) {
            std::ostringstream oss;
            oss << "nls: warning: failed to open config database '" << path << "': "
                << sqlite3_errmsg(raw);
            error = oss.str();
            sqlite3_close(raw);
        } else {
            std::ostringstream oss;
            oss << "nls: warning: failed to open config database '" << path << "': "
                << sqlite3_errstr(rc);
            error = oss.str();
        }
        return {nullptr, &CloseSqlite};
    }
    return {raw, &CloseSqlite};
}

std::optional<std::string> DatabaseInspector::LoadIconTable(sqlite3* db, std::string_view sql, IconMap& target)
{
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        return sqlite3_errmsg(db);
    }

    SqliteStmtPtr stmt(stmt_raw, &FinalizeSqlite);
    sqlite3_stmt* handle = stmt.get();

    while ((rc = sqlite3_step(handle)) == SQLITE_ROW) {
        IconRecord record;
        record.name = ExtractText(handle, 0);
        if (record.name.empty()) {
            continue;
        }
        record.icon = ExtractText(handle, 1);
        record.icon_class = ExtractText(handle, 2);
        record.icon_utf16 = ExtractCode(handle, 3);
        record.icon_hex = ExtractCode(handle, 4);
        record.description = ExtractText(handle, 5);
        record.used_by = ExtractText(handle, 6);

        std::string key = StringUtils::ToLower(record.name);
        target[std::move(key)] = std::move(record);
    }

    if (rc != SQLITE_DONE) {
        return sqlite3_errmsg(db);
    }

    return std::nullopt;
}

std::optional<std::string> DatabaseInspector::LoadAliasTable(sqlite3* db, std::string_view sql, AliasMap& target)
{
    sqlite3_stmt* stmt_raw = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_raw, nullptr);
    if (rc != SQLITE_OK) {
        return sqlite3_errmsg(db);
    }

    SqliteStmtPtr stmt(stmt_raw, &FinalizeSqlite);
    sqlite3_stmt* handle = stmt.get();

    while ((rc = sqlite3_step(handle)) == SQLITE_ROW) {
        AliasRecord record;
        record.alias = ExtractText(handle, 0);
        record.target_name = ExtractText(handle, 1);
        if (record.alias.empty() || record.target_name.empty()) {
            continue;
        }
        record.target_key = StringUtils::ToLower(record.target_name);

        std::string alias_key = StringUtils::ToLower(record.alias);
        target[std::move(alias_key)] = std::move(record);
    }

    if (rc != SQLITE_DONE) {
        return sqlite3_errmsg(db);
    }

    return std::nullopt;
}

std::string DatabaseInspector::FormatUtf16(std::optional<std::uint32_t> code)
{
    if (!code) {
        return {};
    }
    std::ostringstream oss;
    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (*code & 0xFFFFu);
    return oss.str();
}

std::string DatabaseInspector::FormatHex(std::optional<std::uint32_t> code)
{
    if (!code) {
        return {};
    }
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << (*code & 0xFFFFu);
    return oss.str();
}

std::string DatabaseInspector::ExtractText(sqlite3_stmt* stmt, int column)
{
    const unsigned char* data = sqlite3_column_text(stmt, column);
    if (!data) {
        return {};
    }
    return reinterpret_cast<const char*>(data);
}

std::optional<std::uint32_t> DatabaseInspector::ExtractCode(sqlite3_stmt* stmt, int column)
{
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(sqlite3_column_int64(stmt, column));
}

void DatabaseInspector::PrintIcons(const IconMap& icons) const
{
    std::vector<const IconRecord*> records;
    records.reserve(icons.size());
    for (const auto& [_, record] : icons) {
        records.push_back(&record);
    }

    std::sort(records.begin(), records.end(), [](const IconRecord* lhs, const IconRecord* rhs) {
        return lhs->name < rhs->name;
    });

    std::cout << "name\ticon\ticon_class\tIcon_UTF_16_codes\tIcon_Hex_Code\tdescription\tused_by\n";

    for (const IconRecord* record : records) {
        std::cout << record->name << '\t'
                  << record->icon << '\t'
                  << record->icon_class << '\t'
                  << FormatUtf16(record->icon_utf16) << '\t'
                  << FormatHex(record->icon_hex) << '\t'
                  << record->description << '\t'
                  << record->used_by << '\n';
    }
}

void DatabaseInspector::PrintAliases(const AliasMap& aliases, const IconMap& icons) const
{
    std::vector<AliasPresentation> rows;
    rows.reserve(aliases.size());

    for (const auto& [_, alias] : aliases) {
        AliasPresentation row;
        row.name = alias.target_name;
        row.alias = alias.alias;

        if (const auto icon_it = icons.find(alias.target_key); icon_it != icons.end()) {
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

    std::ranges::sort(rows, [](const AliasPresentation& lhs, const AliasPresentation& rhs) {
        if (lhs.name == rhs.name) {
            return lhs.alias < rhs.alias;
        }
        return lhs.name < rhs.name;
    });

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

}  // namespace nls
