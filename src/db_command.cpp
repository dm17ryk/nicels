#include "db_command.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#include <securitybaseapi.h>
#else
#include <unistd.h>
#endif

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

int DatabaseInspector::Execute(Config::DbAction action,
                               const Config::DbIconEntry& icon_entry,
                               const Config::DbAliasEntry& alias_entry)
{
    if (action == Config::DbAction::None) {
        return 0;
    }

    switch (action) {
        case Config::DbAction::ShowFiles:
        case Config::DbAction::ShowFolders:
        case Config::DbAction::ShowFileAliases:
        case Config::DbAction::ShowFolderAliases: {
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
                default:
                    break;
            }
            return 0;
        }
        case Config::DbAction::SetFile:
            return ApplyIconEntry(IconTarget::Files, icon_entry);
        case Config::DbAction::SetFolder:
            return ApplyIconEntry(IconTarget::Folders, icon_entry);
        case Config::DbAction::SetFileAlias:
            return ApplyAliasEntry(AliasTarget::Files, alias_entry);
        case Config::DbAction::SetFolderAlias:
            return ApplyAliasEntry(AliasTarget::Folders, alias_entry);
        case Config::DbAction::None:
        default:
            return 0;
    }
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
    SqliteDbPtr db(OpenDatabase(path, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, open_error));
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

bool DatabaseInspector::IsElevated()
{
#if defined(_WIN32)
    BOOL is_admin = FALSE;
    PSID admin_group = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(nullptr, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin == TRUE;
#else
    return geteuid() == 0;
#endif
}

bool DatabaseInspector::ExecuteSimple(sqlite3* db, std::string_view sql)
{
    std::string statement(sql);
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, statement.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "nls: error: " << (err_msg ? err_msg : sqlite3_errmsg(db)) << '\n';
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

namespace {

constexpr std::string_view kDatabaseFilename = "NLS.sqlite3";

} // namespace

std::filesystem::path DatabaseInspector::UserDatabasePath()
{
    std::filesystem::path dir = ResourceManager::userConfigDir();
    if (dir.empty()) {
        return {};
    }
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            std::cerr << "nls: error: unable to create user configuration directory '" << dir
                      << "': " << ec.message() << '\n';
            return {};
        }
    } else if (!std::filesystem::is_directory(dir, ec)) {
        if (!ec) {
            std::cerr << "nls: error: user configuration path '" << dir
                      << "' is not a directory\n";
        } else {
            std::cerr << "nls: error: unable to access '" << dir
                      << "': " << ec.message() << '\n';
        }
        return {};
    }
    return dir / std::filesystem::path(kDatabaseFilename);
}

std::filesystem::path DatabaseInspector::ResolveWritableDatabasePath(bool prefer_system) const
{
    if (!writable_cache_.empty()) {
        return writable_cache_;
    }

    auto env_override = ResourceManager::envOverrideDir();
    if (!env_override.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(env_override, ec)) {
            std::filesystem::create_directories(env_override, ec);
            if (ec) {
                std::cerr << "nls: error: unable to prepare environment override directory '"
                          << env_override << "': " << ec.message() << '\n';
                return {};
            }
        }
        writable_cache_ = env_override / std::filesystem::path(kDatabaseFilename);
        return writable_cache_;
    }

    if (prefer_system) {
        auto user_dir = ResourceManager::userConfigDir();
        for (const auto& candidate : ResourceManager::databaseCandidates()) {
            if (candidate.empty()) {
                continue;
            }
            std::error_code ec;
            if (!user_dir.empty()) {
                std::filesystem::path user_path = user_dir / std::filesystem::path(kDatabaseFilename);
                if (std::filesystem::equivalent(candidate, user_path, ec)) {
                    continue;
                }
            }
            auto parent = candidate.parent_path();
            if (!parent.empty()) {
                std::error_code create_ec;
                if (!std::filesystem::exists(parent, create_ec)) {
                    std::filesystem::create_directories(parent, create_ec);
                    if (create_ec) {
                        std::cerr << "nls: warning: unable to prepare directory '" << parent
                                  << "': " << create_ec.message() << '\n';
                        continue;
                    }
                }
            }
            writable_cache_ = candidate;
            return writable_cache_;
        }
    }

    auto user_path = UserDatabasePath();
    if (!user_path.empty()) {
        writable_cache_ = user_path;
        return writable_cache_;
    }

    for (const auto& candidate : ResourceManager::databaseCandidates()) {
        if (!candidate.empty()) {
            writable_cache_ = candidate;
            return writable_cache_;
        }
    }

    return {};
}

