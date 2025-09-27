#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace nicels {

struct GitFileStatus {
    bool staged { false };
    bool unstaged { false };
    bool untracked { false };
};

struct FileEntry {
    std::filesystem::path path;
    std::string name;
    std::filesystem::file_status status;
    std::filesystem::file_time_type last_write_time;
    std::uintmax_t size { 0 };
    bool is_directory { false };
    bool is_symlink { false };
    bool is_hidden { false };
    GitFileStatus git;
};

} // namespace nicels
