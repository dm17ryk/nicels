#include "nicels/renderer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

#include "nicels/formatter.h"
#include "nicels/platform.h"

namespace nicels {
namespace {

char newline_char(const Config::Options& options) {
    return options.zero_terminate ? '\0' : '\n';
}

std::string sanitize_name(std::string_view input, bool hide_control) {
    std::string result;
    result.reserve(input.size());
    for (unsigned char ch : input) {
        if (hide_control && std::iscntrl(ch) && ch != '\t') {
            result.push_back('?');
        } else {
            result.push_back(static_cast<char>(ch));
        }
    }
    return result;
}

std::size_t display_width(std::string_view text) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\033') {
            auto pos = text.find('m', i);
            if (pos == std::string_view::npos) {
                break;
            }
            i = pos;
            continue;
        }
        ++width;
    }
    return width;
}

std::string hyperlink_wrap(const std::filesystem::path& path, std::string_view text) {
    auto absolute = std::filesystem::absolute(path);
    std::string url = "file://" + absolute.generic_string();
    return std::format("\033]8;;{}\033\\{}\033]8;;\033\\", url, text);
}

std::string entry_label(const FileEntry& entry, const Config::Options& options, const Theme& theme) {
    std::string name = sanitize_name(entry.display_path.string(), options.hide_control_chars);
    if (entry.type == FileEntry::Type::Symlink && !entry.link_target.empty()) {
        name += " -> " + entry.link_target.string();
    }
    auto colored = theme.colorize(entry, name);
    if (options.hyperlink) {
        return hyperlink_wrap(entry.path, colored);
    }
    return colored;
}

std::string git_badge(const FileEntry& entry, const Config::Options& options, const Theme& theme) {
    if (!options.show_git_status || entry.git_status.empty()) {
        return {};
    }
    auto colored = formatter::git_status(entry, theme);
    return colored.empty() ? entry.git_status : colored;
}

} // namespace

Renderer::Renderer(const Config::Options& options, std::ostream& output)
    : options_{options}, out_{output}, theme_{options} {}

void Renderer::render(std::vector<FileEntry> entries) {
    if (options_.show_git_status) {
        // git status values should already be filled before rendering
    }

    switch (options_.format) {
        case Config::FormatStyle::Long:
            render_long(entries);
            break;
        case Config::FormatStyle::SingleColumn:
            render_single_column(entries);
            break;
        case Config::FormatStyle::ColumnsHorizontal:
            render_columns(entries, true);
            break;
        case Config::FormatStyle::Columns:
            render_columns(entries, false);
            break;
        case Config::FormatStyle::CommaSeparated:
            render_comma_separated(entries);
            break;
    }
}

std::string Renderer::tree_prefix(const FileEntry& entry) const {
    if (!options_.tree || entry.depth == 0) {
        return {};
    }
    std::string prefix;
    prefix.reserve(entry.depth * 3);
    for (std::size_t i = 1; i < entry.depth; ++i) {
        prefix += "│  ";
    }
    prefix += entry.last_sibling ? "└─ " : "├─ ";
    return prefix;
}

void Renderer::render_single_column(const std::vector<FileEntry>& entries) {
    const char nl = newline_char(options_);
    for (const auto& entry : entries) {
        auto badge = git_badge(entry, options_, theme_);
        auto label = entry_label(entry, options_, theme_);
        std::string prefix = tree_prefix(entry);
        if (!badge.empty()) {
            auto badge_width = display_width(badge);
            if (badge_width < 3) {
                out_ << std::string(3 - badge_width, ' ');
            }
            out_ << badge << ' ';
        }
        out_ << prefix << label << nl;
    }
}

