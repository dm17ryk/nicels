#include "util.h"
#include "colors.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <limits>

#include <CLI/CLI.hpp>

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#include <Aclapi.h>
#include <sddl.h>
#endif

namespace fs = std::filesystem;

namespace nls {

static const std::map<std::string, Options::QuotingStyle>& quoting_style_map() {
    static const std::map<std::string, Options::QuotingStyle> map{
        {"literal", Options::QuotingStyle::Literal},
        {"locale", Options::QuotingStyle::Locale},
        {"shell", Options::QuotingStyle::Shell},
        {"shell-always", Options::QuotingStyle::ShellAlways},
        {"shell-escape", Options::QuotingStyle::ShellEscape},
        {"shell-escape-always", Options::QuotingStyle::ShellEscapeAlways},
        {"c", Options::QuotingStyle::C},
        {"escape", Options::QuotingStyle::Escape}
    };
    return map;
}

static std::optional<Options::QuotingStyle> parse_quoting_style_word(std::string word) {
    word = to_lower(std::move(word));
    const auto& mapping = quoting_style_map();
    auto it = mapping.find(word);
    if (it != mapping.end()) {
        return it->second;
    }
    return std::nullopt;
}

namespace {

static bool should_colorize_help() {
    static const bool colorize = [] {
        if (std::getenv("NO_COLOR") != nullptr) {
            return false;
        }

#ifdef _WIN32
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        if (GetFileType(handle) != FILE_TYPE_CHAR) {
            return false;
        }
        DWORD mode = 0;
        return GetConsoleMode(handle, &mode) != 0;
#else
        return ::isatty(STDOUT_FILENO) != 0;
#endif
    }();
    return colorize;
}

static std::string color_text(std::string_view text,
    std::string_view theme_key,
    std::string_view fallback_color)
{
    if (!should_colorize_help()) {
        return std::string(text);
    }

    const ThemeColors& theme = active_theme();
    const std::string color = theme.color_or(theme_key, fallback_color);
    return apply_color(color, text, theme, false);
}

class ColorFormatter : public CLI::Formatter {
public:
    std::string make_usage(const CLI::App* app, std::string name) const override {
        std::ostringstream out;
        out << '\n';

        out << color_text(get_label("Usage"), "help_usage_label", "\x1b[33m") << ':';
        if (!name.empty()) {
            out << ' ' << color_text(name, "help_usage_command", "\x1b[33m");
        }

        std::vector<const CLI::Option*> non_positional =
            app->get_options([](const CLI::Option* opt) { return opt->nonpositional(); });
        if (!non_positional.empty()) {
            out << " [" << get_label("OPTIONS") << "]";
        }

        std::vector<const CLI::Option*> positionals =
            app->get_options([](const CLI::Option* opt) { return opt->get_positional(); });
        if (!positionals.empty()) {
            std::vector<std::string> positional_names(positionals.size());
            std::transform(positionals.begin(), positionals.end(), positional_names.begin(),
                [this](const CLI::Option* opt) { return make_option_usage(opt); });
            out << " " << CLI::detail::join(positional_names, " ");
        }

        if (!app->get_subcommands(
                [](const CLI::App* subc) {
                    return (!subc->get_disabled()) && (!subc->get_name().empty());
                })
                .empty()) {
            out << ' ' << (app->get_require_subcommand_min() == 0 ? "[" : "")
                << get_label(app->get_require_subcommand_max() == 1 ? "SUBCOMMAND" : "SUBCOMMANDS")
                << (app->get_require_subcommand_min() == 0 ? "]" : "");
        }

        out << "\n\n";
        return out.str();
    }

    std::string make_group(std::string group, bool is_positional, std::vector<const CLI::Option*> opts) const override {
        if (opts.empty()) {
            return {};
        }

        std::ostringstream out;
        out << "\n";
        if (!group.empty()) {
            out << color_text(group, "help_option_group", "\x1b[36m");
        }
        out << ":\n";
        for (const CLI::Option* opt : opts) {
            out << make_option(opt, is_positional);
        }
        return out.str();
    }

    std::string make_option_name(const CLI::Option *opt, bool is_positional) const override {
        return color_text(Formatter::make_option_name(opt, is_positional),
            "help_option_name", "\x1b[33m");
    }

    std::string make_option_opts(const CLI::Option* opt) const override {
        return color_text(Formatter::make_option_opts(opt),
            "help_option_opts", "\x1b[34m");
    }

    std::string make_option_desc(const CLI::Option *opt) const override {
        return color_text(Formatter::make_option_desc(opt),
            "help_option_desc", "\x1b[32m");
    }

    std::string make_footer(const CLI::App *app) const override {
        std::string footer = Formatter::make_footer(app);
        if (footer.empty()) {
            return footer;
        }
        return color_text(footer, "help_footer", "\x1b[35m");
    }

