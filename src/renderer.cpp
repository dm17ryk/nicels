#include "renderer.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "perf.h"
#include "platform.h"
#include "theme.h"

namespace nls {
namespace {

namespace fs = std::filesystem;

bool IsNonGraphic(unsigned char ch) {
    return !std::isprint(ch);
}

bool IsShellSafeChar(unsigned char ch) {
    if (std::isalnum(ch)) return true;
    switch (ch) {
        case '_': case '@': case '%': case '+': case '=':
        case ':': case ',': case '.': case '/': case '-':
            return true;
        default:
            return false;
    }
}

std::string CStyleEscape(const std::string& input, bool include_quotes, bool escape_single_quote) {
    const char* hex = "0123456789ABCDEF";
    std::string out;
    if (include_quotes) out.push_back('"');
    out.reserve(input.size() + 4);
    for (unsigned char ch : input) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '\"':
                out += "\\\"";
                break;
            case '\'':
                if (escape_single_quote) {
                    out += "\\'";
                } else {
                    out.push_back('\'');
                }
                break;
            case '\a':
                out += "\\a";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\v':
                out += "\\v";
                break;
            default:
                if (std::isprint(ch)) {
                    out.push_back(static_cast<char>(ch));
                } else {
                    out.push_back('\\');
                    out.push_back('x');
                    out.push_back(hex[(ch >> 4) & 0x0F]);
                    out.push_back(hex[ch & 0x0F]);
                }
                break;
        }
    }
    if (include_quotes) out.push_back('"');
    return out;
}

bool NeedsShellQuotes(const std::string& text) {
    if (text.empty()) return true;
    for (unsigned char ch : text) {
        if (!IsShellSafeChar(ch)) return true;
    }
    return false;
}

std::string ShellQuote(const std::string& text, bool always) {
    bool needs = always || NeedsShellQuotes(text);
    if (!needs) return text;
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('\'');
    for (unsigned char ch : text) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(static_cast<char>(ch));
        }
    }
    out.push_back('\'');
    return out;
}

std::string ShellEscape(const std::string& text, bool always) {
    bool needs = always || NeedsShellQuotes(text);
    if (!needs) return text;
    return std::string("$") + "'" + CStyleEscape(text, false, true) + "'";
}

std::string PercentEncode(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    const char* hex = "0123456789ABCDEF";
    for (unsigned char ch : input) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(hex[(ch >> 4) & 0x0F]);
            out.push_back(hex[ch & 0x0F]);
        }
    }
    return out;
}

std::chrono::system_clock::time_point ToSystemClock(const fs::file_time_type& tp) {
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - fs::file_time_type::clock::now() + system_clock::now());
}

std::string AgeColor(const fs::file_time_type& tp, const ThemeColors& theme) {
    auto file_time = ToSystemClock(tp);
    auto now = std::chrono::system_clock::now();
    auto diff = now - file_time;
    if (diff <= std::chrono::hours(1)) {
        return theme.get("hour_old");
    }
    if (diff <= std::chrono::hours(24)) {
        return theme.get("day_old");
    }
    return theme.get("no_modifier");
}

std::string SizeColor(uintmax_t size, const ThemeColors& theme) {
    constexpr uintmax_t MEDIUM_THRESHOLD = 1ull * 1024ull * 1024ull;
    constexpr uintmax_t LARGE_THRESHOLD = 100ull * 1024ull * 1024ull;
    if (size >= LARGE_THRESHOLD) {
        return theme.get("file_large");
    }
    if (size >= MEDIUM_THRESHOLD) {
        return theme.get("file_medium");
    }
    return theme.get("file_small");
}

}  // namespace

Renderer::Renderer(const Config& config)
    : opt_(config),
      size_formatter_(config),
      time_formatter_(config),
      permission_formatter_(config) {}

void Renderer::PrintPathHeader(const fs::path& path) const {
    std::cout << path.string() << ':';
    TerminateLine();
}

