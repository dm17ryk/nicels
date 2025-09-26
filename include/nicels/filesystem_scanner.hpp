#pragma once

#include "nicels/options.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nicels {

struct FileEntry {
    std::filesystem::path path;
    std::string display_name;
    std::filesystem::file_status status;
    std::filesystem::file_time_type last_write;
    std::uintmax_t size{0};
    std::uintmax_t hard_links{0};
    std::optional<std::uintmax_t> inode;
    std::string owner;
    std::string group;
    bool is_directory{false};
    bool is_symlink{false};
    bool is_hidden{false};
    bool is_executable{false};
    bool is_broken_symlink{false};
    std::string symlink_target;
};

struct DirectoryResult {
    std::filesystem::path source;
    bool is_directory{false};
    std::vector<FileEntry> entries;
    std::vector<DirectoryResult> children;
    std::optional<FileEntry> self;
};

class FilesystemScanner {
public:
    FilesystemScanner();

    [[nodiscard]] std::vector<DirectoryResult> scan(const Options& options) const;
};

} // namespace nicels
