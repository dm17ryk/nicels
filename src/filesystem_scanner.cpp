#include "nicels/filesystem_scanner.hpp"

#include "nicels/logger.hpp"
#include "nicels/platform.hpp"
#include "nicels/utility.hpp"

#include <algorithm>
#include <system_error>
#include <string_view>

namespace nicels {

namespace {

[[nodiscard]] bool matches_any_pattern(std::string_view name, const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        if (wildcard_match(pattern, name)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] FileEntry make_entry(const std::filesystem::directory_entry& entry, const Options& options) {
    FileEntry result;
    result.path = entry.path();
    const auto filename = entry.path().filename().string();
    result.display_name = sanitize_control_chars(filename.empty() ? entry.path().string() : filename, options.hide_control_chars);

    std::error_code ec;
    auto status = options.follow_symlinks ? entry.status(ec) : entry.symlink_status(ec);
    if (ec) {
        Logger::instance().log(LogLevel::Warn, "scan", "Failed status for " + entry.path().string() + ": " + ec.message());
        ec.clear();
    }
    result.status = status;
    result.is_directory = status.type() == std::filesystem::file_type::directory;
    result.is_symlink = status.type() == std::filesystem::file_type::symlink;
    result.size = entry.is_regular_file(ec) ? entry.file_size(ec) : 0;
    if (ec) {
        ec.clear();
    }
    result.last_write = entry.last_write_time(ec);
    if (ec) {
        result.last_write = std::filesystem::file_time_type::clock::now();
        ec.clear();
    }
    if (!result.is_directory) {
        result.hard_links = std::filesystem::hard_link_count(entry.path(), ec);
        if (ec) {
            result.hard_links = 1;
            ec.clear();
        }
    } else {
        result.hard_links = 1;
    }
    result.inode = inode_number(entry);
    result.owner = owner_name(entry.path(), options.numeric_ids, options.follow_symlinks);
    result.group = group_name(entry.path(), options.numeric_ids, options.follow_symlinks);
    result.is_hidden = is_hidden(entry);
    result.is_executable = has_executable_bit(entry);
    result.symlink_target = resolve_symlink(entry, options.follow_symlinks);
    if (result.is_symlink) {
        std::error_code exists_ec;
        result.is_broken_symlink = !std::filesystem::exists(entry.path(), exists_ec);
    }
    return result;
}

[[nodiscard]] bool should_include(const FileEntry& entry, const Options& options) {
    const auto name = entry.path.filename().string();
    if (!options.all) {
        if (entry.is_hidden) {
            if (options.almost_all && (name == "." || name == "..")) {
                return false;
            }
            if (!options.almost_all) {
                return false;
            }
        }
    }

    if (options.ignore_backups && !name.empty() && name.back() == '~') {
        return false;
    }

    if (!options.hide_patterns.empty() && matches_any_pattern(name, options.hide_patterns)) {
        return false;
    }

    if (!options.ignore_patterns.empty() && matches_any_pattern(name, options.ignore_patterns)) {
        return false;
    }

    if (options.directories_only && !entry.is_directory) {
        return false;
    }
    if (options.files_only && entry.is_directory) {
        return false;
    }
    return true;
}

[[nodiscard]] std::vector<FileEntry> sort_entries(std::vector<FileEntry> entries, const Options& options) {
    auto comparator = [&](const FileEntry& lhs, const FileEntry& rhs) {
        if (options.group_directories_first && lhs.is_directory != rhs.is_directory) {
            return lhs.is_directory > rhs.is_directory;
        }
        if (options.sort_directories_last && lhs.is_directory != rhs.is_directory) {
            return lhs.is_directory < rhs.is_directory;
        }
        switch (options.sort) {
        case SortField::Time:
            return lhs.last_write > rhs.last_write;
        case SortField::Size:
            return lhs.size > rhs.size;
        case SortField::Extension: {
            const auto le = lhs.path.extension().string();
            const auto re = rhs.path.extension().string();
            if (le == re) {
                return lhs.display_name < rhs.display_name;
            }
            return le < re;
        }
        case SortField::None:
            return false;
        case SortField::Name:
        default:
            return lhs.display_name < rhs.display_name;
        }
    };

    std::stable_sort(entries.begin(), entries.end(), comparator);
    if (options.reverse) {
        std::reverse(entries.begin(), entries.end());
    }
    return entries;
}

[[nodiscard]] DirectoryResult collect_directory(const std::filesystem::directory_entry& root,
                                                const Options& options,
                                                std::size_t depth = 0) {
    DirectoryResult result;
    result.source = root.path();
    std::error_code ec;
    result.is_directory = root.is_directory(ec);
    if (ec) {
        Logger::instance().log(LogLevel::Warn, "scan", "Failed to probe " + root.path().string() + ": " + ec.message());
    }

    FileEntry self_entry = make_entry(root, options);
    result.self = self_entry;

    if (!result.is_directory || options.directories_only) {
        if (should_include(self_entry, options)) {
            result.entries.push_back(std::move(self_entry));
        }
        return result;
    }

    std::vector<FileEntry> entries;
    const auto dir_opts = options.follow_symlinks ? std::filesystem::directory_options::follow_directory_symlink : std::filesystem::directory_options::none;
    std::error_code iter_ec;
    for (std::filesystem::directory_iterator it{root.path(), dir_opts | std::filesystem::directory_options::skip_permission_denied, iter_ec};
         it != std::filesystem::directory_iterator{};
         it.increment(iter_ec)) {
        if (iter_ec) {
            Logger::instance().log(LogLevel::Warn, "scan", "Failed to iterate " + root.path().string() + ": " + iter_ec.message());
            iter_ec.clear();
            continue;
        }
        FileEntry entry = make_entry(*it, options);
        if (!should_include(entry, options)) {
            continue;
        }
        entries.push_back(entry);
    }

    result.entries = sort_entries(std::move(entries), options);

    if (options.format == FormatStyle::Tree) {
        for (const auto& entry : result.entries) {
            if (!entry.is_directory) {
                continue;
            }
            if (entry.is_symlink && !options.follow_symlinks) {
                continue;
            }
            if (options.tree_depth && depth >= *options.tree_depth) {
                continue;
            }
            std::filesystem::directory_entry child{entry.path};
            result.children.push_back(collect_directory(child, options, depth + 1));
        }
    }

    return result;
}

} // namespace

FilesystemScanner::FilesystemScanner() = default;

std::vector<DirectoryResult> FilesystemScanner::scan(const Options& options) const {
    std::vector<std::string> paths = options.paths;
    if (paths.empty()) {
        paths.emplace_back(".");
    }

    std::vector<DirectoryResult> results;
    results.reserve(paths.size());

    for (const auto& path_str : paths) {
        std::filesystem::path path{path_str};
        std::error_code ec;
        std::filesystem::directory_entry entry{path, ec};
        if (ec) {
            Logger::instance().log(LogLevel::Error, "scan", "Unable to access " + path.string() + ": " + ec.message());
            continue;
        }
        auto dir = collect_directory(entry, options);
        results.push_back(std::move(dir));
    }

    return results;
}

} // namespace nicels
