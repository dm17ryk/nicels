#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nicels {

struct FileEntry {
    std::filesystem::path path;
    std::filesystem::path display_path;
    std::filesystem::file_status status;
    uintmax_t size{0};
    std::filesystem::file_time_type mtime{};
    std::optional<uintmax_t> inode;
    std::optional<std::string> owner;
    std::optional<std::string> group;
    bool is_directory{false};
    bool is_symlink{false};
    bool is_regular{false};
    bool hidden{false};
};

class FileSystemScanner {
public:
    FileSystemScanner();

    std::vector<FileEntry> scan(const std::filesystem::path& path, bool follow_symlinks, std::error_code& ec) const;
    FileEntry stat_path(const std::filesystem::path& path, bool follow_symlinks, std::error_code& ec) const;

private:
    FileEntry make_entry(const std::filesystem::directory_entry& entry, bool follow_symlinks, std::error_code& ec) const;
};

} // namespace nicels