void Renderer::PrintDirectoryHeader(const fs::path& path, bool is_directory) const {
    if (!opt_.header() || opt_.format() != Config::Format::Long) {
        return;
    }

    std::error_code ec;
    fs::path absolute_path = fs::absolute(path, ec);
    fs::path header_path;
    if (!ec) {
        header_path = is_directory ? absolute_path : absolute_path.parent_path();
    }
    if (header_path.empty()) {
        header_path = is_directory ? path : path.parent_path();
    }
    if (header_path.empty()) {
        std::error_code cwd_ec;
        fs::path cwd = fs::current_path(cwd_ec);
        if (!cwd_ec) header_path = cwd;
    }
    if (!header_path.empty()) {
        header_path = header_path.lexically_normal();
    }
    std::string header_str = header_path.string();
    if (!header_str.empty()) {
        std::string root_str = header_path.root_path().string();
        while (header_str.size() > 1 &&
               (header_str.back() == '/' || header_str.back() == '\\') &&
               header_str != root_str) {
            header_str.pop_back();
        }
    }
    const ThemeColors& theme = Theme::instance().colors();
    std::string colored_header = apply_color(theme.get("header_directory"), header_str, theme, opt_.no_color());
    std::cout << '\n' << "Directory: " << colored_header << "\n\n";
}

void Renderer::RenderTree(const std::vector<TreeItem>& nodes,
                          const std::vector<Entry>& flat_entries) const {
    size_t inode_width = ComputeInodeWidth(flat_entries);
    size_t block_width = ComputeBlockWidth(flat_entries);
    std::vector<bool> branch_stack;
    PrintTreeNodes(nodes, inode_width, block_width, branch_stack);
}

void Renderer::RenderEntries(const std::vector<Entry>& entries) const {
    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("renderer::RenderEntries");
        perf_manager.IncrementCounter("entries_rendered", static_cast<std::uint64_t>(entries.size()));
    }

    size_t inode_width = ComputeInodeWidth(entries);
    size_t block_width = ComputeBlockWidth(entries);
    switch (opt_.format()) {
        case Config::Format::Long:
            PrintLong(entries, inode_width, block_width);
            break;
        case Config::Format::SingleColumn:
            for (const auto& entry : entries) {
                std::cout << FormatEntryCell(entry, inode_width, block_width, true);
                TerminateLine();
            }
            break;
        case Config::Format::CommaSeparated:
            PrintCommaSeparated(entries, inode_width, block_width);
            break;
        case Config::Format::ColumnsHorizontal:
        case Config::Format::ColumnsVertical:
        default:
            PrintColumns(entries, inode_width, block_width);
            break;
    }
}

void Renderer::RenderReport(const std::vector<Entry>& entries) const {
    auto& perf_manager = perf::Manager::Instance();
    std::optional<perf::Timer> timer;
    if (perf_manager.enabled()) {
        timer.emplace("renderer::RenderReport");
        perf_manager.IncrementCounter("reports_rendered");
        perf_manager.IncrementCounter("report_entries", static_cast<std::uint64_t>(entries.size()));
    }

    if (opt_.report() == Config::Report::None) {
        return;
    }
    ReportStats stats = ComputeReportStats(entries);
    std::cout << "\n";
    if (opt_.report() == Config::Report::Long) {
        PrintReportLong(stats);
    } else {
        PrintReportShort(stats);
    }
}

void Renderer::TerminateLine() const {
    std::cout.put(opt_.zero_terminate() ? '\0' : '\n');
}

std::string Renderer::ApplyControlCharHandling(const std::string& name) const {
    if (!opt_.hide_control_chars()) return name;
    std::string out;
    out.reserve(name.size());
    for (unsigned char ch : name) {
        out.push_back(IsNonGraphic(ch) ? '?' : static_cast<char>(ch));
    }
    return out;
}

std::string Renderer::ApplyQuoting(const std::string& name) const {
    using QS = Config::QuotingStyle;
    switch (opt_.quoting_style()) {
        case QS::Literal:
            return name;
        case QS::Locale:
        case QS::C:
            return CStyleEscape(name, true, false);
        case QS::Escape:
            return CStyleEscape(name, false, false);
        case QS::Shell:
            return ShellQuote(name, false);
        case QS::ShellAlways:
            return ShellQuote(name, true);
        case QS::ShellEscape:
            return ShellEscape(name, false);
        case QS::ShellEscapeAlways:
            return ShellEscape(name, true);
    }
    return name;
}

std::string Renderer::BaseDisplayName(const Entry& entry) const {
    std::string name = ApplyControlCharHandling(entry.info.name);
    if (opt_.indicator() == Config::IndicatorStyle::Slash && entry.info.is_dir) name.push_back('/');
    name = ApplyQuoting(name);
    if (!entry.info.icon.empty()) {
        return entry.info.icon + std::string(" ") + name;
    }
    return name;
}