    std::string make_description(const CLI::App *app) const override {
        std::string desc = Formatter::make_description(app);
        if (desc.empty()) {
            return desc;
        }
        return color_text(desc, "help_description", "\x1b[35m");
    }

    std::string make_subcommand(const CLI::App *sub) const override {
        std::string cmd = Formatter::make_subcommand(sub);
        if (cmd.empty()) {
            return cmd;
        }
        return color_text(cmd, "help_option_group", "\x1b[33m");
    }

    std::string make_expanded(const CLI::App *sub, CLI::AppFormatMode mode) const override {
        std::string cmd = Formatter::make_expanded(sub, mode);
        if (cmd.empty()) {
            return cmd;
        }
        return color_text(cmd, "help_option_group", "\x1b[33m");
    }
};

struct SizeSpec {
    uintmax_t value = 0;
    bool show_suffix = false;
    std::string suffix;
};

static bool multiply_with_overflow(uintmax_t a, uintmax_t b, uintmax_t& result) {
    if (a == 0 || b == 0) {
        result = 0;
        return true;
    }
    if (a > std::numeric_limits<uintmax_t>::max() / b) {
        return false;
    }
    result = a * b;
    return true;
}

static bool pow_with_overflow(uintmax_t base, unsigned exponent, uintmax_t& result) {
    result = 1;
    for (unsigned i = 0; i < exponent; ++i) {
        if (!multiply_with_overflow(result, base, result)) {
            return false;
        }
    }
    return true;
}

static std::optional<SizeSpec> parse_size_spec(const std::string& text) {
    if (text.empty()) return std::nullopt;

    size_t pos = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }

    std::string number_part = text.substr(0, pos);
    std::string suffix_part = text.substr(pos);
    if (number_part.empty() && suffix_part.empty()) {
        return std::nullopt;
    }

    uintmax_t number = 1;
    if (!number_part.empty()) {
        try {
            size_t idx = 0;
            unsigned long long parsed = std::stoull(number_part, &idx, 10);
            if (idx != number_part.size()) return std::nullopt;
            number = static_cast<uintmax_t>(parsed);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    uintmax_t multiplier = 1;
    if (!suffix_part.empty()) {
        std::string upper;
        upper.reserve(suffix_part.size());
        for (char ch : suffix_part) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }

        bool binary = true;
        std::string base = upper;
        if (!base.empty() && base.back() == 'B') {
            if (base.size() >= 2 && base[base.size() - 2] == 'I') {
                binary = true;
                base.erase(base.end() - 2, base.end());
            } else {
                binary = false;
                base.pop_back();
            }
        } else {
            binary = true;
        }

        if (base.empty()) {
            return std::nullopt;
        }

        const std::string letters = "KMGTPEZYRQ";
        auto it = letters.find(base);
        if (it == std::string::npos) {
            return std::nullopt;
        }
        unsigned exponent = static_cast<unsigned>(it) + 1;
        uintmax_t base_value = binary ? 1024u : 1000u;
        if (!pow_with_overflow(base_value, exponent, multiplier)) {
            return std::nullopt;
        }
    }

    uintmax_t scaled = 0;
    if (!multiply_with_overflow(number, multiplier, scaled)) {
        return std::nullopt;
    }

    SizeSpec spec;
    spec.value = scaled;
    spec.show_suffix = suffix_part.size() > 0 && number_part.empty();
    spec.suffix = suffix_part;
    return spec;
}

} // namespace

Options parse_args(int argc, char** argv) {
    Options opt;

    if (const char* env = std::getenv("QUOTING_STYLE")) {
        if (auto style = parse_quoting_style_word(env)) {
            opt.quoting_style = *style;
        }
    }

    CLI::App program{R"(List information about the FILEs (the current directory by default).
Sort entries alphabetically if none of -cftuvSUX nor --sort is specified.)", "nls"};
    program.formatter(std::make_shared<ColorFormatter>());
    program.set_version_flag("--version", "1.0.0");
    program.footer(R"(The SIZE argument is an integer and optional unit (example: 10K is 10*1024).
Units are K,M,G,T,P,E,Z,Y,R,Q (powers of 1024) or KB,MB,... (powers of 1000).
Binary prefixes can be used, too: KiB=K, MiB=M, and so on.

The TIME_STYLE argument can be full-iso, long-iso, iso, locale, or +FORMAT.
FORMAT is interpreted like in date(1). If FORMAT is FORMAT1<newline>FORMAT2,
then FORMAT1 applies to non-recent files and FORMAT2 to recent files.
TIME_STYLE prefixed with 'posix-' takes effect only outside the POSIX locale.
Also the TIME_STYLE environment variable sets the default style to use.

The WHEN argument defaults to 'always' and can also be 'auto' or 'never'.

Using color to distinguish file types is disabled both by default and
with --color=never. With --color=auto, ls emits color codes only when
standard output is connected to a terminal. The LS_COLORS environment
variable can change the settings. Use the dircolors(1) command to set it.

Exit status:
 0  if OK,
 1  if minor problems (e.g., cannot access subdirectory),
 2  if serious trouble (e.g., cannot access command-line argument).)"
    );

