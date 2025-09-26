#include "nicels/fs_scanner.hpp"

#include <algorithm>
#include <system_error>

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

#include "nicels/logger.hpp"

namespace nicels {
namespace {

#ifndef _WIN32
std::optional<std::string> lookup_user(uid_t uid) {
    if (auto* pw = ::getpwuid(uid)) {
        return std::string{pw->pw_name};
    }
    return std::nullopt;
}

std::optional<std::string> lookup_group(gid_t gid) {
    if (auto* gr = ::getgrgid(gid)) {
        return std::string{gr->gr_name};
    }
    return std::nullopt;
}

std::optional<uintmax_t> read_inode(const std::filesystem::path& path, bool follow) {
    struct ::stat st;
    if ((follow ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st)) == 0) {
        return static_cast<uintmax_t>(st.st_ino);
    }
    return std::nullopt;
}

std::pair<std::optional<std::string>, std::optional<std::string>>
lookup_owner_group(const std::filesystem::path& path, bool follow) {
    struct ::stat st;
    if ((follow ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st)) == 0) {
        return {lookup_user(st.st_uid), lookup_group(st.st_gid)};
    }
    return {};
}

#else
std::optional<uintmax_t> read_inode(const std::filesystem::path&, bool) {
    return std::nullopt;
}

std::pair<std::optional<std::string>, std::optional<std::string>>
lookup_owner_group(const std::filesystem::path&, bool) {
    return {};
}

bool is_hidden_windows(const std::filesystem::path& path) {
    const DWORD attrs = ::GetFileAttributesW(path.wstring().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
}
#endif

bool is_hidden_posix(const std::filesystem::path& path) {
    auto name = path.filename().string();
    return !name.empty() && name.front() == '.';
}

} // namespace

FileSystemScanner::FileSystemScanner() = default;

std::vector<FileEntry> FileSystemScanner::scan(const std::filesystem::path& path, bool follow_symlinks, std::error_code& ec) const {
    std::vector<FileEntry> entries;
    ec.clear();
    std::filesystem::directory_options options = follow_symlinks
        ? std::filesystem::directory_options::follow_directory_symlink
        : std::filesystem::directory_options::none;
    for (std::filesystem::directory_iterator it(path, options, ec); !ec && it != std::filesystem::directory_iterator{}; ++it) {
        entries.push_back(make_entry(*it, follow_symlinks, ec));
        if (ec) {
            Logger::instance().log(LogLevel::Warn, "failed to inspect {}: {}", it->path().string(), ec.message());
            ec.clear();
        }
    }
    return entries;
}

FileEntry FileSystemScanner::stat_path(const std::filesystem::path& path, bool follow_symlinks, std::error_code& ec) const {
    std::filesystem::directory_entry entry{path, ec};
    if (ec) {
        return {};
    }
    return make_entry(entry, follow_symlinks, ec);
}

FileEntry FileSystemScanner::make_entry(const std::filesystem::directory_entry& entry, bool follow_symlinks, std::error_code& ec) const {
    FileEntry out;
    out.path = entry.path();
    auto filename = entry.path().filename();
    if (filename.empty()) {
        out.display_path = entry.path();
    } else {
        out.display_path = filename;
    }
    out.is_symlink = entry.is_symlink(ec);
    if (ec) {
        return out;
    }
    out.status = follow_symlinks ? entry.status(ec) : entry.symlink_status(ec);
    if (ec) {
        return out;
    }
    out.is_directory = entry.is_directory(ec);
    if (ec) {
        return out;
    }
    out.is_regular = entry.is_regular_file(ec);
    if (ec) {
        return out;
    }
    if (out.is_regular) {
        out.size = entry.file_size(ec);
        if (ec) {
            ec.clear();
            out.size = 0;
        }
    }
    out.mtime = entry.last_write_time(ec);
    if (ec) {
        ec.clear();
    }

#ifndef _WIN32
    out.hidden = is_hidden_posix(out.display_path);
#else
    out.hidden = is_hidden_windows(entry.path());
#endif

#ifndef _WIN32
    auto [owner, group] = lookup_owner_group(entry.path(), follow_symlinks);
    out.owner = owner;
    out.group = group;
#else
    auto pair = lookup_owner_group(entry.path(), follow_symlinks);
    out.owner = pair.first;
    out.group = pair.second;
#endif
    out.inode = read_inode(entry.path(), follow_symlinks);

    return out;
}

} // namespace nicels