std::string Renderer::FileUri(const fs::path& path) const {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec) abs = path;
    abs = abs.lexically_normal();
    std::string generic = abs.generic_string();
#ifdef _WIN32
    if (generic.size() >= 2 && generic[1] == ':') {
        generic.insert(generic.begin(), '/');
    }
#endif
    return std::string("file://") + PercentEncode(generic);
}

std::string Renderer::StyledName(const Entry& entry) const {
    std::string label = BaseDisplayName(entry);
    std::string out;
    const ThemeColors& theme = Theme::instance().colors();
    if (opt_.hyperlink()) {
        out += "\x1b]8;;";
        out += FileUri(entry.info.path);
        out += "\x1b\\";
    }
    if (!opt_.no_color() && !entry.info.color_fg.empty()) out += entry.info.color_fg;
    out += label;
    if (!opt_.no_color() && !entry.info.color_fg.empty()) {
        if (!entry.info.color_reset.empty()) {
            out += entry.info.color_reset;
        } else {
            out += theme.reset;
        }
    }
    if (opt_.hyperlink()) {
        out += "\x1b]8;;\x1b\\";
    }
    return out;
}

std::string Renderer::BlockDisplay(const Entry& entry) const {
    std::optional<uintmax_t> allocated;
    if (entry.info.has_allocated_size) {
        allocated = entry.info.allocated_size;
    }
    return size_formatter_.FormatBlocks(entry.info.size, allocated);
}

std::string Renderer::FormatSizeValue(uintmax_t size) const {
    return size_formatter_.FormatSize(size);
}

size_t Renderer::ComputeInodeWidth(const std::vector<Entry>& entries) const {
    if (!opt_.show_inode()) return 0;
    size_t width = 0;
    for (const auto& entry : entries) {
        std::string s = std::to_string(entry.info.inode);
        width = std::max(width, s.size());
    }
    return width;
}

size_t Renderer::ComputeBlockWidth(const std::vector<Entry>& entries) const {
    if (!opt_.show_block_size()) return 0;
    size_t width = 0;
    for (const auto& entry : entries) {
        std::string block = BlockDisplay(entry);
        width = std::max(width, block.size());
    }
    return width;
}

size_t Renderer::PrintableWidth(const std::string& text) const {
    size_t w = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0x1b) {
            size_t j = i + 1;
            if (j < text.size()) {
                unsigned char next = static_cast<unsigned char>(text[j]);
                if (next == '[') {
                    ++j;
                    while (j < text.size() && text[j] != 'm') ++j;
                    if (j < text.size()) ++j;
                    i = j;
                    continue;
                } else if (next == ']') {
                    ++j;
                    while (j < text.size()) {
                        if (text[j] == '\x07') { ++j; break; }
                        if (text[j] == '\x1b' && j + 1 < text.size() && text[j + 1] == '\\') {
                            j += 2;
                            break;
                        }
                        ++j;
                    }
                    i = j;
                    continue;
                }
            }
            ++i;
            continue;
        }

        if (c == '\t') {
            int tab = opt_.tab_size();
            if (tab > 0) {
                size_t tabsize = static_cast<size_t>(tab);
                size_t remainder = w % tabsize;
                size_t advance = remainder == 0 ? tabsize : (tabsize - remainder);
                w += advance;
            }
            ++i;
            continue;
        }

        size_t adv = 1;
        if      ((c & 0x80u) == 0x00u) adv = 1;
        else if ((c & 0xE0u) == 0xC0u && i + 1 < text.size()) adv = 2;
        else if ((c & 0xF0u) == 0xE0u && i + 2 < text.size()) adv = 3;
        else if ((c & 0xF8u) == 0xF0u && i + 3 < text.size()) adv = 4;
        i += adv;
        w += 1;
    }
    return w;
}

int Renderer::EffectiveTerminalWidth() const {
    if (opt_.output_width().has_value()) {
        int value = *opt_.output_width();
        if (value <= 0) {
            return std::numeric_limits<int>::max();
        }
        return value;
    }
    return Platform::terminalWidth();
}