    program.add_option("paths", opt.paths, "paths to list")->type_name("PATH");

    const std::map<std::string, Options::Format> format_map{
        {"long", Options::Format::Long},
        {"l", Options::Format::Long},
        {"single-column", Options::Format::SingleColumn},
        {"single", Options::Format::SingleColumn},
        {"1", Options::Format::SingleColumn},
        {"across", Options::Format::ColumnsHorizontal},
        {"horizontal", Options::Format::ColumnsHorizontal},
        {"x", Options::Format::ColumnsHorizontal},
        {"vertical", Options::Format::ColumnsVertical},
        {"columns", Options::Format::ColumnsVertical},
        {"column", Options::Format::ColumnsVertical},
        {"c", Options::Format::ColumnsVertical},
        {"comma", Options::Format::CommaSeparated},
        {"commas", Options::Format::CommaSeparated},
        {"m", Options::Format::CommaSeparated}
    };

    const std::map<std::string, Options::Sort> sort_map{
        {"none", Options::Sort::None},
        {"name", Options::Sort::Name},
        {"time", Options::Sort::Time},
        {"mtime", Options::Sort::Time},
        {"size", Options::Sort::Size},
        {"extension", Options::Sort::Extension},
        {"ext", Options::Sort::Extension}
    };

    const std::map<std::string, Options::IndicatorStyle> indicator_map{
        {"slash", Options::IndicatorStyle::Slash},
        {"slashes", Options::IndicatorStyle::Slash},
        {"none", Options::IndicatorStyle::None},
        {"off", Options::IndicatorStyle::None}
    };

    const auto& quoting_map = quoting_style_map();

    const std::map<std::string, Options::Report> report_map{
        {"long", Options::Report::Long},
        {"short", Options::Report::Short}
    };

    enum class ColorMode { Auto, Always, Never };
    const std::map<std::string, ColorMode> color_map{
        {"auto", ColorMode::Auto},
        {"always", ColorMode::Always},
        {"never", ColorMode::Never}
    };

    auto layout = program.add_option_group("Layout options");
    layout->add_flag_callback("-l,--long", [&]() { opt.format = Options::Format::Long; },
        "use a long listing format");
    layout->add_flag_callback("-1,--one-per-line", [&]() { opt.format = Options::Format::SingleColumn; },
        "list one file per line");
    layout->add_flag_callback("-x", [&]() { opt.format = Options::Format::ColumnsHorizontal; },
        "list entries by lines instead of by columns");
    layout->add_flag_callback("-C", [&]() { opt.format = Options::Format::ColumnsVertical; },
        "list entries by columns instead of by lines");

    auto format_option = layout->add_option("--format", opt.format,
        R"(use format: across (-x), horizontal (-x),
long (-l), single-column (-1), vertical (-C)
or comma (-m) (default: vertical))");
    format_option->type_name("WORD");
    format_option->transform(
        CLI::CheckedTransformer(format_map, CLI::ignore_case)
            .description(""));
    format_option->default_str("vertical");

    layout->add_flag_callback("--header", [&]() { opt.header = true; },
        "print directory header and column names in long listing");
    layout->add_flag_callback("-m", [&]() { opt.format = Options::Format::CommaSeparated; },
        "fill width with a comma separated list of entries");

