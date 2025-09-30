#include "command_line_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>

#include "colors.h"
#include "string_utils.h"
#include "color_formatter.h"
#include "platform.h"

namespace nls {

namespace {

enum class ColorMode { Auto, Always, Never };

class ConfigBuilder {
public:
    std::vector<std::string>& paths() { return paths_; }
    std::vector<std::string>& hide_patterns() { return hide_patterns_; }
    std::vector<std::string>& ignore_patterns() { return ignore_patterns_; }

    void SetFormat(Config::Format format)
    {
        actions_.emplace_back([format](Config& cfg) { cfg.set_format(format); });
    }

    void SetHeader(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_header(value); });
    }

    void SetTabSize(int value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_tab_size(value); });
    }

    void SetOutputWidth(int value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_output_width(value); });
    }

    void EnableTree(std::optional<std::size_t> depth)
    {
        actions_.emplace_back([depth](Config& cfg) {
            cfg.set_tree(true);
            cfg.set_tree_depth(depth);
        });
    }

    void SetReport(Config::Report report)
    {
        actions_.emplace_back([report](Config& cfg) { cfg.set_report(report); });
    }

    void SetZeroTerminate(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_zero_terminate(value); });
    }

    void SetAll(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_all(value); });
    }

    void SetAlmostAll(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_almost_all(value); });
    }

    void SetDirsOnly()
    {
        actions_.emplace_back([](Config& cfg) {
            cfg.set_dirs_only(true);
            cfg.set_files_only(false);
        });
    }

    void SetFilesOnly()
    {
        actions_.emplace_back([](Config& cfg) {
            cfg.set_files_only(true);
            cfg.set_dirs_only(false);
        });
    }

    void SetIgnoreBackups(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_ignore_backups(value); });
    }

    void SetSort(Config::Sort sort)
    {
        actions_.emplace_back([sort](Config& cfg) { cfg.set_sort(sort); });
    }

    void SetReverse(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_reverse(value); });
    }

    void SetGroupDirsFirst()
    {
        actions_.emplace_back([](Config& cfg) {
            cfg.set_group_dirs_first(true);
            cfg.set_sort_files_first(false);
        });
    }

    void SetSortFilesFirst()
    {
        actions_.emplace_back([](Config& cfg) {
            cfg.set_sort_files_first(true);
            cfg.set_group_dirs_first(false);
        });
    }

    void SetDotsFirst(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_dots_first(value); });
    }

    void SetQuotingStyle(Config::QuotingStyle style)
    {
        actions_.emplace_back([style](Config& cfg) { cfg.set_quoting_style(style); });
    }

    void SetIndicator(Config::IndicatorStyle style)
    {
        actions_.emplace_back([style](Config& cfg) { cfg.set_indicator(style); });
    }

    void SetNoIcons(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_no_icons(value); });
    }

    void SetColorMode(ColorMode mode)
    {
        color_mode_ = mode;
    }

    void SetColorTheme(Config::ColorTheme theme)
    {
        actions_.emplace_back([theme](Config& cfg) { cfg.set_color_theme(theme); });
    }

    void SetHideControlChars(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_hide_control_chars(value); });
    }

    void SetTimeStyle(std::string style)
    {
        actions_.emplace_back([style = std::move(style)](Config& cfg) { cfg.set_time_style(style); });
    }

    void SetHyperlink(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_hyperlink(value); });
    }

    void SetShowInode(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_show_inode(value); });
    }

    void SetShowGroup(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_show_group(value); });
    }

    void SetShowOwner(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_show_owner(value); });
    }

    void SetNumericUidGid(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_numeric_uid_gid(value); });
    }

    void SetBytes(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_bytes(value); });
    }

    void SetShowBlockSize(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_show_block_size(value); });
    }

    void SetBlockSize(uintmax_t value, bool specified, bool show_suffix, std::string suffix)
    {
        actions_.emplace_back([value, specified, show_suffix, suffix = std::move(suffix)](Config& cfg) {
            cfg.set_block_size(value);
            cfg.set_block_size_specified(specified);
            cfg.set_block_size_show_suffix(show_suffix);
            cfg.set_block_size_suffix(suffix);
        });
    }

    void SetDereference(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_dereference(value); });
    }

    void SetGitStatus(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_git_status(value); });
    }

    Config& Build()
    {
        Config& cfg = Config::Instance();
        cfg.Reset();
        cfg.set_paths(paths_);
        cfg.set_hide_patterns(hide_patterns_);
        cfg.set_ignore_patterns(ignore_patterns_);
        for (const auto& action : actions_) {
            action(cfg);
        }
        if (cfg.paths().empty()) {
            cfg.mutable_paths().push_back(".");
        }
        if (cfg.all()) {
            cfg.set_almost_all(false);
        }
        const ColorMode mode = color_mode_.value_or(ColorMode::Auto);
        if (mode == ColorMode::Auto) {
            const bool disable_from_env = std::getenv("NO_COLOR") != nullptr;
            cfg.set_no_color(disable_from_env || !Platform::isOutputTerminal());
        } else if (mode == ColorMode::Never) {
            cfg.set_no_color(true);
        } else {
            cfg.set_no_color(false);
        }
        return cfg;
    }