std::string Renderer::FormatEntryCell(const Entry& entry,
                                      size_t inode_width,
                                      size_t block_width,
                                      bool include_git_prefix) const {
    std::string out;
    const ThemeColors* theme = opt_.no_color() ? nullptr : &Theme::instance().colors();
    if (opt_.show_inode()) {
        std::string inode = std::to_string(entry.info.inode);
        if (inode_width > inode.size()) out.append(inode_width - inode.size(), ' ');
        if (theme) {
            const std::string& color = theme->get("inode");
            if (!color.empty()) {
                out += color;
                out += inode;
                out += theme->reset;
            } else {
                out += inode;
            }
        } else {
            out += inode;
        }
        out.push_back(' ');
    }
    if (opt_.show_block_size()) {
        std::string block = BlockDisplay(entry);
        if (block_width > block.size()) out.append(block_width - block.size(), ' ');
        out += block;
        out.push_back(' ');
    }
    if (include_git_prefix && opt_.git_status() && !entry.info.git_prefix.empty()) {
        out += entry.info.git_prefix;
        out.push_back(' ');
    }
    out += StyledName(entry);
    return out;
}

std::string Renderer::TreePrefix(const std::vector<bool>& branches, bool is_last) const {
    std::string prefix;
    prefix.reserve(branches.size() * 4 + 5);
    for (bool branch : branches) {
        prefix += branch ? " │  " : "    ";
    }
    prefix += is_last ? " └── " : " ├── ";
    return prefix;
}

void Renderer::PrintTreeNodes(const std::vector<TreeItem>& nodes,
                              size_t inode_width,
                              size_t block_width,
                              std::vector<bool>& branch_stack) const {
    const ThemeColors& theme = Theme::instance().colors();
    for (size_t i = 0; i < nodes.size(); ++i) {
        const TreeItem& node = nodes[i];
        bool is_last = (i + 1 == nodes.size());
        std::string prefix = TreePrefix(branch_stack, is_last);
        std::cout << apply_color(theme.get("tree"), prefix, theme, opt_.no_color());
        std::cout << FormatEntryCell(node.entry, inode_width, block_width, true);
        TerminateLine();
        if (!node.children.empty()) {
            branch_stack.push_back(!is_last);
            PrintTreeNodes(node.children, inode_width, block_width, branch_stack);
            branch_stack.pop_back();
        }
    }
}

