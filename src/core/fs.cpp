#include "nicels/fs.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <system_error>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#include "nicels/logger.h"

namespace nicels {
namespace {
bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}
} // namespace

FileSystemScanner::FileSystemScanner(const Config::Options& options)
    : options_{options} {}

std::vector<FileEntry> FileSystemScanner::collect(const std::vector<std::filesystem::path>& paths) {
    std::vector<std::filesystem::path> effective_paths = paths;
    if (effective_paths.empty()) {
        effective_paths.emplace_back(".");
    }

    std::vector<FileEntry> entries;
    entries.reserve(256);

    for (const auto& input_path : effective_paths) {
        std::error_code ec;
        std::filesystem::directory_entry entry{input_path, ec};
        if (ec) {
            Logger::instance().warn("failed to stat {}: {}", input_path.string(), ec.message());
            continue;
        }

        bool is_dir = entry.is_directory(ec);
        if (!ec && is_dir && !options_.files_only) {
            if (!options_.tree) {
                walk_directory(entry.path(), 0, entries);
            } else {
                // include the directory itself when tree mode is enabled
                if (auto self = make_entry(entry, entry.path(), 0)) {
                    entries.push_back(*self);
                    entries.back().depth = 0;
                    entries.back().last_sibling = true;
                }
                if (!options_.tree_depth || *options_.tree_depth > 0) {
                    walk_directory(entry.path(), 1, entries);
                }
            }
        } else {
            if (auto single = make_entry(entry, entry.path().parent_path(), 0)) {
                entries.push_back(*single);
            }
        }
    }

    return entries;
}

bool FileSystemScanner::should_include(const std::filesystem::directory_entry& entry) const {
    const auto name = entry.path().filename();
    const std::string name_str = name.string();
    const bool is_dot_entry = name == "." || name == "..";

    if (options_.directories_only && !entry.is_directory()) {
        return false;
    }
    if (options_.files_only && entry.is_directory()) {
        return false;
    }
    if (options_.ignore_backups && is_backup_file(entry.path())) {
        return false;
    }

    if (options_.show_all) {
        return true;
    }

    if (is_dot_entry) {
        return options_.show_almost_all;
    }

    if (!options_.show_almost_all && is_hidden(entry.path())) {
        return false;
    }

    if (!options_.hide_patterns.empty() || !options_.ignore_patterns.empty()) {
        auto lower_name = name_str;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        for (const auto& pattern : options_.hide_patterns) {
            if (equals_ignore_case(lower_name, pattern)) {
                return false;
            }
        }
        for (const auto& pattern : options_.ignore_patterns) {
            if (equals_ignore_case(lower_name, pattern)) {
                return false;
            }
        }
    }

    return true;
}