    auto tab_option = layout->add_option_function<int>("-T,--tabsize",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--tabsize", "COLS must be non-negative");
            }
            opt.tab_size = cols;
        },
        "assume tab stops at each COLS instead of 8");
    tab_option->type_name("COLS");
    tab_option->expected(1);

    auto width_option = layout->add_option_function<int>("-w,--width",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--width", "COLS must be non-negative");
            }
            opt.output_width = cols;
        },
        "set output width to COLS.  0 means no limit");
    width_option->type_name("COLS");
    width_option->expected(0, 256);

    auto tree_option = layout->add_option_function<std::size_t>("--tree",
        [&](const std::size_t& depth) {
            if (depth == 0) {
                throw CLI::ValidationError("--tree", "DEPTH must be greater than zero");
            }
            opt.tree = true;
            opt.tree_depth = depth;
        },
        "show tree view of directories, optionally limited to DEPTH");
    tree_option->type_name("DEPTH");
    tree_option->expected(1);

    auto report_option = layout->add_option_function<Options::Report>("--report",
        [&](const Options::Report& report) {
            opt.report = report;
        },
        R"(show summary report: short, long (default: long)
 )");
    report_option->type_name("WORD");
    report_option->expected(1);
    report_option->transform(
        CLI::CheckedTransformer(report_map, CLI::ignore_case)
            .description(""));
    report_option->default_str("long");

    layout->add_flag_callback("--zero", [&]() { opt.zero_terminate = true; },
        "end each output line with NUL, not newline");

    auto filtering = program.add_option_group("Filtering options");
    filtering->add_flag_callback("-a,--all", [&]() { opt.all = true; },
        "do not ignore entries starting with .");
    filtering->add_flag_callback("-A,--almost-all", [&]() { opt.almost_all = true; },
        "do not list . and ..");
    filtering->add_flag_callback("-d,--dirs", [&]() {
        opt.dirs_only = true;
        opt.files_only = false;
    }, "show only directories");
    filtering->add_flag_callback("-f,--files", [&]() {
        opt.files_only = true;
        opt.dirs_only = false;
    }, "show only files");
    filtering->add_flag_callback("-B,--ignore-backups", [&]() { opt.ignore_backups = true; },
        "do not list implied entries ending with ~");
    filtering->add_option("--hide", opt.hide_patterns,
        R"(do not list implied entries matching shell
PATTERN (overridden by -a or -A))")->type_name("PATTERN");
    filtering->add_option("-I,--ignore", opt.ignore_patterns,
        "do not list implied entries matching shell PATTERN")->type_name("PATTERN");

    auto sorting = program.add_option_group("Sorting options");
    sorting->add_flag_callback("-t", [&]() { opt.sort = Options::Sort::Time; },
        "sort by modification time, newest first");
    sorting->add_flag_callback("-S", [&]() { opt.sort = Options::Sort::Size; },
        "sort by file size, largest first");
    sorting->add_flag_callback("-X", [&]() { opt.sort = Options::Sort::Extension; },
        "sort by file extension");
    sorting->add_flag_callback("-U", [&]() { opt.sort = Options::Sort::None; },
        "do not sort; list entries in directory order");
    sorting->add_flag_callback("-r,--reverse", [&]() { opt.reverse = true; },
        "reverse order while sorting");

    auto sort_option = sorting->add_option("--sort", opt.sort,
        R"(sort by WORD instead of name: none, size,
time, extension (default: name))");
    sort_option->type_name("WORD");
    sort_option->transform(
        CLI::CheckedTransformer(sort_map, CLI::ignore_case)
            .description(""));
    sort_option->default_str("name");

    sorting->add_flag_callback("--group-directories-first,--sd,--sort-dirs", [&]() {
        opt.group_dirs_first = true;
        opt.sort_files_first = false;
    }, "sort directories before files");
    sorting->add_flag_callback("--sf,--sort-files", [&]() {
        opt.sort_files_first = true;
        opt.group_dirs_first = false;
    }, "sort files first");
    sorting->add_flag_callback("--df,--dots-first", [&]() { opt.dots_first = true; },
        "sort dot-files and dot-folders first");

    auto appearance = program.add_option_group("Appearance options");
    appearance->add_flag_callback("-b,--escape", [&]() { opt.quoting_style = Options::QuotingStyle::Escape; },
        "print C-style escapes for nongraphic characters");
    appearance->add_flag_callback("-N,--literal", [&]() { opt.quoting_style = Options::QuotingStyle::Literal; },
        "print entry names without quoting");
    appearance->add_flag_callback("-Q,--quote-name", [&]() { opt.quoting_style = Options::QuotingStyle::C; },
        "enclose entry names in double quotes");

    auto quoting_option = appearance->add_option("--quoting-style", opt.quoting_style,
        R"(use quoting style WORD for entry names:
literal, locale, shell, shell-always, shell-escape,
shell-escape-always, c, escape (default: literal))");
    quoting_option->type_name("WORD");
    quoting_option->transform(
        CLI::CheckedTransformer(quoting_map, CLI::ignore_case)
            .description(""));
    quoting_option->default_str("literal");

    appearance->add_flag_callback("-p", [&]() { opt.indicator = Options::IndicatorStyle::Slash; },
        "append / indicator to directories");

    auto indicator_option = appearance->add_option("--indicator-style", opt.indicator,
        R"(append indicator with style STYLE to entry names:
none, slash (-p) (default: slash))");
    indicator_option->type_name("STYLE");
    indicator_option->transform(
        CLI::CheckedTransformer(indicator_map, CLI::ignore_case)
            .description(""));
    indicator_option->default_str("slash");

    appearance->add_flag_callback("--no-icons,--without-icons", [&]() { opt.no_icons = true; },
        "disable icons in output");
    appearance->add_flag_callback("--no-color", [&]() { opt.no_color = true; },
        "disable ANSI colors");

    auto color_option = appearance->add_option_function<ColorMode>("--color",
        [&](const ColorMode& color) {
            // opt.color = color;
            opt.no_color = (color == ColorMode::Never);
        },
        R"(colorize the output: auto, always,
never (default: auto))");
    color_option->type_name("WHEN");
    color_option->transform(
        CLI::CheckedTransformer(color_map, CLI::ignore_case)
            .description(""));
    color_option->default_str("auto");

    appearance->add_flag_callback("--light", [&]() { opt.color_theme = Options::ColorTheme::Light; },
        "use light color scheme");
    appearance->add_flag_callback("--dark", [&]() { opt.color_theme = Options::ColorTheme::Dark; },
        "use dark color scheme");
    appearance->add_flag_callback("-q,--hide-control-chars", [&]() { opt.hide_control_chars = true; },
        "print ? instead of nongraphic characters");
    appearance->add_flag_callback("--show-control-chars", [&]() { opt.hide_control_chars = false; },
        "show nongraphic characters as-is");
    appearance->add_option("--time-style", opt.time_style,
        R"(use time display format: default, locale,
long-iso, full-iso, iso, iso8601,
+FORMAT (default: locale))")->type_name("FORMAT");
    appearance->add_flag_callback("--full-time", [&]() {
        opt.format = Options::Format::Long;
        opt.time_style = "full-iso";
    }, "like -l --time-style=full-iso");
    appearance->add_flag_callback("--hyperlink", [&]() { opt.hyperlink = true; },
        "emit hyperlinks for entries");

    auto information = program.add_option_group("Information options");
    information->add_flag_callback("-i,--inode", [&]() { opt.show_inode = true; },
        "show inode number");
    information->add_flag_callback("-o", [&]() {
        opt.format = Options::Format::Long;
        opt.show_group = false;
    }, "use a long listing format without group information");
    information->add_flag_callback("-g", [&]() {
        opt.format = Options::Format::Long;
        opt.show_owner = false;
    }, "use a long listing format without owner information");
    information->add_flag_callback("-G,--no-group", [&]() { opt.show_group = false; },
        "show no group information in a long listing");
    information->add_flag_callback("-n,--numeric-uid-gid", [&]() {
        opt.format = Options::Format::Long;
        opt.numeric_uid_gid = true;
    }, "like -l, but list numeric user and group IDs");
    information->add_flag_callback("--bytes,--non-human-readable", [&]() { opt.bytes = true; },
        "show file sizes in bytes");
    information->add_flag_callback("-s,--size", [&]() { opt.show_block_size = true; },
        "print the allocated size of each file, in blocks");

    auto block_option = information->add_option_function<std::string>("--block-size",
        [&](const std::string& text) {
            auto spec = parse_size_spec(text);
            if (!spec) {
                throw CLI::ValidationError("--block-size", "invalid value '" + text + "'");
            }
            opt.block_size = spec->value;
            opt.block_size_specified = true;
            opt.block_size_show_suffix = spec->show_suffix;
            opt.block_size_suffix = spec->suffix;
        },
        "with -l, scale sizes by SIZE when printing them");
    block_option->type_name("SIZE");

    information->add_flag_callback("-L,--dereference", [&]() { opt.dereference = true; },
        R"(when showing file information for a symbolic link,
show information for the file the link references)");
    information->add_flag_callback("--gs,--git-status", [&]() { opt.git_status = true; },
        "show git status for each file");

    try {
        program.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(program.exit(e));
    }

    if (opt.paths.empty()) opt.paths.push_back(".");
    if (opt.all) opt.almost_all = false;
    return opt;
}

