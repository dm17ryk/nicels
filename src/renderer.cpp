#include "nicels/renderer.hpp"

#include "nicels/formatters.hpp"
#include "nicels/platform.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace nicels {

Renderer::Renderer(const Options& options, GitStatusCache& git_cache)
    : options_{options}, git_cache_{git_cache}, theme_{options} {}

void Renderer::render(const std::vector<DirectoryResult>& results, std::ostream& os) const {
    const bool multiple = results.size() > 1;
    for (std::size_t i = 0; i < results.size(); ++i) {
        render_directory(results[i], os, multiple);
        if (i + 1 < results.size()) {
            os << '\n';
        }
    }
}

void Renderer::render_directory(const DirectoryResult& dir, std::ostream& os, bool multiple) const {
    if (dir.is_directory && (multiple || options_.show_header)) {
        os << dir.source << ":\n";
    }

    if (options_.format == FormatStyle::Tree) {
        const auto count = dir.entries.size();
        for (std::size_t index = 0; index < count; ++index) {
            const auto& entry = dir.entries[index];
            const bool last = index + 1 == count;
            const DirectoryResult* child_dir = nullptr;
            if (entry.is_directory) {
                for (const auto& child : dir.children) {
                    if (child.self && child.self->path == entry.path) {
                        child_dir = &child;
                        break;
                    }
                }
            }
            render_tree_entry(entry, child_dir, os, "", last);
        }
        return;
    }

    render_entries(dir.entries, os);
}

void Renderer::render_entries(const std::vector<FileEntry>& entries, std::ostream& os) const {
    if (entries.empty()) {
        return;
    }

    switch (options_.format) {
    case FormatStyle::Long:
        for (const auto& entry : entries) {
            render_long(entry, os);
        }
        break;
    case FormatStyle::Single:
        render_single(entries, os);
        break;
    case FormatStyle::Columns:
    default:
        render_columns(entries, os);
        break;
    }
}

void Renderer::render_entry(const FileEntry& entry, std::ostream& os) const {
    std::string label = entry_label(entry);
    const auto status = git_cache_.status_for(entry.path);
    if (!status.empty()) {
        os << status << ' ';
    }
    os << label;
    if (entry.is_symlink && !entry.symlink_target.empty()) {
        os << " -> " << entry.symlink_target;
    }
}

void Renderer::render_long(const FileEntry& entry, std::ostream& os) const {
    os << format_permissions(entry) << ' ';
    os << std::setw(3) << entry.hard_links << ' ';
    if (options_.show_owner) {
        os << std::setw(8) << entry.owner << ' ';
    }
    if (options_.show_group) {
        os << std::setw(8) << entry.group << ' ';
    }
    if (options_.show_inode && entry.inode) {
        os << std::setw(10) << *entry.inode << ' ';
    }
    os << std::setw(8) << format_size(entry, options_) << ' ';
    os << std::setw(15) << format_time(entry, options_) << ' ';
    render_entry(entry, os);
    os << (options_.zero_terminate ? '\0' : '\n');
}

void Renderer::render_columns(const std::vector<FileEntry>& entries, std::ostream& os) const {
    int width = options_.output_width.value_or(detect_terminal_width());
    std::vector<std::string> labels;
    labels.reserve(entries.size());
    std::size_t max_len = 0;
    for (const auto& entry : entries) {
        auto label = entry_label(entry);
        if (entry.is_symlink && !entry.symlink_target.empty()) {
            label += " -> " + entry.symlink_target;
        }
        labels.push_back(label);
        max_len = std::max(max_len, label.size());
    }

    const int column_width = static_cast<int>(max_len) + 2;
    const int columns = std::max(1, width / column_width);
    const int rows = static_cast<int>((labels.size() + columns - 1) / columns);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            const auto index = col * rows + row;
            if (index >= static_cast<int>(labels.size())) {
                continue;
            }
            os << std::left << std::setw(column_width) << labels[index];
        }
        os << (options_.zero_terminate ? '\0' : '\n');
    }
}

void Renderer::render_single(const std::vector<FileEntry>& entries, std::ostream& os) const {
    for (const auto& entry : entries) {
        render_entry(entry, os);
        os << (options_.zero_terminate ? '\0' : '\n');
    }
}

void Renderer::render_tree(const DirectoryResult& dir, std::ostream& os, std::string indent, bool is_last) const {
    const std::string branch = indent + (is_last ? "└── " : "├── ");
    os << branch;
    render_entry(dir.entries.front(), os);
    os << (options_.zero_terminate ? '\0' : '\n');

    const std::string next_indent = indent + (is_last ? "    " : "│   ");
    const auto& entry = dir.entries.front();
    if (!entry.is_directory) {
        return;
    }

    const auto count = dir.children.size();
    for (std::size_t index = 0; index < count; ++index) {
        render_tree(dir.children[index], os, next_indent, index + 1 == count);
    }
}

void Renderer::render_tree_entry(const FileEntry& entry, const DirectoryResult* child_dir, std::ostream& os, const std::string& indent, bool is_last) const {
    os << indent << (is_last ? "└── " : "├── ");
    render_entry(entry, os);
    os << (options_.zero_terminate ? '\0' : '\n');

    if (!child_dir) {
        return;
    }
    const std::string next_indent = indent + (is_last ? "    " : "│   ");
    const auto count = child_dir->entries.size();
    for (std::size_t index = 0; index < count; ++index) {
        const auto& child_entry = child_dir->entries[index];
        const DirectoryResult* grandchild = nullptr;
        if (child_entry.is_directory) {
            for (const auto& nested : child_dir->children) {
                if (nested.self && nested.self->path == child_entry.path) {
                    grandchild = &nested;
                    break;
                }
            }
        }
        render_tree_entry(child_entry, grandchild, os, next_indent, index + 1 == count);
    }
}

std::string Renderer::entry_label(const FileEntry& entry) const {
    std::string label;
    label.reserve(entry.display_name.size() + 4);
    label += theme_.icon_for(entry);
    label += theme_.apply(entry, entry.display_name);
    return label;
}

} // namespace nicels