void Renderer::render_columns(const std::vector<FileEntry>& entries, bool horizontal) {
    std::vector<std::string> labels;
    labels.reserve(entries.size());
    std::size_t max_width = 0;
    for (const auto& entry : entries) {
        auto badge = git_badge(entry, options_, theme_);
        std::string prefix;
        if (!badge.empty()) {
            auto badge_width = display_width(badge);
            if (badge_width < 3) {
                prefix.append(3 - badge_width, ' ');
            }
            prefix += badge;
            prefix.push_back(' ');
        }
        prefix += tree_prefix(entry);
        auto label = entry_label(entry, options_, theme_);
        std::string composed = prefix + label;
        labels.push_back(composed);
        max_width = std::max(max_width, display_width(composed));
    }

    const int term_width = options_.output_width ? *options_.output_width : platform::terminal_width();
    std::size_t column_width = max_width + 2;
    if (column_width == 0) {
        column_width = 1;
    }
    std::size_t columns = std::max<std::size_t>(1, term_width / static_cast<int>(column_width));
    if (columns == 0) {
        columns = 1;
    }
    std::size_t rows = (labels.size() + columns - 1) / columns;
    const char nl = newline_char(options_);

    auto index_at = [&](std::size_t row, std::size_t col) {
        if (horizontal) {
            return row * columns + col;
        }
        return col * rows + row;
    };

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t col = 0; col < columns; ++col) {
            auto idx = index_at(row, col);
            if (idx >= labels.size()) {
                continue;
            }
            auto& text = labels[idx];
            auto width = display_width(text);
            out_ << text;
            if (col + 1 < columns) {
                auto padding = column_width > width ? column_width - width : 1;
                out_ << std::string(padding, ' ');
            }
        }
        out_ << nl;
    }
}

void Renderer::render_comma_separated(const std::vector<FileEntry>& entries) {
    const char nl = newline_char(options_);
    bool first = true;
    for (const auto& entry : entries) {
        auto label = entry_label(entry, options_, theme_);
        if (!first) {
            out_ << ", ";
        }
        out_ << label;
        first = false;
    }
    out_ << nl;
}

void Renderer::render_long(const std::vector<FileEntry>& entries) {
    struct Row {
        std::string inode;
        std::string perms;
        std::string links;
        std::string owner;
        std::string group;
        std::string size;
        std::string time;
        std::string git;
        std::string name;
        const FileEntry* entry = nullptr;
    };

    std::vector<Row> rows;
    rows.reserve(entries.size());

    std::size_t inode_w = 0;
    std::size_t perms_w = 0;
    std::size_t links_w = 0;
    std::size_t owner_w = 0;
    std::size_t group_w = 0;
    std::size_t size_w = 0;
    std::size_t git_w = 0;

    for (const auto& entry : entries) {
        Row row;
        row.entry = &entry;
        row.perms = formatter::permissions(entry);
        perms_w = std::max(perms_w, row.perms.size());
        row.links = formatter::link_count(entry);
        links_w = std::max(links_w, row.links.size());
        if (options_.show_owner) {
            row.owner = formatter::owner(entry, options_);
            owner_w = std::max(owner_w, row.owner.size());
        }
        if (options_.show_group) {
            row.group = formatter::group(entry, options_);
            group_w = std::max(group_w, row.group.size());
        }
        row.size = formatter::size(entry, options_);
        size_w = std::max(size_w, row.size.size());
        row.time = formatter::modified_time(entry, options_);
        if (options_.show_inode) {
            row.inode = formatter::inode(entry);
            inode_w = std::max(inode_w, row.inode.size());
        }
        if (options_.show_git_status) {
            row.git = git_badge(entry, options_, theme_);
            git_w = std::max(git_w, display_width(row.git));
        }
        auto label = entry_label(entry, options_, theme_);
        row.name = tree_prefix(entry) + label;
        rows.push_back(std::move(row));
    }

    const char nl = newline_char(options_);
    for (const auto& row : rows) {
        if (options_.show_inode) {
            out_ << std::setw(static_cast<int>(inode_w)) << row.inode << ' ';
        }
        out_ << std::setw(static_cast<int>(perms_w)) << row.perms << ' ';
        out_ << std::setw(static_cast<int>(links_w)) << row.links << ' ';
        if (options_.show_owner) {
            out_ << std::setw(static_cast<int>(owner_w)) << row.owner << ' ';
        }
        if (options_.show_group) {
            out_ << std::setw(static_cast<int>(group_w)) << row.group << ' ';
        }
        out_ << std::setw(static_cast<int>(size_w)) << row.size << ' ';
        out_ << row.time << ' ';
        if (options_.show_git_status && !row.git.empty()) {
            auto pad = git_w > display_width(row.git) ? git_w - display_width(row.git) : 0;
            if (pad > 0) {
                out_ << std::string(pad, ' ');
            }
            out_ << row.git << ' ';
        } else if (options_.show_git_status && git_w > 0) {
            out_ << std::string(git_w + 1, ' ');
        }
        out_ << row.name << nl;
    }
}

} // namespace nicels