bool is_hidden(const std::string& name) {
    return !name.empty() && name[0] == '.';
}

bool iequals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string human_size(uintmax_t bytes) {
    static const char* units[] = {"B","KiB","MiB","GiB","TiB","PiB"};
    double v = static_cast<double>(bytes);
    int idx = 0;
    while (v >= 1024.0 && idx < 5) { v /= 1024.0; ++idx; }
    std::ostringstream oss;
    if (idx == 0) oss << (uintmax_t)v << ' ' << units[idx];
    else oss << std::fixed << std::setprecision(v < 10 ? 1 : 0) << v << ' ' << units[idx];
    return oss.str();
}

// Convert filesystem time to time_t (C++17 portable trick)
static std::time_t to_time_t(fs::file_time_type tp) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        tp - fs::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

static std::string resolve_time_format(const Options& opt) {
    if (opt.time_style.empty() || opt.time_style == "locale" || opt.time_style == "default") {
        return "%a %b %d %H:%M:%S %Y";
    }

    std::string word = to_lower(opt.time_style);
    if (word == "long-iso") {
        return "%Y-%m-%d %H:%M";
    }
    if (word == "full-iso") {
        return "%Y-%m-%d %H:%M:%S %z";
    }
    if (word == "iso" || word == "iso8601") {
        return "%Y-%m-%d";
    }
    if (!opt.time_style.empty() && opt.time_style[0] == '+') {
        return opt.time_style.substr(1);
    }
    return opt.time_style;
}