std::optional<FileEntry> FileSystemScanner::make_entry(const std::filesystem::directory_entry& entry,
                                                       const std::filesystem::path& root,
                                                       std::size_t depth) const {
    std::error_code ec;
    FileEntry result;
    result.path = entry.path();
    result.display_path = entry.path().filename();
    result.root_path = root;
    result.depth = depth;
    result.is_dot = result.display_path == "." || result.display_path == "..";

    const bool is_symlink = entry.is_symlink(ec);
    if (is_symlink) {
        result.type = FileEntry::Type::Symlink;
        std::error_code link_ec;
        result.link_target = std::filesystem::read_symlink(entry.path(), link_ec);
        if (!link_ec) {
            std::filesystem::path resolved = entry.path().parent_path() / result.link_target;
            std::error_code follow_ec;
            if (!std::filesystem::exists(resolved, follow_ec)) {
                result.is_broken_symlink = true;
            }
        }
    } else if (entry.is_directory(ec)) {
        result.type = FileEntry::Type::Directory;
    } else if (entry.is_regular_file(ec)) {
        result.type = FileEntry::Type::Regular;
    } else if (entry.status(ec).type() == std::filesystem::file_type::block) {
        result.type = FileEntry::Type::Block;
    } else if (entry.status(ec).type() == std::filesystem::file_type::character) {
        result.type = FileEntry::Type::Character;
    } else if (entry.status(ec).type() == std::filesystem::file_type::fifo) {
        result.type = FileEntry::Type::Fifo;
    } else if (entry.status(ec).type() == std::filesystem::file_type::socket) {
        result.type = FileEntry::Type::Socket;
    } else {
        result.type = FileEntry::Type::Unknown;
    }

    const bool follow_symlink = options_.dereference && result.type == FileEntry::Type::Symlink;
    if (auto stat = stat_path(entry.path(), follow_symlink)) {
        result.size = stat->size;
        result.allocated_size = stat->allocated;
        result.has_allocated_size = stat->has_allocated;
        result.inode = stat->inode;
        result.has_inode = stat->has_inode;
        result.hardlink_count = stat->hard_links;
        result.owner_id = stat->uid;
        result.group_id = stat->gid;
        result.owner = stat->owner;
        result.group = stat->group;
        result.owner_available = stat->owner_available;
        result.group_available = stat->group_available;
    }

    result.permissions = entry.symlink_status().permissions();
    result.modified = entry.last_write_time(ec);
    result.is_hidden = is_hidden(result.path);

    if (!follow_symlink && result.type == FileEntry::Type::Symlink) {
        std::error_code target_ec;
        auto status = std::filesystem::status(entry.path(), target_ec);
        if (!target_ec) {
            if (status.type() == std::filesystem::file_type::directory) {
                result.type = FileEntry::Type::Directory;
            } else if (status.type() == std::filesystem::file_type::regular) {
                result.type = FileEntry::Type::Regular;
            }
        }
    }

    result.icon_key = icon_for(result);
    const auto exec_mask = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                           std::filesystem::perms::others_exec;
    result.executable = (result.permissions & exec_mask) != std::filesystem::perms::none;

    return result;
}

std::optional<FileSystemScanner::StatResult> FileSystemScanner::stat_path(const std::filesystem::path& path,
                                                                          bool follow_symlink) const {
#ifndef _WIN32
    struct stat st {};
    const int rc = follow_symlink ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st);
    if (rc != 0) {
        return std::nullopt;
    }

    StatResult result;
    result.size = static_cast<std::uintmax_t>(st.st_size);
    result.hard_links = static_cast<std::uintmax_t>(st.st_nlink);
    result.inode = static_cast<std::uintmax_t>(st.st_ino);
    result.has_inode = true;
    result.uid = static_cast<std::uintmax_t>(st.st_uid);
    result.gid = static_cast<std::uintmax_t>(st.st_gid);

    if (st.st_blocks >= 0) {
        result.allocated = static_cast<std::uintmax_t>(st.st_blocks) * 512u;
        result.has_allocated = true;
    }

    if (auto* pw = ::getpwuid(st.st_uid)) {
        result.owner = pw->pw_name ? pw->pw_name : "";
        result.owner_available = true;
    } else {
        result.owner = std::to_string(result.uid);
        result.owner_available = true;
    }

    if (auto* gr = ::getgrgid(st.st_gid)) {
        result.group = gr->gr_name ? gr->gr_name : "";
        result.group_available = true;
    } else {
        result.group = std::to_string(result.gid);
        result.group_available = true;
    }

    return result;
#else
    struct _stat64 st {};
    if ((follow_symlink ? ::_wstat64 : ::_wstat64)(path.c_str(), &st) != 0) {
        return std::nullopt;
    }
    StatResult result;
    result.size = static_cast<std::uintmax_t>(st.st_size);
    result.hard_links = static_cast<std::uintmax_t>(st.st_nlink);
    result.inode = static_cast<std::uintmax_t>(st.st_ino);
    result.has_inode = true;
    result.uid = static_cast<std::uintmax_t>(st.st_uid);
    result.gid = static_cast<std::uintmax_t>(st.st_gid);
    result.owner = std::to_string(result.uid);
    result.group = std::to_string(result.gid);
    result.owner_available = true;
    result.group_available = true;

    HANDLE file = ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info{};
        if (::GetFileInformationByHandle(file, &info)) {
            ULARGE_INTEGER size{};
            size.HighPart = info.nFileSizeHigh;
            size.LowPart = info.nFileSizeLow;
            result.allocated = static_cast<std::uintmax_t>(size.QuadPart);
            result.has_allocated = true;
        }
        ::CloseHandle(file);
    }
    return result;
