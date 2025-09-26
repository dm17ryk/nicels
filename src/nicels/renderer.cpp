#include "nicels/renderer.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <iostream>
#include <system_error>

#include "nicels/formatter.hpp"
#include "nicels/logger.hpp"
#include "nicels/perf.hpp"
#include "nicels/platform.hpp"

namespace nicels {
namespace {

bool should_skip(const ListingOptions& options, const FileEntry& entry) {
    if (!options.include_all) {
        if (!options.almost_all) {
            if (entry.hidden) {
                return true;
            }
        } else {
            const auto name = entry.display_path.filename().string();
            if (name == "." || name == "..") {
                return true;
            }
        }
    }
    if (options.directories_only && !entry.is_directory) {
        return true;
    }
    if (options.files_only && entry.is_directory) {
        return true;
    }
    return false;
}

std::vector<FileEntry> filter_entries(const ListingOptions& options, std::vector<FileEntry> entries) {
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const FileEntry& entry) {
                      return should_skip(options, entry);
                  }),
        entries.end());
    return entries;
}

void sort_entries(const ListingOptions& options, std::vector<FileEntry>& entries) {
    auto compare_name = [](const FileEntry& a, const FileEntry& b) {
        return a.display_path.string() < b.display_path.string();
    };
    auto compare_time = [](const FileEntry& a, const FileEntry& b) {
        return a.mtime > b.mtime;
    };
    auto compare_size = [](const FileEntry& a, const FileEntry& b) {
        return a.size > b.size;
    };

    if (options.sort_time) {
        std::stable_sort(entries.begin(), entries.end(), compare_time);
    } else if (options.sort_size) {
        std::stable_sort(entries.begin(), entries.end(), compare_size);
    } else {
        std::stable_sort(entries.begin(), entries.end(), compare_name);
    }

    if (options.group_directories_first) {
        std::stable_partition(entries.begin(), entries.end(), [](const FileEntry& entry) {
            return entry.is_directory;
        });
    }

    if (options.reverse_sort) {
        std::reverse(entries.begin(), entries.end());
    }
}

std::string sanitize_name(const std::string& name, bool hide_control_chars) {
    if (!hide_control_chars) {
        return name;
    }
    std::string out;
    out.reserve(name.size());
    for (unsigned char ch : name) {
        out.push_back(std::isprint(ch) ? static_cast<char>(ch) : '?');
    }
    return out;
}

} // namespace

Renderer::Renderer(const ListingOptions& options, const Theme& theme, GitStatusCache& git_cache)
    : options_{options}
    , theme_{theme}
    , git_cache_{git_cache} {}

void Renderer::render_entries(const std::vector<FileEntry>& entries) const {
    std::vector<FileEntry> copy(entries.begin(), entries.end());
    auto filtered = prepare_entries(std::move(copy));
    for (const auto& entry : filtered) {
        render_single(entry);
    }
}

void Renderer::render_single(const FileEntry& entry) const {
    std::cout << format_line(entry) << '\n';
}

void Renderer::render_tree(const FileEntry& root, const FileSystemScanner& scanner, std::optional<int> depth_limit) const {
    struct StackItem {
        FileEntry entry;
        std::vector<FileEntry> children;
        std::size_t index{0};
        std::string prefix;
    };

    std::error_code ec;
    std::vector<StackItem> stack;
    stack.push_back(StackItem{root, {}, 0, ""});

    while (!stack.empty()) {
        auto& current = stack.back();
        const bool is_root = stack.size() == 1;
        std::string line_prefix = current.prefix;
        if (!is_root) {
            line_prefix += current.index + 1 < current.children.size() ? "├── " : "└── ";
        }
        std::cout << line_prefix << format_name(current.entry) << '\n';

        if (!current.entry.is_directory) {
            stack.pop_back();
            if (!stack.empty()) {
                ++stack.back().index;
            }
            continue;
        }

        if (depth_limit && static_cast<int>(stack.size() - 1) >= *depth_limit) {
            stack.pop_back();
            if (!stack.empty()) {
                ++stack.back().index;
            }
            continue;
        }

        if (current.children.empty()) {
            auto children = scanner.scan(current.entry.path, false, ec);
            if (ec) {
                Logger::instance().log(LogLevel::Warn, "tree scan failure for {}: {}", current.entry.path.string(), ec.message());
                ec.clear();
            }
            children = filter_entries(options_, std::move(children));
            sort_entries(options_, children);
            current.children = std::move(children);
        }

        if (current.index >= current.children.size()) {
            stack.pop_back();
            if (!stack.empty()) {
                ++stack.back().index;
            }
            continue;
        }

        auto child = current.children[current.index];
        std::string next_prefix = current.prefix;
        if (!is_root) {
            next_prefix += current.index + 1 < current.children.size() ? "│   " : "    ";
        }

        stack.push_back(StackItem{child, {}, 0, next_prefix});
    }
}

std::string Renderer::format_name(const FileEntry& entry) const {
    const std::string raw = sanitize_name(entry.display_path.string(), options_.hide_control_chars);
    std::error_code ec;
    std::filesystem::directory_entry dir_entry{entry.path, ec};
    const std::string color = ec ? std::string{} : theme_.color_for(dir_entry);
    const std::string icon = ec ? std::string{} : theme_.icon_for(dir_entry);
    std::string decorated = raw;
    if (!icon.empty()) {
        decorated = icon + " " + decorated;
    }
    if (!color.empty()) {
        decorated = color + decorated + std::string{Theme::reset_color()};
    }
    if (options_.classify) {
        if (entry.is_directory) {
            decorated += '/';
        } else if (entry.is_symlink) {
            decorated += '@';
        }
    }
    if (options_.hyperlink_paths) {
        const auto target = entry.path.lexically_normal().string();
        decorated = std::format("\033]8;;{}\033\\{}\033]8;;\033\\", target, decorated);
    }
    return decorated;
}

std::string Renderer::format_line(const FileEntry& entry) const {
    const auto indicator = git_indicator(entry);
    const std::string indicator_text = indicator ? std::string(1, *indicator) + ' ' : std::string{};
    if (!options_.long_format) {
        return indicator_text + format_name(entry);
    }
    const auto perms = formatter::permissions(entry.status.permissions());
    const auto owner = formatter::owner_group(entry.owner, entry.group);
    const auto size = formatter::file_size(entry.size, options_.size_style);
    const auto time = formatter::timestamp(entry.mtime, options_.time_style);
    return std::format("{}{:>11} {:<20} {:>8} {} {}", indicator_text, perms, owner, size, time, format_name(entry));
}

std::optional<char> Renderer::git_indicator(const FileEntry& entry) const {
    if (!git_cache_.enabled()) {
        return std::nullopt;
    }
    auto status = git_cache_.status_for(entry.path);
    if (!status) {
        return std::nullopt;
    }
    return to_indicator(*status);
}

std::vector<FileEntry> Renderer::prepare_entries(std::vector<FileEntry> entries) const {
    entries = filter_entries(options_, std::move(entries));
    sort_entries(options_, entries);
    return entries;
}

} // namespace nicels