std::string format_time(const fs::file_time_type& tp, const Options& opt) {
    std::time_t t = to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[128];
    std::string fmt = resolve_time_format(opt);
    if (fmt.empty()) fmt = "%a %b %d %H:%M:%S %Y";
    if (std::strftime(buf, sizeof(buf), fmt.c_str(), &tm) == 0) {
        std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm);
    }
    return buf;
}

std::string perm_string(const fs::directory_entry& de, bool is_symlink_hint, bool dereference) {
    std::error_code ec;
    auto link_status = de.symlink_status(ec);
    if (ec) return "???????????";
    bool is_link = fs::is_symlink(link_status) || is_symlink_hint;

    fs::file_status used_status = link_status;
    bool followed = false;
    if (dereference) {
        std::error_code follow_ec;
        auto follow = de.status(follow_ec);
        if (!follow_ec) {
            used_status = follow;
            followed = true;
        }
    }

    char type = '-';
    if (!followed && is_link) type = 'l';
    else if (fs::is_directory(used_status)) type = 'd';
    else if (fs::is_character_file(used_status)) type = 'c';
    else if (fs::is_block_file(used_status)) type = 'b';
    else if (fs::is_fifo(used_status)) type = 'p';
    else if (fs::is_socket(used_status)) type = 's';
    auto p = used_status.permissions();
    std::string out;
    out += type;
    if (p == fs::perms::unknown) {
        out.append(9, '?');
        return out;
    }

    auto has = [&](fs::perms mask) {
        return (p & mask) != fs::perms::none;
    };

    std::array<bool, 3> can_read = {
        has(fs::perms::owner_read),
        has(fs::perms::group_read),
        has(fs::perms::others_read)
    };
    std::array<bool, 3> can_write = {
        has(fs::perms::owner_write),
        has(fs::perms::group_write),
        has(fs::perms::others_write)
    };
    std::array<bool, 3> can_exec = {
        has(fs::perms::owner_exec),
        has(fs::perms::group_exec),
        has(fs::perms::others_exec)
    };

#ifdef _WIN32
    const auto native_path = de.path();
    DWORD attrs = GetFileAttributesW(native_path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if ((attrs & FILE_ATTRIBUTE_READONLY) != 0) {
            can_write.fill(false);
        } else {
            // Windows reports coarse permission bits where write access for
            // group/others mirrors the owner bit.  Mirror colorls behaviour
            // by suppressing those write flags unless the entry is read-only.
            can_write[1] = false;
            can_write[2] = false;
        }
    }
#endif

    auto append_triplet = [&](int idx, bool special, char special_char) {
        out += can_read[idx] ? 'r' : '-';
        bool writable = can_write[idx];
        out += writable ? 'w' : '-';
        if (special) {
            out += can_exec[idx]
                ? special_char
                : static_cast<char>(std::toupper(static_cast<unsigned char>(special_char)));
        } else {
            out += can_exec[idx] ? 'x' : '-';
        }
    };

    append_triplet(0, has(fs::perms::set_uid), 's');
    append_triplet(1, has(fs::perms::set_gid), 's');
    append_triplet(2, has(fs::perms::sticky_bit), 't');
    return out;
}

// Bootstrap color scheme for permission bits: r=green, w=red, x=yellow.
// Type char: blue for dir, cyan for link.
std::string colorize_perm(const std::string& perm, bool no_color) {
    if (no_color) return perm;
    const auto& theme = active_theme();
    std::string c_r = theme.color_or("read", "\x1b[32m");
    std::string c_w = theme.color_or("write", "\x1b[31m");
    std::string c_x = theme.color_or("exec", "\x1b[33m");
    std::string c_dir = theme.color_or("dir", "\x1b[34m");
    std::string c_link = theme.color_or("link", "\x1b[36m");

    std::string out;
    out.reserve(perm.size() * 5);
    for (size_t i = 0; i < perm.size(); ++i) {
        char ch = perm[i];
        if (i == 0) {
            if (ch == 'd' && !c_dir.empty()) {
                out += c_dir;
                out += ch;
                out += theme.reset;
            } else if (ch == 'l' && !c_link.empty()) {
                out += c_link;
                out += ch;
                out += theme.reset;
            } else {
                out += ch;
            }
        } else if (ch == 'r' && !c_r.empty()) {
            out += c_r;
            out += 'r';
            out += theme.reset;
        } else if (ch == 'w' && !c_w.empty()) {
            out += c_w;
            out += 'w';
            out += theme.reset;
        } else if ((ch == 'x' || ch == 's' || ch == 'S' || ch == 't' || ch == 'T') && !c_x.empty()) {
            out += c_x;
            out += ch;
            out += theme.reset;
        } else {
            out += ch;
        }
    }
    return out;
}

