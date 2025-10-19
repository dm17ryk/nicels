#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sqlite3.h>

#include "config.h"

namespace nls {

class DatabaseInspector final {
public:
    static DatabaseInspector CreateFromResourceManager();

    explicit DatabaseInspector(std::vector<std::filesystem::path> candidates);
    int Execute(Config::DbAction action);

private:
    struct IconRecord {
        std::string name;
        std::string icon;
        std::string icon_class;
        std::optional<std::uint32_t> icon_utf16;
        std::optional<std::uint32_t> icon_hex;
        std::string description;
        std::string used_by;
    };

    struct AliasRecord {
        std::string alias;
        std::string target_name;
        std::string target_key;
    };

    struct AliasPresentation {
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

    using IconMap = std::unordered_map<std::string, IconRecord>;
    using AliasMap = std::unordered_map<std::string, AliasRecord>;

    [[nodiscard]] bool EnsureLoaded();
    [[nodiscard]] bool LoadAllSources();
    [[nodiscard]] bool LoadSingleSource(const std::filesystem::path& path);

    [[nodiscard]] static std::vector<std::filesystem::path>
        FilterReadable(std::vector<std::filesystem::path> paths);

    [[nodiscard]] static std::unique_ptr<sqlite3, void(*)(sqlite3*)>
        OpenDatabase(const std::filesystem::path& path, std::string& error);
    [[nodiscard]] static std::optional<std::string>
        LoadIconTable(sqlite3* db, std::string_view sql, IconMap& target);
    [[nodiscard]] static std::optional<std::string>
        LoadAliasTable(sqlite3* db, std::string_view sql, AliasMap& target);

    [[nodiscard]] static std::string ExtractText(sqlite3_stmt* stmt, int column);
    [[nodiscard]] static std::optional<std::uint32_t> ExtractCode(sqlite3_stmt* stmt, int column);

    [[nodiscard]] static std::string FormatUtf16(std::optional<std::uint32_t> code);
    [[nodiscard]] static std::string FormatHex(std::optional<std::uint32_t> code);

    void PrintIcons(const IconMap& icons) const;
    void PrintAliases(const AliasMap& aliases, const IconMap& icons) const;

    std::vector<std::filesystem::path> candidates_;
    IconMap files_;
    IconMap folders_;
    AliasMap file_aliases_;
    AliasMap folder_aliases_;
    bool loaded_ = false;
    bool had_error_ = false;
    std::string last_error_;
};

}  // namespace nls