private:
    std::vector<std::function<void(Config&)>> actions_{};
    std::vector<std::string> paths_{};
    std::vector<std::string> hide_patterns_{};
    std::vector<std::string> ignore_patterns_{};
    std::optional<ColorMode> color_mode_{};
};

} // namespace

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

const std::map<std::string, Config::QuotingStyle>& CommandLineParser::QuotingStyleMap() const {
    static const std::map<std::string, Config::QuotingStyle> map{
        {"literal", Config::QuotingStyle::Literal},
        {"locale", Config::QuotingStyle::Locale},
        {"shell", Config::QuotingStyle::Shell},
        {"shell-always", Config::QuotingStyle::ShellAlways},
        {"shell-escape", Config::QuotingStyle::ShellEscape},
        {"shell-escape-always", Config::QuotingStyle::ShellEscapeAlways},
        {"c", Config::QuotingStyle::C},
        {"escape", Config::QuotingStyle::Escape},
    };
    return map;
}

std::optional<Config::QuotingStyle> CommandLineParser::ParseQuotingStyleWord(std::string word) const {
    word = StringUtils::ToLower(std::move(word));
    const auto& mapping = QuotingStyleMap();
    auto it = mapping.find(word);
    if (it != mapping.end()) {
        return it->second;
    }
    return std::nullopt;
}

