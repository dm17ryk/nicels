#include "nicels/renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <format>
#include <filesystem>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace nicels {

namespace {
std::string human_readable(std::uintmax_t value) {
    static constexpr std::array units { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    double number = static_cast<double>(value);
    std::size_t unit = 0;
    while (number >= 1024.0 && unit + 1 < units.size()) {
        number /= 1024.0;
        ++unit;
    }
    if (unit == 0) {
        return std::format("{} {}", static_cast<std::uintmax_t>(number), units[unit]);
    }
    const int precision = number < 10.0 ? 1 : 0;
    return std::vformat("{:.{}f} {}", std::make_format_args(number, precision, units[unit]));
}

std::string escape_c_style(std::string_view input) {
    std::ostringstream oss;
    oss << '"';
    for (unsigned char ch : input) {
        switch (ch) {
        case '\\': oss << "\\\\"; break;
        case '\"': oss << "\\\""; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (std::isprint(ch)) {
                oss << static_cast<char>(ch);
            } else {
                oss << std::format("\\x{:02X}", ch);
            }
            break;
        }
    }
    oss << '"';
    return oss.str();
}

std::string literal_escape(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char ch : input) {
        if (std::isprint(ch)) {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

char file_type_char(const FileEntry& entry) {
    if (entry.is_directory) {
        return 'd';
    }
    if (entry.is_symlink) {
        return 'l';
    }
    return '-';
}

} // namespace

Renderer::Renderer(const Config::Data& config, std::ostream& stream)
    : config_(config)
    , out_(stream) { }

std::string Renderer::format_permissions(const FileEntry& entry) const {
    std::string perms(10, '-');
    perms[0] = file_type_char(entry);
    const auto p = entry.status.permissions();
    auto set = [&](std::size_t index, std::filesystem::perms bit, char ch) {
        if ((p & bit) != std::filesystem::perms::none) {
            perms[index] = ch;
        }
    };
    set(1, std::filesystem::perms::owner_read, 'r');
    set(2, std::filesystem::perms::owner_write, 'w');
    set(3, std::filesystem::perms::owner_exec, 'x');
    set(4, std::filesystem::perms::group_read, 'r');
    set(5, std::filesystem::perms::group_write, 'w');
    set(6, std::filesystem::perms::group_exec, 'x');
    set(7, std::filesystem::perms::others_read, 'r');
    set(8, std::filesystem::perms::others_write, 'w');
    set(9, std::filesystem::perms::others_exec, 'x');
    return perms;
}

std::string Renderer::format_size(const FileEntry& entry) const {
    if (entry.is_directory) {
        return "-";
    }
    if (config_.bytes || config_.show_block_size) {
        std::uintmax_t size = entry.size;
        if (config_.block_size) {
            const auto block = *config_.block_size;
            if (block > 0) {
                size = (size + block - 1) / block;
            }
        }
        return std::to_string(size);
    }
    return human_readable(entry.size);
}

std::string Renderer::format_time(const FileEntry& entry) const {
    const auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(entry.last_write_time);
    if (!config_.time_style.empty() && config_.time_style.starts_with('+')) {
        return std::format("{:%Y-%m-%d %H:%M:%S}", system_time);
    }
    return std::format("{:%Y-%m-%d %H:%M}", system_time);
}

std::string Renderer::format_git(const FileEntry& entry) const {
    if (!config_.git_status) {
        return {};
    }
    if (entry.git.untracked) {
        return "??";
    }
    std::string code(2, ' ');
    if (entry.git.staged) {
        code[0] = 'S';
    }
    if (entry.git.unstaged) {
        code[1] = 'M';
    }
    return code;
}

std::string Renderer::apply_hyperlink(const FileEntry& entry, std::string_view text) const {
    if (!config_.hyperlink) {
        return std::string(text);
    }
    const auto absolute = std::filesystem::absolute(entry.path).generic_string();
    return std::format("\033]8;;file://{}\033\\{}\033]8;;\033\\", absolute, text);
}

std::string Renderer::colorize(const FileEntry& entry, std::string_view text) const {
    if (config_.no_color) {
        return std::string(text);
    }
    std::string color = "\033[0m";
    if (entry.is_directory) {
        color = "\033[34m";
    } else if (entry.is_symlink) {
        color = "\033[36m";
    } else {
        color = "\033[37m";
    }
    return std::format("{}{}\033[0m", color, text);
}

std::string Renderer::format_name(const FileEntry& entry) const {
    std::string display = entry.name;
    if (config_.hide_control_chars) {
        display = literal_escape(display);
    }

    if (config_.indicator == Config::IndicatorStyle::Slash && entry.is_directory) {
        display.push_back('/');
    }

    std::string quoted;
    switch (config_.quoting_style) {
    case Config::QuotingStyle::C:
        quoted = escape_c_style(display);
        break;
    case Config::QuotingStyle::Escape:
        quoted = escape_c_style(display);
        break;
    default:
        quoted = display;
        break;
    }

    auto colored = colorize(entry, quoted);
    return apply_hyperlink(entry, colored);
}

void Renderer::render(const std::filesystem::path& root, std::span<const FileEntry> entries) const {
    (void)root;
    if (entries.empty()) {
        return;
    }

    if (config_.layout_format == Config::LayoutFormat::Long) {
        std::vector<std::string> size_strings(entries.size());
        std::vector<std::string> time_strings(entries.size());
        std::vector<std::string> git_strings(entries.size());
        std::size_t size_width = 0;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            size_strings[i] = format_size(entries[i]);
            size_width = std::max(size_width, size_strings[i].size());
            time_strings[i] = format_time(entries[i]);
            git_strings[i] = format_git(entries[i]);
        }

        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            const auto git = git_strings[i];
            if (!git.empty()) {
                out_ << git << ' ';
            }
            out_ << format_permissions(entry) << ' ';
            out_ << std::setw(static_cast<int>(size_width)) << size_strings[i] << ' ';
            out_ << time_strings[i] << ' ';
            out_ << format_name(entry);
            if (config_.zero_terminate) {
                out_.put('\0');
            } else {
                out_ << '\n';
            }
        }
        return;
    }

    switch (config_.layout_format) {
    case Config::LayoutFormat::CommaSeparated: {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) {
                out_ << ", ";
            }
            out_ << format_name(entries[i]);
        }
        if (config_.zero_terminate) {
            out_.put('\0');
        } else {
            out_ << '\n';
        }
        break;
    }
    case Config::LayoutFormat::ColumnsHorizontal:
    case Config::LayoutFormat::ColumnsVertical:
    case Config::LayoutFormat::SingleColumn:
    default:
        for (const auto& entry : entries) {
            out_ << format_name(entry);
            if (config_.zero_terminate) {
                out_.put('\0');
            } else {
                out_ << '\n';
            }
        }
        break;
    }
}

} // namespace nicels
