#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nicels/config.h"

namespace nicels {

struct FileEntry {
    enum class Type {
        Directory,
        Regular,
        Symlink,
        Block,
        Character,
        Fifo,
        Socket,
        Unknown
    };

    std::filesystem::path path;
    std::filesystem::path display_path;
    std::filesystem::path link_target;
    std::filesystem::path root_path;
    Type type = Type::Unknown;
    std::filesystem::perms permissions = std::filesystem::perms::unknown;
    std::uintmax_t size = 0;
    std::uintmax_t allocated_size = 0;
    std::uintmax_t inode = 0;
    std::uintmax_t hardlink_count = 1;
    std::uintmax_t owner_id = 0;
    std::uintmax_t group_id = 0;
    bool has_allocated_size = false;
    bool has_inode = false;
    bool owner_available = false;
    bool group_available = false;
    bool is_hidden = false;
    bool is_dot = false;
    bool is_broken_symlink = false;
    bool executable = false;
    std::filesystem::file_time_type modified{};
    std::string owner;
    std::string group;
    std::string git_status;
    std::string icon_key;
    std::size_t depth = 0;
    bool last_sibling = false;
};

class FileSystemScanner {
public:
    explicit FileSystemScanner(const Config::Options& options);

    std::vector<FileEntry> collect(const std::vector<std::filesystem::path>& paths);

private:
    struct StatResult {
        std::uintmax_t size = 0;
        std::uintmax_t allocated = 0;
        std::uintmax_t inode = 0;
        std::uintmax_t hard_links = 1;
        std::uintmax_t uid = 0;
        std::uintmax_t gid = 0;
        bool has_inode = false;
        bool has_allocated = false;
        bool owner_available = false;
        bool group_available = false;
        std::string owner;
        std::string group;
    };

    bool should_include(const std::filesystem::directory_entry& entry) const;
    std::optional<FileEntry> make_entry(const std::filesystem::directory_entry& entry,
                                        const std::filesystem::path& root,
                                        std::size_t depth) const;
    std::optional<StatResult> stat_path(const std::filesystem::path& path, bool follow_symlink) const;
    bool is_hidden(const std::filesystem::path& path) const;
    bool is_backup_file(const std::filesystem::path& path) const;
    std::string icon_for(const FileEntry& entry) const;
    void walk_directory(const std::filesystem::path& path, std::size_t depth,
                        std::vector<FileEntry>& out) const;

    const Config::Options& options_;
};

} // namespace nicels