Config& CommandLineParser::Parse(int argc, char** argv) {

    ConfigBuilder builder;

    if (const char* env = std::getenv("QUOTING_STYLE")) {
        if (auto style = ParseQuotingStyleWord(env)) {
            builder.SetQuotingStyle(*style);
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

    program.add_option("paths", builder.paths(), "paths to list")->type_name("PATH");

    const std::map<std::string, Config::Format> format_map{
        {"long", Config::Format::Long},
        {"l", Config::Format::Long},
        {"single-column", Config::Format::SingleColumn},
        {"single", Config::Format::SingleColumn},
        {"1", Config::Format::SingleColumn},
        {"across", Config::Format::ColumnsHorizontal},
        {"horizontal", Config::Format::ColumnsHorizontal},
        {"x", Config::Format::ColumnsHorizontal},
        {"vertical", Config::Format::ColumnsVertical},
        {"columns", Config::Format::ColumnsVertical},
        {"column", Config::Format::ColumnsVertical},
        {"c", Config::Format::ColumnsVertical},
        {"comma", Config::Format::CommaSeparated},
        {"commas", Config::Format::CommaSeparated},
        {"m", Config::Format::CommaSeparated},
    };

    const std::map<std::string, Config::Sort> sort_map{
        {"none", Config::Sort::None},
        {"name", Config::Sort::Name},
        {"time", Config::Sort::Time},
        {"mtime", Config::Sort::Time},
        {"size", Config::Sort::Size},
        {"extension", Config::Sort::Extension},
        {"ext", Config::Sort::Extension},
    };

    const std::map<std::string, Config::IndicatorStyle> indicator_map{
        {"slash", Config::IndicatorStyle::Slash},
        {"slashes", Config::IndicatorStyle::Slash},
        {"none", Config::IndicatorStyle::None},
        {"off", Config::IndicatorStyle::None},
    };

    const auto& quoting_map = QuotingStyleMap();

    const std::map<std::string, Config::Report> report_map{
        {"long", Config::Report::Long},
        {"short", Config::Report::Short},
    };

    const std::map<std::string, ColorMode> color_map{
        {"auto", ColorMode::Auto},
        {"always", ColorMode::Always},
        {"never", ColorMode::Never},
    };

    auto layout = program.add_option_group("Layout options");
    layout->add_flag_callback("-l,--long", [&]() { builder.SetFormat(Config::Format::Long); },
        "use a long listing format");
    layout->add_flag_callback("-1,--one-per-line", [&]() { builder.SetFormat(Config::Format::SingleColumn); },
        "list one file per line");
    layout->add_flag_callback("-x", [&]() { builder.SetFormat(Config::Format::ColumnsHorizontal); },
        "list entries by lines instead of by columns");
    layout->add_flag_callback("-C", [&]() { builder.SetFormat(Config::Format::ColumnsVertical); },
        "list entries by columns instead of by lines");

    auto format_option = layout->add_option_function<Config::Format>("--format",
        [&](const Config::Format& format) { builder.SetFormat(format); },
        R"(use format: across (-x), horizontal (-x),
long (-l), single-column (-1), vertical (-C)
or comma (-m) (default: vertical))");
    format_option->type_name("WORD");
    format_option->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case).description(""));
    format_option->default_str("vertical");

    layout->add_flag_callback("--header", [&]() { builder.SetHeader(true); },
        "print directory header and column names in long listing");
    layout->add_flag_callback("-m", [&]() { builder.SetFormat(Config::Format::CommaSeparated); },
        "fill width with a comma separated list of entries");

    auto tab_option = layout->add_option_function<int>("-T,--tabsize",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--tabsize", "COLS must be non-negative");
            }
            builder.SetTabSize(cols);
        },
        "assume tab stops at each COLS instead of 8");
    tab_option->type_name("COLS");
    tab_option->expected(1);

    auto width_option = layout->add_option_function<int>("-w,--width",
        [&](const int& cols) {
            if (cols < 0) {
                throw CLI::ValidationError("--width", "COLS must be non-negative");
            }
            builder.SetOutputWidth(cols);
        },
        "set output width to COLS.  0 means no limit");
    width_option->type_name("COLS");
    width_option->expected(0, 256);

    auto tree_option = layout->add_option_function<std::size_t>("--tree",
        [&](const std::size_t& depth) {
            if (depth == 0) {
                throw CLI::ValidationError("--tree", "DEPTH must be greater than zero");
            }
            builder.EnableTree(depth);
        },
        "show tree view of directories, optionally limited to DEPTH");
    tree_option->type_name("DEPTH");
    tree_option->expected(1);

    auto report_option = layout->add_option_function<Config::Report>("--report",
        [&](const Config::Report& report) {
            builder.SetReport(report);
        },
        R"(show summary report: short, long (default: long)
)");
    report_option->type_name("WORD");
    report_option->expected(1);
    report_option->transform(CLI::CheckedTransformer(report_map, CLI::ignore_case).description(""));
    report_option->default_str("long");

    layout->add_flag_callback("--zero", [&]() { builder.SetZeroTerminate(true); },
        "end each output line with NUL, not newline");

    auto filtering = program.add_option_group("Filtering options");
    filtering->add_flag_callback("-a,--all", [&]() { builder.SetAll(true); },
        "do not ignore entries starting with .");
    filtering->add_flag_callback("-A,--almost-all", [&]() { builder.SetAlmostAll(true); },
        "do not list . and ..");
    filtering->add_flag_callback("-d,--dirs", [&]() {
        builder.SetDirsOnly();
    }, "show only directories");
    filtering->add_flag_callback("-f,--files", [&]() {
        builder.SetFilesOnly();
    }, "show only files");
    filtering->add_flag_callback("-B,--ignore-backups", [&]() { builder.SetIgnoreBackups(true); },
        "do not list implied entries ending with ~");

    auto hide_option = filtering->add_option("--hide", builder.hide_patterns(),
        R"(do not list implied entries matching shell
PATTERN (overridden by -a or -A))");
    hide_option->type_name("PATTERN");

    auto ignore_option = filtering->add_option("-I,--ignore", builder.ignore_patterns(),
        "do not list implied entries matching shell PATTERN");
    ignore_option->type_name("PATTERN");

    auto sorting = program.add_option_group("Sorting options");
    sorting->add_flag_callback("-t", [&]() { builder.SetSort(Config::Sort::Time); },
        "sort by modification time, newest first");
    sorting->add_flag_callback("-S", [&]() { builder.SetSort(Config::Sort::Size); },
        "sort by file size, largest first");
    sorting->add_flag_callback("-X", [&]() { builder.SetSort(Config::Sort::Extension); },
        "sort by file extension");
    sorting->add_flag_callback("-U", [&]() { builder.SetSort(Config::Sort::None); },
        "do not sort; list entries in directory order");
    sorting->add_flag_callback("-r,--reverse", [&]() { builder.SetReverse(true); },
        "reverse order while sorting");

    auto sort_option = sorting->add_option_function<Config::Sort>("--sort",
        [&](const Config::Sort& sort) { builder.SetSort(sort); },
        R"(sort by WORD instead of name: none, size,
time, extension (default: name))");
    sort_option->type_name("WORD");
    sort_option->transform(CLI::CheckedTransformer(sort_map, CLI::ignore_case).description(""));
    sort_option->default_str("name");

    sorting->add_flag_callback("--group-directories-first,--sd,--sort-dirs", [&]() {
        builder.SetGroupDirsFirst();
    }, "sort directories before files");
    sorting->add_flag_callback("--sf,--sort-files", [&]() {
        builder.SetSortFilesFirst();
    }, "sort files first");
    sorting->add_flag_callback("--df,--dots-first", [&]() { builder.SetDotsFirst(true); },
        "sort dot-files and dot-folders first");

    auto appearance = program.add_option_group("Appearance options");
    appearance->add_flag_callback("-b,--escape", [&]() { builder.SetQuotingStyle(Config::QuotingStyle::Escape); },
        "print C-style escapes for nongraphic characters");
    appearance->add_flag_callback("-N,--literal", [&]() { builder.SetQuotingStyle(Config::QuotingStyle::Literal); },
        "print entry names without quoting");
    appearance->add_flag_callback("-Q,--quote-name", [&]() { builder.SetQuotingStyle(Config::QuotingStyle::C); },
        "enclose entry names in double quotes");

    auto quoting_option = appearance->add_option_function<Config::QuotingStyle>("--quoting-style",
        [&](const Config::QuotingStyle& quoting) { builder.SetQuotingStyle(quoting); },
        R"(use quoting style WORD for entry names:
literal, locale, shell, shell-always, shell-escape,
shell-escape-always, c, escape (default: literal))");
    quoting_option->type_name("WORD");
    quoting_option->transform(CLI::CheckedTransformer(quoting_map, CLI::ignore_case).description(""));
    quoting_option->default_str("literal");

    appearance->add_flag_callback("-p", [&]() { builder.SetIndicator(Config::IndicatorStyle::Slash); },
        "append / indicator to directories");

    auto indicator_option = appearance->add_option_function<Config::IndicatorStyle>("--indicator-style",
        [&](const Config::IndicatorStyle& indicator) { builder.SetIndicator(indicator); },
        R"(append indicator with style STYLE to entry names:
none, slash (-p) (default: slash))");
    indicator_option->type_name("STYLE");
    indicator_option->transform(CLI::CheckedTransformer(indicator_map, CLI::ignore_case).description(""));
    indicator_option->default_str("slash");

    appearance->add_flag_callback("--no-icons,--without-icons", [&]() { builder.SetNoIcons(true); },
        "disable icons in output");
    appearance->add_flag_callback("--no-color", [&]() { builder.SetColorMode(ColorMode::Never); },
        "disable ANSI colors");

    auto color_option = appearance->add_option_function<ColorMode>("--color",
        [&](const ColorMode& color) {
            builder.SetColorMode(color);
        },
        R"(colorize the output: auto, always,
never (default: auto))");
    color_option->type_name("WHEN");
    color_option->transform(CLI::CheckedTransformer(color_map, CLI::ignore_case).description(""));
    color_option->default_str("auto");

    appearance->add_flag_callback("--light", [&]() { builder.SetColorTheme(Config::ColorTheme::Light); },
        "use light color scheme");
    appearance->add_flag_callback("--dark", [&]() { builder.SetColorTheme(Config::ColorTheme::Dark); },
        "use dark color scheme");
    appearance->add_flag_callback("-q,--hide-control-chars", [&]() { builder.SetHideControlChars(true); },
        "print ? instead of nongraphic characters");
    appearance->add_flag_callback("--show-control-chars", [&]() { builder.SetHideControlChars(false); },
        "show nongraphic characters as-is");
    auto time_style_option = appearance->add_option_function<std::string>("--time-style",
        [&](const std::string& style) { builder.SetTimeStyle(style); },
        R"(use time display format: default, locale,
long-iso, full-iso, iso, iso8601,
FORMAT (default: locale))");
    time_style_option->type_name("FORMAT");
    appearance->add_flag_callback("--full-time", [&]() {
        builder.SetFormat(Config::Format::Long);
        builder.SetTimeStyle("full-iso");
    }, "like -l --time-style=full-iso");
    appearance->add_flag_callback("--hyperlink", [&]() { builder.SetHyperlink(true); },
        "emit hyperlinks for entries");

    auto information = program.add_option_group("Information options");
    information->add_flag_callback("-i,--inode", [&]() { builder.SetShowInode(true); },
        "show inode number");
    information->add_flag_callback("-o", [&]() {
        builder.SetFormat(Config::Format::Long);
        builder.SetShowGroup(false);
    }, "use a long listing format without group information");
    information->add_flag_callback("-g", [&]() {
        builder.SetFormat(Config::Format::Long);
        builder.SetShowOwner(false);
    }, "use a long listing format without owner information");
    information->add_flag_callback("-G,--no-group", [&]() { builder.SetShowGroup(false); },
        "show no group information in a long listing");
    information->add_flag_callback("-n,--numeric-uid-gid", [&]() {
        builder.SetFormat(Config::Format::Long);
        builder.SetNumericUidGid(true);
    }, "like -l, but list numeric user and group IDs");
    information->add_flag_callback("--bytes,--non-human-readable", [&]() { builder.SetBytes(true); },
        "show file sizes in bytes");
    information->add_flag_callback("-s,--size", [&]() { builder.SetShowBlockSize(true); },
        "print the allocated size of each file, in blocks");

    auto block_option = information->add_option_function<std::string>("--block-size",
        [&](const std::string& text) {
            auto spec = ParseSizeSpec(text);
            if (!spec) {
                throw CLI::ValidationError("--block-size", "invalid value '" + text + "'");
            }
            builder.SetBlockSize(spec->value, true, spec->show_suffix, spec->suffix);
        },
        "with -l, scale sizes by SIZE when printing them");
    block_option->type_name("SIZE");

    information->add_flag_callback("-L,--dereference", [&]() { builder.SetDereference(true); },
        R"(when showing file information for a symbolic link,
show information for the file the link references)");
    information->add_flag_callback("--gs,--git-status", [&]() { builder.SetGitStatus(true); },
        "show git status for each file");

    try {
        program.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(program.exit(e));
    }

    Config& options = builder.Build();
    return options;
}

} // namespace nls
