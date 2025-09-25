#include "command_line_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include <CLI/CLI.hpp>

#include "colors.h"
#include "string_utils.h"
#include "color_formatter.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nls {

bool CommandLineParser::MultiplyWithOverflow(uintmax_t a, uintmax_t b, uintmax_t& result) const {
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

bool CommandLineParser::PowWithOverflow(uintmax_t base, unsigned exponent, uintmax_t& result) const {
    result = 1;
    for (unsigned i = 0; i < exponent; ++i) {
        if (!MultiplyWithOverflow(result, base, result)) {
            return false;
        }
    }
    return true;
}

std::optional<CommandLineParser::SizeSpec> CommandLineParser::ParseSizeSpec(const std::string& text) const {
    if (text.empty()) {
        return std::nullopt;
    }

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
            if (idx != number_part.size()) {
                return std::nullopt;
            }
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
        if (!PowWithOverflow(base_value, exponent, multiplier)) {
            return std::nullopt;
        }
    }

    uintmax_t scaled = 0;
    if (!MultiplyWithOverflow(number, multiplier, scaled)) {
        return std::nullopt;
    }

    SizeSpec spec;
    spec.value = scaled;
    spec.show_suffix = suffix_part.size() > 0 && number_part.empty();
    spec.suffix = suffix_part;
    return spec;
}

const std::map<std::string, Options::QuotingStyle>& CommandLineParser::QuotingStyleMap() const {
    static const std::map<std::string, Options::QuotingStyle> map{
        {"literal", Options::QuotingStyle::Literal},
        {"locale", Options::QuotingStyle::Locale},
        {"shell", Options::QuotingStyle::Shell},
        {"shell-always", Options::QuotingStyle::ShellAlways},
        {"shell-escape", Options::QuotingStyle::ShellEscape},
        {"shell-escape-always", Options::QuotingStyle::ShellEscapeAlways},
        {"c", Options::QuotingStyle::C},
        {"escape", Options::QuotingStyle::Escape},
    };
    return map;
}

std::optional<Options::QuotingStyle> CommandLineParser::ParseQuotingStyleWord(std::string word) const {
    word = StringUtils::ToLower(std::move(word));
    const auto& mapping = QuotingStyleMap();
    auto it = mapping.find(word);
    if (it != mapping.end()) {
        return it->second;
    }
    return std::nullopt;
}