#endif
}

bool FileSystemScanner::is_hidden(const std::filesystem::path& path) const {
    auto filename = path.filename().string();
    if (filename.empty()) {
        return false;
    }
#ifndef _WIN32
    return filename.front() == '.';
#else
    DWORD attrs = ::GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#endif
}

bool FileSystemScanner::is_backup_file(const std::filesystem::path& path) const {
    auto filename = path.filename().string();
    return !filename.empty() && filename.back() == '~';
}

std::string FileSystemScanner::icon_for(const FileEntry& entry) const {
    if (entry.type == FileEntry::Type::Directory) {
        return entry.is_hidden ? "hidden" : "folder";
    }
    if (entry.type == FileEntry::Type::Symlink) {
        return "symlink";
    }
    if (entry.type == FileEntry::Type::Regular) {
        const auto ext = entry.path.extension().string();
        if (ext.size() > 1) {
            std::string lower = ext.substr(1);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return lower;
        }
        return "file";
    }
    return "file";
}

void FileSystemScanner::walk_directory(const std::filesystem::path& path, std::size_t depth,
                                       std::vector<FileEntry>& out) const {
    std::error_code ec;
    std::vector<std::filesystem::directory_entry> entries;
    for (std::filesystem::directory_iterator it{path, ec}; !ec && it != std::filesystem::directory_iterator{}; it.increment(ec)) {
        if (ec) {
            break;
        }
        if (!should_include(*it)) {
            continue;
        }
        entries.push_back(*it);
    }

    auto comparator = [&](const std::filesystem::directory_entry& lhs, const std::filesystem::directory_entry& rhs) {
        std::error_code lec;
        std::error_code rec;
        const bool lhs_dir = lhs.is_directory(lec);
        const bool rhs_dir = rhs.is_directory(rec);

        if (options_.group_directories_first && lhs_dir != rhs_dir) {
            return lhs_dir && !rhs_dir;
        }
        if (options_.sort_files_first && lhs_dir != rhs_dir) {
            return !lhs_dir && rhs_dir;
        }
        if (options_.dots_first) {
            const bool lhs_dot = lhs.path().filename() == "." || lhs.path().filename() == "..";
            const bool rhs_dot = rhs.path().filename() == "." || rhs.path().filename() == "..";
            if (lhs_dot != rhs_dot) {
                return lhs_dot && !rhs_dot;
            }
        }

        const auto compare_name = [&]() {
            return lhs.path().filename().string() < rhs.path().filename().string();
        };

        switch (options_.sort_mode) {
            case Config::SortMode::Name:
                return compare_name();
            case Config::SortMode::Time: {
                auto ltime = lhs.last_write_time(lec);
                auto rtime = rhs.last_write_time(rec);
                if (ltime == rtime) {
                    return compare_name();
                }
                return ltime > rtime;
            }
            case Config::SortMode::Size: {
                auto lsize = lhs.file_size(lec);
                auto rsize = rhs.file_size(rec);
                if (lsize == rsize) {
                    return compare_name();
                }
                return lsize > rsize;
            }
            case Config::SortMode::Extension: {
                auto lext = lhs.path().extension().string();
                auto rext = rhs.path().extension().string();
                if (lext == rext) {
                    return compare_name();
                }
                return lext < rext;
            }
            case Config::SortMode::None:
                return false;
        }
        return compare_name();
    };

    if (options_.sort_mode != Config::SortMode::None || options_.group_directories_first || options_.sort_files_first ||
        options_.dots_first) {
        std::sort(entries.begin(), entries.end(), comparator);
    }

    if (options_.reverse) {
        std::reverse(entries.begin(), entries.end());
    }

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const bool last = index + 1 == entries.size();
        if (auto item = make_entry(entries[index], path, depth)) {
            item->depth = depth;
            item->last_sibling = last;
            out.push_back(*item);

            bool is_directory = item->type == FileEntry::Type::Directory;
            if (options_.tree && is_directory) {
                if (!options_.tree_depth || depth < *options_.tree_depth) {
                    walk_directory(entries[index].path(), depth + 1, out);
                }
            }
        }
    }
}

} // namespace nicels