std::optional<fs::path> resolve_symlink_target_path(const FileInfo& fi) {
    if (!fi.is_symlink || !fi.has_symlink_target) {
        return std::nullopt;
    }

    fs::path target = fi.symlink_target;
    if (target.empty()) {
        return std::nullopt;
    }

    if (!target.is_absolute()) {
        fs::path base = fi.path.parent_path();
        if (base.empty()) {
            target = target.lexically_normal();
        } else {
            target = (base / target).lexically_normal();
        }
    } else {
        target = target.lexically_normal();
    }

    return target;
}

void fill_owner_group(FileInfo& fi, bool dereference) {
#ifndef _WIN32
    fi.owner.clear();
    fi.group.clear();
    fi.has_owner_id = false;
    fi.has_group_id = false;
    fi.owner_numeric.clear();
    fi.group_numeric.clear();
    fi.has_owner_numeric = false;
    fi.has_group_numeric = false;
    fi.has_link_size = false;
    fi.allocated_size = 0;
    fi.has_allocated_size = false;

    auto assign_from_stat = [&](const struct stat& st) {
        fi.nlink = st.st_nlink;
        fi.inode = static_cast<uintmax_t>(st.st_ino);
        fi.owner_id = static_cast<uintmax_t>(st.st_uid);
        fi.group_id = static_cast<uintmax_t>(st.st_gid);
        fi.has_owner_id = true;
        fi.has_group_id = true;
        fi.owner_numeric = std::to_string(static_cast<uintmax_t>(st.st_uid));
        fi.group_numeric = std::to_string(static_cast<uintmax_t>(st.st_gid));
        fi.has_owner_numeric = true;
        fi.has_group_numeric = true;
        if (auto* pw = ::getpwuid(st.st_uid)) {
            fi.owner = pw->pw_name;
        } else {
            fi.owner = std::to_string(static_cast<uintmax_t>(st.st_uid));
        }
        if (auto* gr = ::getgrgid(st.st_gid)) {
            fi.group = gr->gr_name;
        } else {
            fi.group = std::to_string(static_cast<uintmax_t>(st.st_gid));
        }
        if (st.st_blocks >= 0) {
            uintmax_t blocks = static_cast<uintmax_t>(st.st_blocks);
            uintmax_t allocated = 0;
            if (!multiply_with_overflow(blocks, static_cast<uintmax_t>(512), allocated)) {
                allocated = std::numeric_limits<uintmax_t>::max();
            }
            fi.allocated_size = allocated;
            fi.has_allocated_size = true;
        }
    };

    struct stat st{};
    if (::lstat(fi.path.c_str(), &st) == 0) {
        assign_from_stat(st);
        fi.link_size = static_cast<uintmax_t>(st.st_size);
        fi.has_link_size = true;
    }

    if (dereference) {
        struct stat st_follow{};
        if (::stat(fi.path.c_str(), &st_follow) == 0) {
            assign_from_stat(st_follow);
        }
    }
#else
    fi.nlink = 1;
    fi.owner.clear();
    fi.group.clear();
    fi.inode = 0;
    fi.has_owner_id = false;
    fi.has_group_id = false;
    fi.owner_numeric.clear();
    fi.group_numeric.clear();
    fi.has_owner_numeric = false;
    fi.has_group_numeric = false;
    fi.has_link_size = false;
    fi.allocated_size = 0;
    fi.has_allocated_size = false;

    bool want_target_attributes = dereference && !fi.is_broken_symlink;
    fs::path query_path = fi.path;
    if (want_target_attributes) {
        if (auto resolved = resolve_symlink_target_path(fi)) {
            query_path = std::move(*resolved);
        }
    }

    std::wstring native_path = query_path.native();

    ::SetLastError(ERROR_SUCCESS);
    DWORD compressed_high = 0;
    DWORD compressed_low = ::GetCompressedFileSizeW(native_path.c_str(), &compressed_high);
    DWORD compressed_err = ::GetLastError();
    if (compressed_low != INVALID_FILE_SIZE || compressed_err == ERROR_SUCCESS) {
        ULARGE_INTEGER comp{};
        comp.HighPart = compressed_high;
        comp.LowPart = compressed_low;
        fi.allocated_size = static_cast<uintmax_t>(comp.QuadPart);
        fi.has_allocated_size = true;
    }

    DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (fi.is_symlink && !want_target_attributes) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    HANDLE handle = ::CreateFileW(native_path.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  share_mode,
                                  nullptr,
                                  OPEN_EXISTING,
                                  flags,
                                  nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION file_info{};
        if (::GetFileInformationByHandle(handle, &file_info)) {
            fi.nlink = file_info.nNumberOfLinks;
            ULARGE_INTEGER index{};
            index.HighPart = file_info.nFileIndexHigh;
            index.LowPart = file_info.nFileIndexLow;
            fi.inode = static_cast<uintmax_t>(index.QuadPart);
            if (fi.is_symlink) {
                ULARGE_INTEGER sz{};
                sz.HighPart = file_info.nFileSizeHigh;
                sz.LowPart = file_info.nFileSizeLow;
                uintmax_t handle_size = static_cast<uintmax_t>(sz.QuadPart);
                if (!want_target_attributes) {
                    fi.link_size = handle_size;
                    fi.has_link_size = true;
                } else if ((file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    fi.size = handle_size;
                }
            }
        }
        ::CloseHandle(handle);
    }

    auto wide_to_utf8 = [](const std::wstring& wide) -> std::string {
        if (wide.empty()) return {};
        int required = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1) return {};
        std::string utf8(static_cast<size_t>(required - 1), '\0');
        int converted = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), required, nullptr, nullptr);
        if (converted <= 0) return {};
        return utf8;
    };

    auto sid_to_string = [&](PSID sid) -> std::string {
        if (sid == nullptr || !::IsValidSid(sid)) return {};
        LPWSTR sid_w = nullptr;
        if (!::ConvertSidToStringSidW(sid, &sid_w)) {
            return {};
        }
        std::wstring sid_native = sid_w ? sid_w : L"";
        if (sid_w) {
            ::LocalFree(sid_w);
        }
        return wide_to_utf8(sid_native);
    };

    auto sid_to_rid = [&](PSID sid) -> std::optional<uintmax_t> {
        if (sid == nullptr || !::IsValidSid(sid)) return std::nullopt;
        auto* count = ::GetSidSubAuthorityCount(sid);
        if (count == nullptr || *count == 0) return std::nullopt;
        DWORD value = *::GetSidSubAuthority(sid, static_cast<DWORD>(*count - 1));
        return static_cast<uintmax_t>(value);
    };

    auto sid_to_account_name = [&](PSID sid) -> std::string {
        if (sid == nullptr || !::IsValidSid(sid)) return {};

        DWORD name_len = 0;
        DWORD domain_len = 0;
        SID_NAME_USE sid_type;
        if (!::LookupAccountSidW(nullptr, sid, nullptr, &name_len, nullptr, &domain_len, &sid_type)) {
            DWORD err = ::GetLastError();
            if (err != ERROR_INSUFFICIENT_BUFFER || name_len == 0) {
                return {};
            }
        }

        std::wstring name(name_len, L'\0');
        std::wstring domain(domain_len, L'\0');
        if (!::LookupAccountSidW(
                nullptr,
                sid,
                name_len ? name.data() : nullptr,
                &name_len,
                domain_len ? domain.data() : nullptr,
                &domain_len,
                &sid_type)) {
            return {};
        }

        name.resize(name_len);
        domain.resize(domain_len);

        if (name.empty()) return {};
        if (!domain.empty()) {
            return wide_to_utf8(domain + L"\\" + name);
        }
        return wide_to_utf8(name);
    };

    PSECURITY_DESCRIPTOR security_descriptor = nullptr;
    PSID owner_sid = nullptr;
    PSID group_sid = nullptr;
    DWORD result = ::GetNamedSecurityInfoW(
        native_path.c_str(),
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        &owner_sid,
        &group_sid,
        nullptr,
        nullptr,
        &security_descriptor);

    if (result == ERROR_SUCCESS) {
        fi.owner = sid_to_account_name(owner_sid);
        fi.group = sid_to_account_name(group_sid);
        std::string owner_sid_string = sid_to_string(owner_sid);
        if (!owner_sid_string.empty()) {
            fi.owner_numeric = std::move(owner_sid_string);
            fi.has_owner_numeric = true;
        }
        std::string group_sid_string = sid_to_string(group_sid);
        if (!group_sid_string.empty()) {
            fi.group_numeric = std::move(group_sid_string);
            fi.has_group_numeric = true;
        }
        if (auto owner_rid = sid_to_rid(owner_sid)) {
            fi.owner_id = *owner_rid;
            fi.has_owner_id = true;
        }
        if (auto group_rid = sid_to_rid(group_sid)) {
            fi.group_id = *group_rid;
            fi.has_group_id = true;
        }
    }

    if (security_descriptor) {
        ::LocalFree(security_descriptor);
    }
#endif
}

} // namespace nls