void Renderer::PrintLong(const std::vector<Entry>& entries,
                         size_t inode_width,
                         size_t block_width) const {
    constexpr size_t perm_width = 10;
    const ThemeColors& theme = Theme::instance().colors();

    auto owner_display = [&](const Entry& entry) -> std::string {
        if (opt_.numeric_uid_gid()) {
            if (entry.info.has_owner_numeric) {
                return entry.info.owner_numeric;
            }
            if (entry.info.has_owner_id) {
                return std::to_string(entry.info.owner_id);
            }
        }
        if (!entry.info.owner.empty()) {
            return entry.info.owner;
        }
        if (entry.info.has_owner_numeric) {
            return entry.info.owner_numeric;
        }
        if (entry.info.has_owner_id) {
            return std::to_string(entry.info.owner_id);
        }
        return std::string();
    };

    auto group_display = [&](const Entry& entry) -> std::string {
        if (opt_.numeric_uid_gid()) {
            if (entry.info.has_group_numeric) {
                return entry.info.group_numeric;
            }
            if (entry.info.has_group_id) {
                return std::to_string(entry.info.group_id);
            }
        }
        if (!entry.info.group.empty()) {
            return entry.info.group;
        }
        if (entry.info.has_group_numeric) {
            return entry.info.group_numeric;
        }
        if (entry.info.has_group_id) {
            return std::to_string(entry.info.group_id);
        }
        return std::string();
    };

    size_t w_owner = 0, w_group = 0, w_size = 0, w_nlink = 0, w_time = 0, w_git = 0, w_blocks = block_width;
    for (const auto& entry : entries) {
        if (opt_.show_owner()) w_owner = std::max(w_owner, owner_display(entry).size());
        if (opt_.show_group()) w_group = std::max(w_group, group_display(entry).size());
        w_nlink = std::max(w_nlink, std::to_string(entry.info.nlink).size());
        std::string size_str = FormatSizeValue(entry.info.size);
        w_size = std::max(w_size, size_str.size());
        std::string time_str = time_formatter_.Format(entry.info.mtime);
        w_time = std::max(w_time, time_str.size());
        if (opt_.git_status()) {
            w_git = std::max(w_git, PrintableWidth(entry.info.git_prefix));
        }
        if (opt_.show_block_size()) {
            std::string block = BlockDisplay(entry);
            w_blocks = std::max(w_blocks, block.size());
        }
    }

    const std::string inode_color = opt_.no_color() ? std::string() : theme.get("inode");
    const std::string links_color = inode_color;
    const std::string owner_color = opt_.no_color() ? std::string() : theme.get("owned");
    const std::string group_color = opt_.no_color() ? std::string() : theme.get("group");

    const std::string size_header = opt_.bytes() ? "Length" : "Size";
    const std::string links_header = "Links";
    const std::string owner_header = "Owner";
    const std::string group_header = "Group";
    const std::string time_header = "LastWriteTime";
    const std::string inode_header = "Inode";
    const std::string blocks_header = "Blocks";
    const std::string git_header = "Git";
    const std::string name_header = "Name";

    enum class HeaderAlign { Left, Right };
    const std::string& header_color = theme.get("header_names");
    auto format_header_cell = [&](const std::string& text, size_t width, HeaderAlign align) {
        std::ostringstream oss;
        if (align == HeaderAlign::Left) {
            oss << std::left;
        } else {
            oss << std::right;
        }
        if (width > 0) {
            oss << std::setw(static_cast<int>(width));
        }
        oss << text;
        return apply_color(header_color, oss.str(), theme, opt_.no_color());
    };
    auto format_simple_header = [&](const std::string& text) {
        return apply_color(header_color, text, theme, opt_.no_color());
    };

    if (opt_.header()) {
        if (opt_.show_inode()) inode_width = std::max(inode_width, inode_header.size());
        w_nlink = std::max(w_nlink, links_header.size());
        if (opt_.show_owner()) w_owner = std::max(w_owner, owner_header.size());
        if (opt_.show_group()) w_group = std::max(w_group, group_header.size());
        w_size = std::max(w_size, size_header.size());
        w_time = std::max(w_time, time_header.size());
        if (opt_.show_block_size()) w_blocks = std::max(w_blocks, blocks_header.size());
        if (opt_.git_status()) w_git = std::max(w_git, git_header.size());
    }

    if (opt_.header()) {
        if (opt_.show_inode()) {
            std::cout << format_header_cell(inode_header, inode_width, HeaderAlign::Right) << ' ';
        }
        if (opt_.show_block_size()) {
            std::cout << format_header_cell(blocks_header, w_blocks, HeaderAlign::Right) << ' ';
        }
        std::cout << format_header_cell("Mode", perm_width, HeaderAlign::Left) << ' ';
        std::cout << format_header_cell(links_header, w_nlink, HeaderAlign::Right) << ' ';
        if (opt_.show_owner()) {
            std::cout << format_header_cell(owner_header, w_owner, HeaderAlign::Left) << ' ';
        }
        if (opt_.show_group()) {
            std::cout << format_header_cell(group_header, w_group, HeaderAlign::Left) << ' ';
        }
        std::cout << format_header_cell(size_header, w_size, HeaderAlign::Right) << ' ';
        std::cout << format_header_cell(time_header, w_time, HeaderAlign::Left) << ' ';
        if (opt_.git_status()) {
            std::cout << format_header_cell(git_header, w_git, HeaderAlign::Left) << ' ';
        }
        std::cout << format_simple_header(name_header) << "\n";

        if (opt_.show_inode()) {
            std::cout << std::string(inode_width, '-') << ' ';
        }
        if (opt_.show_block_size()) {
            std::cout << std::string(w_blocks, '-') << ' ';
        }
        std::cout << std::string(perm_width, '-') << ' ';
        std::cout << std::string(w_nlink, '-') << ' ';
        if (opt_.show_owner()) std::cout << std::string(w_owner, '-') << ' ';
        if (opt_.show_group()) std::cout << std::string(w_group, '-') << ' ';
        std::cout << std::string(w_size, '-') << ' ';
        std::cout << std::string(w_time, '-') << ' ';
        if (opt_.git_status()) std::cout << std::string(w_git, '-') << ' ';
        std::cout << std::string(name_header.size(), '-') << "\n";
    }

    for (const auto& entry : entries) {
        if (opt_.show_inode()) {
            std::cout << std::right;
            if (!inode_color.empty()) std::cout << inode_color;
            std::cout << std::setw(static_cast<int>(inode_width)) << entry.info.inode;
            if (!inode_color.empty()) std::cout << theme.reset;
            std::cout << ' ';
        }
        if (opt_.show_block_size()) {
            std::string block = BlockDisplay(entry);
            std::cout << std::right << std::setw(static_cast<int>(w_blocks)) << block << ' ';
        }

        std::string perm = permission_formatter_.Format(entry.info);
        std::cout << permission_formatter_.Colorize(perm, opt_.no_color()) << ' ';

        std::cout << std::right;
        if (!links_color.empty()) std::cout << links_color;
        std::cout << std::setw(static_cast<int>(w_nlink)) << entry.info.nlink;
        if (!links_color.empty()) std::cout << theme.reset;
        std::cout << ' ';

        if (opt_.show_owner()) {
            std::string owner_text = owner_display(entry);
            if (!owner_color.empty()) std::cout << owner_color;
            std::cout << std::left << std::setw(static_cast<int>(w_owner)) << owner_text;
            if (!owner_color.empty()) std::cout << theme.reset;
            std::cout << ' ';
        }
        if (opt_.show_group()) {
            std::string group_text = group_display(entry);
            if (!group_color.empty()) std::cout << group_color;
            std::cout << std::left << std::setw(static_cast<int>(w_group)) << group_text;
            if (!group_color.empty()) std::cout << theme.reset;
            std::cout << ' ';
        }

        std::string size_str = FormatSizeValue(entry.info.size);
        std::string size_col = opt_.no_color() ? std::string() : SizeColor(entry.info.size, theme);
        if (!size_col.empty()) std::cout << size_col;
        std::cout << std::right << std::setw(static_cast<int>(w_size)) << size_str;
        if (!size_col.empty()) std::cout << theme.reset;
        std::cout << ' ';

        std::string time_str = time_formatter_.Format(entry.info.mtime);
        std::string time_col = opt_.no_color() ? std::string() : AgeColor(entry.info.mtime, theme);
        if (!time_col.empty()) std::cout << time_col;
        if (opt_.header()) {
            std::cout << std::left << std::setw(static_cast<int>(w_time)) << time_str;
        } else {
            std::cout << time_str;
        }
        if (!time_col.empty()) std::cout << theme.reset;
        std::cout << ' ';

        if (opt_.git_status()) {
            if (opt_.header()) {
                std::cout << entry.info.git_prefix;
                size_t git_width = PrintableWidth(entry.info.git_prefix);
                if (w_git > git_width) {
                    std::cout << std::string(w_git - git_width, ' ');
                }
                std::cout << ' ';
            } else if (!entry.info.git_prefix.empty()) {
                std::cout << entry.info.git_prefix << ' ';
            }
        }

        std::cout << StyledName(entry);

        if (entry.info.is_symlink) {
            std::string target_str;
            if (entry.info.has_symlink_target) {
                target_str = entry.info.symlink_target.string();
            } else {
                std::error_code target_ec;
                auto target = fs::read_symlink(entry.info.path, target_ec);
                if (!target_ec) target_str = target.string();
            }
            if (!target_str.empty()) {
                target_str = ApplyControlCharHandling(target_str);
                target_str = ApplyQuoting(target_str);
                const char* arrow = "  \xE2\x87\x92 ";
                bool broken = entry.info.is_broken_symlink;
                bool use_color = !opt_.no_color();
                std::string link_color;
                if (use_color) {
                    link_color = broken ? theme.get("dead_link") : theme.get("link");
                    if (link_color.empty()) use_color = false;
                }

                if (use_color) std::cout << link_color;
                std::cout << arrow;
                std::cout << target_str;
                if (broken) {
                    std::cout << " [Dead link]";
                }
                if (use_color) std::cout << theme.reset;
            }
        }

        TerminateLine();
    }
}

