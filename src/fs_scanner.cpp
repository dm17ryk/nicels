#include "nicels/fs_scanner.hpp"

#include "nicels/string_utils.hpp"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <system_error>

namespace nicels {

namespace {
bool match_char_class(const std::string& pattern, std::size_t& idx, char ch) {
    const auto start = idx;
    if (idx >= pattern.size()) {
        return false;
    }
    bool negated = false;
    if (pattern[idx] == '!' || pattern[idx] == '^') {
        negated = true;
        ++idx;
    }
    bool matched = false;
    while (idx < pattern.size() && pattern[idx] != ']') {
        char start_char = pattern[idx];
        if (start_char == '\\' && idx + 1 < pattern.size()) {
            ++idx;
            start_char = pattern[idx];
        }
        ++idx;
        if (idx < pattern.size() && pattern[idx] == '-' && idx + 1 < pattern.size() && pattern[idx + 1] != ']') {
            ++idx;
            char end_char = pattern[idx];
            if (end_char == '\\' && idx + 1 < pattern.size()) {
                ++idx;
                end_char = pattern[idx];
            }
            if (start_char <= ch && ch <= end_char) {
                matched = true;
            }
            ++idx;
        } else {
            if (ch == start_char) {
                matched = true;
            }
        }
    }
    if (idx < pattern.size() && pattern[idx] == ']') {
        ++idx;
        return negated ? !matched : matched;
    }
    idx = start;
    return false;
}

bool wildcard_match(const std::string& pattern, const std::string& text) {
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t match = 0;
    while (t < text.size()) {
        if (p < pattern.size()) {
            char pc = pattern[p];
            if (pc == '?') {
                ++p;
                ++t;
                continue;
            }
            if (pc == '*') {
                star = ++p;
                match = t;
                continue;
            }
            if (pc == '[') {
                std::size_t idx = p + 1;
                if (match_char_class(pattern, idx, text[t])) {
                    p = idx;
                    ++t;
                    continue;
                }
            } else {
                if (pc == '\\' && p + 1 < pattern.size()) {
                    ++p;
                    pc = pattern[p];
                }
                if (pc == text[t]) {
                    ++p;
                    ++t;
                    continue;
                }
            }
        }
        if (star != std::string::npos) {
            p = star;
            ++match;
            t = match;
            continue;
        }
        return false;
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

bool matches_any_pattern(const std::string& name, const std::vector<std::string>& patterns) {
    return std::ranges::any_of(patterns, [&](const std::string& pat) { return wildcard_match(pat, name); });
}

} // namespace

FileSystemScanner::FileSystemScanner(const Config::Data& config) : config_(config) {}

bool FileSystemScanner::matches_patterns(std::string_view name, const std::vector<std::string>& patterns) const {
    if (patterns.empty()) {
        return false;
    }
    return matches_any_pattern(std::string(name), patterns);
}

bool FileSystemScanner::include_entry(const std::filesystem::directory_entry& entry) const {
    const auto filename = entry.path().filename().string();
    const bool hidden = string_utils::is_hidden(filename);

    if (!config_.all) {
        if (hidden) {
            if (config_.almost_all) {
                if (filename == "." || filename == "..") {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    if (config_.ignore_backups && !filename.empty() && filename.back() == '~') {
        return false;
    }

    if (!config_.all && !config_.almost_all) {
        if (matches_patterns(filename, config_.hide_patterns)) {
            return false;
        }
    }
    if (matches_patterns(filename, config_.ignore_patterns)) {
        return false;
    }

    if (config_.dirs_only && !entry.is_directory()) {
        return false;
    }
    if (config_.files_only && entry.is_directory()) {
        return false;
    }

    return true;
}

std::vector<FileEntry> FileSystemScanner::scan(const std::filesystem::path& root) const {
    std::vector<FileEntry> entries;
    std::error_code ec;

    GitRepositoryStatus git_status;
    const bool has_git = config_.git_status && git_status.load(root);

    auto add_entry = [&](const std::filesystem::directory_entry& entry) {
        if (!include_entry(entry)) {
            return;
        }

        FileEntry info;
        info.path = entry.path();
        info.name = entry.path().filename().string();
        info.status = entry.symlink_status(ec);
        if (ec) {
            ec.clear();
            info.status = entry.status(ec);
        }
        info.is_directory = entry.is_directory();
        info.is_symlink = entry.is_symlink();
        info.is_hidden = string_utils::is_hidden(info.name);
        info.last_write_time = entry.last_write_time(ec);
        if (ec) {
            ec.clear();
            info.last_write_time = std::filesystem::file_time_type::clock::now();
        }
        if (entry.is_regular_file()) {
            info.size = entry.file_size(ec);
            if (ec) {
                ec.clear();
                info.size = 0;
            }
        }

        if (has_git) {
            if (auto status = git_status.status_for(info.path)) {
                info.git = *status;
            }
        }

        entries.push_back(std::move(info));
    };

    if (std::filesystem::is_directory(root, ec)) {
        std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry : std::filesystem::directory_iterator(root, options, ec)) {
            add_entry(entry);
        }
    } else {
        if (std::filesystem::exists(root, ec)) {
            add_entry(std::filesystem::directory_entry(root, ec));
        }
    }

    if (entries.empty()) {
        return entries;
    }

    if (config_.dots_first) {
        std::stable_sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
            const bool ldot = lhs.is_hidden;
            const bool rdot = rhs.is_hidden;
            if (ldot == rdot) {
                return false;
            }
            return ldot && !rdot;
        });
    }

    if (config_.group_dirs_first) {
        std::stable_sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
            if (lhs.is_directory == rhs.is_directory) {
                return false;
            }
            return lhs.is_directory && !rhs.is_directory;
        });
    } else if (config_.sort_files_first) {
        std::stable_sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
            if (lhs.is_directory == rhs.is_directory) {
                return false;
            }
            return !lhs.is_directory && rhs.is_directory;
        });
    }

    auto compare_by_name = [](const FileEntry& lhs, const FileEntry& rhs) {
        return lhs.name < rhs.name;
    };

    switch (config_.sort_mode) {
    case Config::SortMode::None:
        break;
    case Config::SortMode::Name:
        std::stable_sort(entries.begin(), entries.end(), compare_by_name);
        break;
    case Config::SortMode::Time:
        std::stable_sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
            return lhs.last_write_time > rhs.last_write_time;
        });
        break;
    case Config::SortMode::Size:
        std::stable_sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
            return lhs.size > rhs.size;
        });
        break;
    case Config::SortMode::Extension:
        std::stable_sort(entries.begin(), entries.end(), [&](const FileEntry& lhs, const FileEntry& rhs) {
            return lhs.path.extension().string() < rhs.path.extension().string();
        });
        break;
    }

    if (config_.reverse) {
        std::reverse(entries.begin(), entries.end());
    }

    return entries;
}

} // namespace nicels
