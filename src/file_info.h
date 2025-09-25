#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nls {

class FileInfo {
public:
    FileInfo() = default;

    std::filesystem::path path;
    std::string name;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_exec = false;
    bool is_hidden = false;
    bool is_broken_symlink = false;
    bool is_socket = false;
    bool is_block_device = false;
    bool is_char_device = false;
    bool has_recognized_icon = false;
    uintmax_t inode = 0;
    uintmax_t size = 0;
    std::filesystem::file_time_type mtime{};
    std::filesystem::path symlink_target;
    bool has_symlink_target = false;
    uintmax_t owner_id = 0;
    uintmax_t group_id = 0;
    bool has_owner_id = false;
    bool has_group_id = false;
    std::string owner_numeric;
    std::string group_numeric;
    bool has_owner_numeric = false;
    bool has_group_numeric = false;
    uintmax_t link_size = 0;
    bool has_link_size = false;
    uintmax_t allocated_size = 0;
    bool has_allocated_size = false;
#ifdef _WIN32
    unsigned long nlink = 1;
    std::string owner = "";
    std::string group = "";
#else
    unsigned long nlink = 1;
    std::string owner;
    std::string group;
#endif
    std::string icon;
    std::string color_fg;
    std::string color_reset;
    std::string git_prefix;
};

} // namespace nls