void Renderer::PrintColumns(const std::vector<Entry>& entries,
                            size_t inode_width,
                            size_t block_width) const {
    struct Cell { std::string text; size_t width; };
    std::vector<Cell> cells;
    cells.reserve(entries.size());

    size_t maxw = 0;
    for (const auto& entry : entries) {
        std::string cell = FormatEntryCell(entry, inode_width, block_width, true);
        size_t w = PrintableWidth(cell);
        maxw = std::max(maxw, w);
        cells.push_back({std::move(cell), w});
    }

    if (cells.empty()) return;

    const size_t gutter = 2;
    size_t per_row = 1;
    if (maxw > 0) {
        int cols = EffectiveTerminalWidth();
        if (cols <= 0) cols = 1;
        size_t denom = maxw + gutter;
        if (denom == 0) denom = 1;
        per_row = std::max<size_t>(1, static_cast<size_t>(cols) / denom);
    }
    size_t rows = (cells.size() + per_row - 1) / per_row;

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < per_row; ++c) {
            size_t idx;
            if (opt_.format() == Config::Format::ColumnsHorizontal) {
                idx = r * per_row + c;
            } else {
                idx = c * rows + r;
            }
            if (idx >= cells.size()) break;
            const auto& cell = cells[idx];
            std::cout << cell.text;

            size_t next;
            if (opt_.format() == Config::Format::ColumnsHorizontal) {
                next = r * per_row + (c + 1);
            } else {
                next = (c + 1) * rows + r;
            }
            if (next < cells.size()) {
                size_t pad = gutter;
                if (cell.width < maxw) pad += (maxw - cell.width);
                std::cout << std::string(pad, ' ');
            }
        }
        TerminateLine();
    }
}