Options CommandLineParser::Parse(int argc, char** argv) {

    if (const char* env = std::getenv("QUOTING_STYLE")) {
        if (auto style = ParseQuotingStyleWord(env)) {
            options.quoting_style = *style;
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

    program.add_option("paths", options.paths, "paths to list")->type_name("PATH");

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
        {"m", Options::Format::CommaSeparated},
    };

    const std::map<std::string, Options::Sort> sort_map{
        {"none", Options::Sort::None},
        {"name", Options::Sort::Name},
        {"time", Options::Sort::Time},
        {"mtime", Options::Sort::Time},
        {"size", Options::Sort::Size},
        {"extension", Options::Sort::Extension},
        {"ext", Options::Sort::Extension},
    };

    const std::map<std::string, Options::IndicatorStyle> indicator_map{
        {"slash", Options::IndicatorStyle::Slash},
        {"slashes", Options::IndicatorStyle::Slash},
        {"none", Options::IndicatorStyle::None},
        {"off", Options::IndicatorStyle::None},
    };

    const auto& quoting_map = QuotingStyleMap();

    const std::map<std::string, Options::Report> report_map{
        {"long", Options::Report::Long},
        {"short", Options::Report::Short},
    };

    enum class ColorMode { Auto, Always, Never };
    const std::map<std::string, ColorMode> color_map{
        {"auto", ColorMode::Auto},
        {"always", ColorMode::Always},
        {"never", ColorMode::Never},
    };

    auto layout = program.add_option_group("Layout options");
    layout->add_flag_callback("-l,--long", [&]() { options.format = Options::Format::Long; },
        "use a long listing format");
    layout->add_flag_callback("-1,--one-per-line", [&]() { options.format = Options::Format::SingleColumn; },
        "list one file per line");
    layout->add_flag_callback("-x", [&]() { options.format = Options::Format::ColumnsHorizontal; },
        "list entries by lines instead of by columns");
    layout->add_flag_callback("-C", [&]() { options.format = Options::Format::ColumnsVertical; },
        "list entries by columns instead of by lines");

    auto format_option = layout->add_option("--format", options.format,
        R"(use format: across (-x), horizontal (-x),
long (-l), single-column (-1), vertical (-C)
or comma (-m) (default: vertical))");
    format_option->type_name("WORD");
    format_option->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case).description(""));
    format_option->default_str("vertical");

    layout->add_flag_callback("--header", [&]() { options.header = true; },
        "print directory header and column names in long listing");
    layout->add_flag_callback("-m", [&]() { options.format = Options::Format::CommaSeparated; },
        "fill width with a comma separated list of entries");

    auto tab_option = layout->add_option_function<int>("-T,--tabsize",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--tabsize", "COLS must be non-negative");
            }
            options.tab_size = cols;
        },
        "assume tab stops at each COLS instead of 8");
    tab_option->type_name("COLS");
    tab_option->expected(1);

    auto width_option = layout->add_option_function<int>("-w,--width",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--width", "COLS must be non-negative");
            }
            options.output_width = cols;
        },
        "set output width to COLS.  0 means no limit");
    width_option->type_name("COLS");
    width_option->expected(0, 256);

    auto tree_option = layout->add_option_function<std::size_t>("--tree",
        [&](const std::size_t& depth) {
            if (depth == 0) {
                throw CLI::ValidationError("--tree", "DEPTH must be greater than zero");
            }
            options.tree = true;
            options.tree_depth = depth;
        },
        "show tree view of directories, optionally limited to DEPTH");
    tree_option->type_name("DEPTH");
    tree_option->expected(1);

    auto report_option = layout->add_option_function<Options::Report>("--report",
        [&](const Options::Report& report) {
            options.report = report;
        },
        R"(show summary report: short, long (default: long)
 )");
    report_option->type_name("WORD");
    report_option->expected(1);
    report_option->transform(CLI::CheckedTransformer(report_map, CLI::ignore_case).description(""));
    report_option->default_str("long");

    layout->add_flag_callback("--zero", [&]() { options.zero_terminate = true; },
        "end each output line with NUL, not newline");

    auto filtering = program.add_option_group("Filtering options");
    filtering->add_flag_callback("-a,--all", [&]() { options.all = true; },
        "do not ignore entries starting with .");
    filtering->add_flag_callback("-A,--almost-all", [&]() { options.almost_all = true; },
        "do not list . and ..");
    filtering->add_flag_callback("-d,--dirs", [&]() {
        options.dirs_only = true;
        options.files_only = false;
    }, "show only directories");
    filtering->add_flag_callback("-f,--files", [&]() {
        options.files_only = true;
        options.dirs_only = false;
    }, "show only files");
    filtering->add_flag_callback("-B,--ignore-backups", [&]() { options.ignore_backups = true; },
        "do not list implied entries ending with ~");

    auto hide_option = filtering->add_option("--hide", options.hide_patterns,
        R"(do not list implied entries matching shell
PATTERN (overridden by -a or -A))");
    hide_option->type_name("PATTERN");

    auto ignore_option = filtering->add_option("-I,--ignore", options.ignore_patterns,
        "do not list implied entries matching shell PATTERN");
    ignore_option->type_name("PATTERN");

    auto sorting = program.add_option_group("Sorting options");
    sorting->add_flag_callback("-t", [&]() { options.sort = Options::Sort::Time; },
        "sort by modification time, newest first");
    sorting->add_flag_callback("-S", [&]() { options.sort = Options::Sort::Size; },
        "sort by file size, largest first");
    sorting->add_flag_callback("-X", [&]() { options.sort = Options::Sort::Extension; },
        "sort by file extension");
    sorting->add_flag_callback("-U", [&]() { options.sort = Options::Sort::None; },
        "do not sort; list entries in directory order");
    sorting->add_flag_callback("-r,--reverse", [&]() { options.reverse = true; },
        "reverse order while sorting");

    auto sort_option = sorting->add_option("--sort", options.sort,
        R"(sort by WORD instead of name: none, size,
time, extension (default: name))");
    sort_option->type_name("WORD");
    sort_option->transform(CLI::CheckedTransformer(sort_map, CLI::ignore_case).description(""));
    sort_option->default_str("name");

    sorting->add_flag_callback("--group-directories-first,--sd,--sort-dirs", [&]() {
        options.group_dirs_first = true;
        options.sort_files_first = false;
    }, "sort directories before files");
    sorting->add_flag_callback("--sf,--sort-files", [&]() {
        options.sort_files_first = true;
        options.group_dirs_first = false;
    }, "sort files first");
    sorting->add_flag_callback("--df,--dots-first", [&]() { options.dots_first = true; },
        "sort dot-files and dot-folders first");

    auto appearance = program.add_option_group("Appearance options");
    appearance->add_flag_callback("-b,--escape", [&]() { options.quoting_style = Options::QuotingStyle::Escape; },
        "print C-style escapes for nongraphic characters");
    appearance->add_flag_callback("-N,--literal", [&]() { options.quoting_style = Options::QuotingStyle::Literal; },
        "print entry names without quoting");
    appearance->add_flag_callback("-Q,--quote-name", [&]() { options.quoting_style = Options::QuotingStyle::C; },
        "enclose entry names in double quotes");

    auto quoting_option = appearance->add_option("--quoting-style", options.quoting_style,
        R"(use quoting style WORD for entry names:
literal, locale, shell, shell-always, shell-escape,
shell-escape-always, c, escape (default: literal))");
    quoting_option->type_name("WORD");
    quoting_option->transform(CLI::CheckedTransformer(quoting_map, CLI::ignore_case).description(""));
    quoting_option->default_str("literal");

    appearance->add_flag_callback("-p", [&]() { options.indicator = Options::IndicatorStyle::Slash; },
        "append / indicator to directories");

    auto indicator_option = appearance->add_option("--indicator-style", options.indicator,
        R"(append indicator with style STYLE to entry names:
none, slash (-p) (default: slash))");
    indicator_option->type_name("STYLE");
    indicator_option->transform(CLI::CheckedTransformer(indicator_map, CLI::ignore_case).description(""));
    indicator_option->default_str("slash");

    appearance->add_flag_callback("--no-icons,--without-icons", [&]() { options.no_icons = true; },
        "disable icons in output");
    appearance->add_flag_callback("--no-color", [&]() { options.no_color = true; },
        "disable ANSI colors");

    auto color_option = appearance->add_option_function<ColorMode>("--color",
        [&](const ColorMode& color) {
            options.no_color = (color == ColorMode::Never);
        },
        R"(colorize the output: auto, always,
never (default: auto))");
    color_option->type_name("WHEN");
    color_option->transform(CLI::CheckedTransformer(color_map, CLI::ignore_case).description(""));
    color_option->default_str("auto");

    appearance->add_flag_callback("--light", [&]() { options.color_theme = Options::ColorTheme::Light; },
        "use light color scheme");
    appearance->add_flag_callback("--dark", [&]() { options.color_theme = Options::ColorTheme::Dark; },
        "use dark color scheme");
    appearance->add_flag_callback("-q,--hide-control-chars", [&]() { options.hide_control_chars = true; },
        "print ? instead of nongraphic characters");
    appearance->add_flag_callback("--show-control-chars", [&]() { options.hide_control_chars = false; },
        "show nongraphic characters as-is");
    appearance->add_option("--time-style", options.time_style,
        R"(use time display format: default, locale,
long-iso, full-iso, iso, iso8601,
+FORMAT (default: locale))")->type_name("FORMAT");
    appearance->add_flag_callback("--full-time", [&]() {
        options.format = Options::Format::Long;
        options.time_style = "full-iso";
    }, "like -l --time-style=full-iso");
    appearance->add_flag_callback("--hyperlink", [&]() { options.hyperlink = true; },
        "emit hyperlinks for entries");

    auto information = program.add_option_group("Information options");
    information->add_flag_callback("-i,--inode", [&]() { options.show_inode = true; },
        "show inode number");
    information->add_flag_callback("-o", [&]() {
        options.format = Options::Format::Long;
        options.show_group = false;
    }, "use a long listing format without group information");
    information->add_flag_callback("-g", [&]() {
        options.format = Options::Format::Long;
        options.show_owner = false;
    }, "use a long listing format without owner information");
    information->add_flag_callback("-G,--no-group", [&]() { options.show_group = false; },
        "show no group information in a long listing");
    information->add_flag_callback("-n,--numeric-uid-gid", [&]() {
        options.format = Options::Format::Long;
        options.numeric_uid_gid = true;
    }, "like -l, but list numeric user and group IDs");
    information->add_flag_callback("--bytes,--non-human-readable", [&]() { options.bytes = true; },
        "show file sizes in bytes");
    information->add_flag_callback("-s,--size", [&]() { options.show_block_size = true; },
        "print the allocated size of each file, in blocks");

    auto block_option = information->add_option_function<std::string>("--block-size",
        [&](const std::string& text) {
            auto spec = ParseSizeSpec(text);
            if (!spec) {
                throw CLI::ValidationError("--block-size", "invalid value '" + text + "'");
            }
            options.block_size = spec->value;
            options.block_size_specified = true;
            options.block_size_show_suffix = spec->show_suffix;
            options.block_size_suffix = spec->suffix;
        },
        "with -l, scale sizes by SIZE when printing them");
    block_option->type_name("SIZE");

    information->add_flag_callback("-L,--dereference", [&]() { options.dereference = true; },
        R"(when showing file information for a symbolic link,
show information for the file the link references)");
    information->add_flag_callback("--gs,--git-status", [&]() { options.git_status = true; },
        "show git status for each file");

    try {
        program.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(program.exit(e));
    }

    if (options.paths.empty()) {
        options.paths.push_back(".");
    }
    if (options.all) {
        options.almost_all = false;
    }
    return options;
}

} // namespace nls