bool DatabaseInspector::EnsureSchema(sqlite3* db)
{
    static constexpr std::string_view statements[] = {
        "CREATE TABLE IF NOT EXISTS Files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL COLLATE NOCASE,"
        "description TEXT,"
        "used_by TEXT,"
        "icon TEXT,"
        "icon_class_name TEXT,"
        "Icon_UTF_16_codes INTEGER,"
        "Icon_Hex_Code INTEGER);",

        "CREATE TABLE IF NOT EXISTS Folders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL COLLATE NOCASE,"
        "description TEXT,"
        "used_by TEXT,"
        "icon TEXT,"
        "icon_class_name TEXT,"
        "Icon_UTF_16_codes INTEGER,"
        "Icon_Hex_Code INTEGER);",

        "CREATE TABLE IF NOT EXISTS File_Aliases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL COLLATE NOCASE,"
        "alias TEXT NOT NULL UNIQUE COLLATE NOCASE);",

        "CREATE TABLE IF NOT EXISTS Folder_Aliases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL COLLATE NOCASE,"
        "alias TEXT NOT NULL UNIQUE COLLATE NOCASE);"
    };

    for (auto statement : statements) {
        if (!ExecuteSimple(db, statement)) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<sqlite3, void(*)(sqlite3*)> DatabaseInspector::OpenDatabase(const std::filesystem::path& path, int flags, std::string& error)
{
    sqlite3* raw = nullptr;
    const std::string utf8 = PathToUtf8(path);
    int rc = sqlite3_open_v2(utf8.c_str(), &raw, flags, nullptr);
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

std::optional<std::uint32_t> DatabaseInspector::ParseUnicode(std::string_view text)
{
    std::string trimmed = StringUtils::Trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    std::string_view view = trimmed;
    if (view.rfind("\\u", 0) == 0 || view.rfind("\\U", 0) == 0) {
        view.remove_prefix(2);
    } else if (view.rfind("U+", 0) == 0) {
        view.remove_prefix(2);
    }
    if (view.rfind("0x", 0) == 0 || view.rfind("0X", 0) == 0) {
        view.remove_prefix(2);
    }

    std::uint32_t value = 0;
    auto result = std::from_chars(view.data(), view.data() + view.size(), value, 16);
    if (result.ec == std::errc{} && result.ptr == view.data() + view.size()) {
        return value;
    }

    try {
        size_t pos = 0;
        unsigned long parsed = std::stoul(std::string(view), &pos, 16);
        if (pos == view.size()) {
            return static_cast<std::uint32_t>(parsed);
        }
    } catch (const std::exception&) {
        // fall through
    }

    value = 0;
    result = std::from_chars(view.data(), view.data() + view.size(), value, 10);
    if (result.ec == std::errc{} && result.ptr == view.data() + view.size()) {
        return value;
    }

    try {
        size_t pos = 0;
        unsigned long parsed = std::stoul(std::string(view), &pos, 10);
        if (pos == view.size()) {
            return static_cast<std::uint32_t>(parsed);
        }
    } catch (const std::exception&) {
        // fall through
    }

    return std::nullopt;
}

std::optional<std::uint32_t> DatabaseInspector::ParseHex(std::string_view text)
{
    std::string trimmed = StringUtils::Trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    std::string_view view = trimmed;
    if (view.rfind("0x", 0) == 0 || view.rfind("0X", 0) == 0) {
        view.remove_prefix(2);
    }
    if (view.rfind("\\u", 0) == 0 || view.rfind("\\U", 0) == 0) {
        view.remove_prefix(2);
    }

    std::uint32_t value = 0;
    auto result = std::from_chars(view.data(), view.data() + view.size(), value, 16);
    if (result.ec == std::errc{} && result.ptr == view.data() + view.size()) {
        return value;
    }

    try {
        size_t pos = 0;
        unsigned long parsed = std::stoul(std::string(view), &pos, 16);
        if (pos == view.size()) {
            return static_cast<std::uint32_t>(parsed);
        }
    } catch (const std::exception&) {
        // fall through
    }

    value = 0;
    result = std::from_chars(view.data(), view.data() + view.size(), value, 10);
    if (result.ec == std::errc{} && result.ptr == view.data() + view.size()) {
        return value;
    }

    try {
        size_t pos = 0;
        unsigned long parsed = std::stoul(std::string(view), &pos, 10);
        if (pos == view.size()) {
            return static_cast<std::uint32_t>(parsed);
        }
    } catch (const std::exception&) {
        // fall through
    }

    return std::nullopt;
}

bool DatabaseInspector::AllFieldsEmpty(const Config::DbIconEntry& entry)
{
    return entry.icon.empty() && entry.icon_class.empty() && entry.icon_utf16.empty() &&
           entry.icon_hex.empty() && entry.description.empty() && entry.used_by.empty();
}

int DatabaseInspector::ApplyIconEntry(IconTarget target, const Config::DbIconEntry& entry)
{
    std::string name = StringUtils::Trim(entry.name);
    if (name.empty()) {
        std::cerr << "nls: error: --name must be specified\n";
        return 1;
    }

    bool prefer_system = IsElevated();
    std::filesystem::path db_path = ResolveWritableDatabasePath(prefer_system);
    if (db_path.empty()) {
        std::cerr << "nls: error: unable to determine writable database location\n";
        return 1;
    }

    std::string open_error;
    SqliteDbPtr db(OpenDatabase(db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, open_error));
    if (!db) {
        if (!open_error.empty()) {
            std::cerr << open_error << '\n';
        }
        return 1;
    }

    if (!EnsureSchema(db.get())) {
        return 1;
    }

    const bool remove = AllFieldsEmpty(entry);

    auto unicode_value = ParseUnicode(entry.icon_utf16);
    if (!entry.icon_utf16.empty() && !unicode_value) {
        std::cerr << "nls: error: invalid --icon_utf_16_codes value '" << entry.icon_utf16 << "'\n";
        return 1;
    }

    auto hex_value = ParseHex(entry.icon_hex);
    if (!entry.icon_hex.empty() && !hex_value) {
        std::cerr << "nls: error: invalid --icon_hex_code value '" << entry.icon_hex << "'\n";
        return 1;
    }

    if (!ExecuteSimple(db.get(), "BEGIN IMMEDIATE TRANSACTION;")) {
        return 1;
    }

    auto rollback = [&]() {
        (void)ExecuteSimple(db.get(), "ROLLBACK;");
    };

    int rc = SQLITE_OK;
    const bool is_file = (target == IconTarget::Files);
    std::string table = is_file ? "Files" : "Folders";
    std::string context = is_file ? "file" : "folder";
    if (remove) {
        std::string sql = "DELETE FROM " + table + " WHERE LOWER(name) = LOWER(?1);";
        sqlite3_stmt* stmt_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr stmt(stmt_raw, &FinalizeSqlite);
        sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        int changes = sqlite3_changes(db.get());
        if (!ExecuteSimple(db.get(), "COMMIT;")) {
            (void)ExecuteSimple(db.get(), "ROLLBACK;");
            return 1;
        }
        if (changes > 0) {
            std::cout << "nls: removed " << context << " icon entry '" << name << "'\n";
        } else {
            std::cout << "nls: no existing " << context << " icon entry for '" << name
                      << "' to remove\n";
        }
        return 0;
    }

    std::string lowered_name = StringUtils::ToLower(name);

    std::optional<sqlite3_int64> existing_id;
    {
        std::string lookup_sql = "SELECT id FROM " + table + " WHERE LOWER(name) = LOWER(?1) LIMIT 1;";
        sqlite3_stmt* lookup_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), lookup_sql.c_str(), -1, &lookup_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr lookup_stmt(lookup_raw, &FinalizeSqlite);
        sqlite3_bind_text(lookup_stmt.get(), 1, lowered_name.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(lookup_stmt.get());
        if (rc == SQLITE_ROW) {
            existing_id = sqlite3_column_int64(lookup_stmt.get(), 0);
        } else if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
    }

    auto bind_common_fields = [&](sqlite3_stmt* stmt, int name_index) {
        sqlite3_bind_text(stmt, name_index, lowered_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, name_index + 1, entry.icon.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, name_index + 2, entry.icon_class.c_str(), -1, SQLITE_TRANSIENT);
        if (unicode_value) {
            sqlite3_bind_int64(stmt, name_index + 3, static_cast<sqlite3_int64>(*unicode_value));
        } else {
            sqlite3_bind_null(stmt, name_index + 3);
        }
        if (hex_value) {
            sqlite3_bind_int64(stmt, name_index + 4, static_cast<sqlite3_int64>(*hex_value));
        } else {
            sqlite3_bind_null(stmt, name_index + 4);
        }
        sqlite3_bind_text(stmt, name_index + 5, entry.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, name_index + 6, entry.used_by.c_str(), -1, SQLITE_TRANSIENT);
    };

    if (existing_id) {
        std::string update_sql =
            "UPDATE " + table +
            " SET name=?1, icon=?2, icon_class_name=?3, Icon_UTF_16_codes=?4, Icon_Hex_Code=?5, "
            "description=?6, used_by=?7 WHERE id=?8;";
        sqlite3_stmt* update_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), update_sql.c_str(), -1, &update_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr update_stmt(update_raw, &FinalizeSqlite);
        bind_common_fields(update_stmt.get(), 1);
        sqlite3_bind_int64(update_stmt.get(), 8, *existing_id);
        rc = sqlite3_step(update_stmt.get());
        if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        if (!ExecuteSimple(db.get(), "COMMIT;")) {
            (void)ExecuteSimple(db.get(), "ROLLBACK;");
            return 1;
        }
        std::cout << "nls: updated " << context << " icon entry '" << name << "'\n";
        return 0;
    }

    sqlite3_int64 new_id = 1;
    {
        std::string next_sql = "SELECT COALESCE(MAX(id), 0) + 1 FROM " + table + ";";
        sqlite3_stmt* next_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), next_sql.c_str(), -1, &next_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr next_stmt(next_raw, &FinalizeSqlite);
        rc = sqlite3_step(next_stmt.get());
        if (rc == SQLITE_ROW) {
            new_id = sqlite3_column_int64(next_stmt.get(), 0);
        } else if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
    }

    std::string insert_sql =
        "INSERT INTO " + table +
        "(id, name, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code, description, used_by) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);";

    sqlite3_stmt* insert_raw = nullptr;
    rc = sqlite3_prepare_v2(db.get(), insert_sql.c_str(), -1, &insert_raw, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
        rollback();
        return 1;
    }
    SqliteStmtPtr insert_stmt(insert_raw, &FinalizeSqlite);
    sqlite3_bind_int64(insert_stmt.get(), 1, new_id);
    bind_common_fields(insert_stmt.get(), 2);
    rc = sqlite3_step(insert_stmt.get());
    if (rc != SQLITE_DONE) {
        std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
        rollback();
        return 1;
    }

    if (!ExecuteSimple(db.get(), "COMMIT;")) {
        (void)ExecuteSimple(db.get(), "ROLLBACK;");
        return 1;
    }

    std::cout << "nls: updated " << context << " icon entry '" << name << "'\n";
    return 0;
}

int DatabaseInspector::ApplyAliasEntry(AliasTarget target, const Config::DbAliasEntry& entry)
{
    std::string name = StringUtils::Trim(entry.name);
    if (name.empty()) {
        std::cerr << "nls: error: --name must be specified\n";
        return 1;
    }

    bool prefer_system = IsElevated();
    std::filesystem::path db_path = ResolveWritableDatabasePath(prefer_system);
    if (db_path.empty()) {
        std::cerr << "nls: error: unable to determine writable database location\n";
        return 1;
    }

    std::string open_error;
    SqliteDbPtr db(OpenDatabase(db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, open_error));
    if (!db) {
        if (!open_error.empty()) {
            std::cerr << open_error << '\n';
        }
        return 1;
    }

    if (!EnsureSchema(db.get())) {
        return 1;
    }

    if (!ExecuteSimple(db.get(), "BEGIN IMMEDIATE TRANSACTION;")) {
        return 1;
    }

    auto rollback = [&]() {
        (void)ExecuteSimple(db.get(), "ROLLBACK;");
    };

    const bool is_file_alias = (target == AliasTarget::Files);
    std::string table = is_file_alias ? "File_Aliases" : "Folder_Aliases";
    std::string context = is_file_alias ? "file" : "folder";
    int rc = SQLITE_OK;
    if (entry.alias.empty()) {
        std::string sql = "DELETE FROM " + table + " WHERE LOWER(name) = LOWER(?1);";
        sqlite3_stmt* stmt_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr stmt(stmt_raw, &FinalizeSqlite);
        sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        int changes = sqlite3_changes(db.get());
        if (!ExecuteSimple(db.get(), "COMMIT;")) {
            (void)ExecuteSimple(db.get(), "ROLLBACK;");
            return 1;
        }
        if (changes > 0) {
            std::cout << "nls: removed " << context << " alias for '" << name << "'\n";
        } else {
            std::cout << "nls: no existing " << context << " alias for '" << name
                      << "' to remove\n";
        }
        return 0;
    }

    std::string alias_value = StringUtils::Trim(entry.alias);
    if (alias_value.empty()) {
        std::cerr << "nls: error: --alias must not be empty (use --alias \"\" to remove)\n";
        rollback();
        return 1;
    }

    std::string lowered_name = StringUtils::ToLower(name);
    std::string lowered_alias = StringUtils::ToLower(alias_value);

    std::optional<sqlite3_int64> existing_id;
    {
        std::string lookup_sql = "SELECT id FROM " + table + " WHERE LOWER(alias) = LOWER(?1) LIMIT 1;";
        sqlite3_stmt* lookup_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), lookup_sql.c_str(), -1, &lookup_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr lookup_stmt(lookup_raw, &FinalizeSqlite);
        sqlite3_bind_text(lookup_stmt.get(), 1, lowered_alias.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(lookup_stmt.get());
        if (rc == SQLITE_ROW) {
            existing_id = sqlite3_column_int64(lookup_stmt.get(), 0);
        } else if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
    }

    if (existing_id) {
        std::string update_sql = "UPDATE " + table + " SET name=?1, alias=?2 WHERE id=?3;";
        sqlite3_stmt* update_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), update_sql.c_str(), -1, &update_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr update_stmt(update_raw, &FinalizeSqlite);
        sqlite3_bind_text(update_stmt.get(), 1, lowered_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt.get(), 2, lowered_alias.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(update_stmt.get(), 3, *existing_id);
        rc = sqlite3_step(update_stmt.get());
        if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        if (!ExecuteSimple(db.get(), "COMMIT;")) {
            (void)ExecuteSimple(db.get(), "ROLLBACK;");
            return 1;
        }
        std::cout << "nls: updated " << context << " alias '" << alias_value
                  << "' -> '" << name << "'\n";
        return 0;
    }

    sqlite3_int64 new_id = 1;
    {
        std::string next_sql = "SELECT COALESCE(MAX(id), 0) + 1 FROM " + table + ";";
        sqlite3_stmt* next_raw = nullptr;
        rc = sqlite3_prepare_v2(db.get(), next_sql.c_str(), -1, &next_raw, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
        SqliteStmtPtr next_stmt(next_raw, &FinalizeSqlite);
        rc = sqlite3_step(next_stmt.get());
        if (rc == SQLITE_ROW) {
            new_id = sqlite3_column_int64(next_stmt.get(), 0);
        } else if (rc != SQLITE_DONE) {
            std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
            rollback();
            return 1;
        }
    }

    std::string insert_sql = "INSERT INTO " + table + "(id, name, alias) VALUES(?1, ?2, ?3);";
    sqlite3_stmt* insert_raw = nullptr;
    rc = sqlite3_prepare_v2(db.get(), insert_sql.c_str(), -1, &insert_raw, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
        rollback();
        return 1;
    }
    SqliteStmtPtr insert_stmt(insert_raw, &FinalizeSqlite);
    sqlite3_bind_int64(insert_stmt.get(), 1, new_id);
    sqlite3_bind_text(insert_stmt.get(), 2, lowered_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt.get(), 3, lowered_alias.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(insert_stmt.get());
    if (rc != SQLITE_DONE) {
        std::cerr << "nls: error: " << sqlite3_errmsg(db.get()) << '\n';
        rollback();
        return 1;
    }

    if (!ExecuteSimple(db.get(), "COMMIT;")) {
        (void)ExecuteSimple(db.get(), "ROLLBACK;");
        return 1;
    }

    std::cout << "nls: updated " << context << " alias '" << alias_value
              << "' -> '" << name << "'\n";
    return 0;
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