void Renderer::PrintCommaSeparated(const std::vector<Entry>& entries,
                                   size_t inode_width,
                                   size_t block_width) const {
    if (entries.empty()) {
        TerminateLine();
        return;
    }

    int cols = EffectiveTerminalWidth();
    if (cols <= 0) cols = 1;
    bool unlimited = (cols == std::numeric_limits<int>::max());
    size_t limit = unlimited ? std::numeric_limits<size_t>::max()
                             : static_cast<size_t>(cols);

    size_t current = 0;
    bool first = true;
    for (const auto& entry : entries) {
        std::string text = FormatEntryCell(entry, inode_width, block_width, true);
        size_t width = PrintableWidth(text);
        size_t separator_width = first ? 0 : 2;

        if (!first) {
            if (!unlimited && (current > limit || limit - current < separator_width + width)) {
                TerminateLine();
                current = 0;
                first = true;
            }
        }

        if (!first) {
            std::cout << ", ";
            current += separator_width;
        }

        std::cout << text;
        current += width;
        first = false;
    }

    TerminateLine();
}

Renderer::ReportStats Renderer::ComputeReportStats(const std::vector<Entry>& entries) const {
    ReportStats stats;
    stats.total = entries.size();
    for (const auto& entry : entries) {
        bool is_directory = entry.info.is_dir && !entry.info.is_symlink;
        if (is_directory) {
            ++stats.folders;
        } else {
            if (entry.info.has_recognized_icon) {
                ++stats.recognized_files;
            } else {
                ++stats.unrecognized_files;
            }
            stats.total_size += entry.info.size;
        }
        if (entry.info.is_symlink) {
            ++stats.links;
            if (entry.info.is_broken_symlink) {
                ++stats.dead_links;
            }
        }
    }
    return stats;
}

void Renderer::PrintReportShort(const ReportStats& stats) const {
    std::cout << "    Folders: " << stats.folders
              << ", Files: " << stats.files()
              << ", Size: "
              << (opt_.bytes() ? std::to_string(stats.total_size)
                               : SizeFormatter::FormatHumanReadable(stats.total_size))
              << ".\n\n";
}

void Renderer::PrintReportLong(const ReportStats& stats) const {
    std::cout << "    Found " << stats.total << ' '
              << (stats.total == 1 ? "item" : "items")
              << " in total.\n\n";
    std::cout << "        Folders                 : " << stats.folders << "\n";
    std::cout << "        Recognized files        : " << stats.recognized_files << "\n";
    std::cout << "        Unrecognized files      : " << stats.unrecognized_files << "\n";
    std::cout << "        Links                   : " << stats.links << "\n";
    std::cout << "        Dead links              : " << stats.dead_links << "\n";
    std::cout << "        Total displayed size    : "
              << (opt_.bytes() ? std::to_string(stats.total_size)
                               : SizeFormatter::FormatHumanReadable(stats.total_size))
              << "\n\n";
}

}  // namespace nls

