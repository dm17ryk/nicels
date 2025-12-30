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

#include "color_formatter.h"
#include "platform.h"
#include "string_utils.h"
#include "version.h"

namespace nls {

namespace {

using ColorMode = Config::ColorMode;

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

    void SetRecursiveFlat(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_recursive_flat(value); });
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

    void SetThemeName(std::string theme)
    {
        actions_.emplace_back([theme = std::move(theme)](Config& cfg) mutable {
            cfg.set_theme_name(std::optional<std::string>(std::move(theme)));
        });
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

    void SetPerfLogging(bool value)
    {
        actions_.emplace_back([value](Config& cfg) { cfg.set_perf_logging(value); });
    }

    void SetDbAction(Config::DbAction action)
    {
        db_action_ = action;
        actions_.emplace_back([action](Config& cfg) { cfg.set_db_action(action); });
    }

    void EnableDbMode()
    {
        db_mode_ = true;
    }

    bool db_mode() const
    {
        return db_mode_;
    }

    void SetDbName(std::string value)
    {
        db_icon_entry_.name = value;
        db_alias_entry_.name = db_icon_entry_.name;
    }

    void SetDbIcon(std::string value)
    {
        db_icon_entry_.icon = std::move(value);
    }

    void SetDbIconClass(std::string value)
    {
        db_icon_entry_.icon_class = std::move(value);
    }

    void SetDbIconUtf16(std::string value)
    {
        db_icon_entry_.icon_utf16 = std::move(value);
    }

    void SetDbIconHex(std::string value)
    {
        db_icon_entry_.icon_hex = std::move(value);
    }

    void SetDbDescription(std::string value)
    {
        db_icon_entry_.description = std::move(value);
    }

    void SetDbUsedBy(std::string value)
    {
        db_icon_entry_.used_by = std::move(value);
    }

    void SetDbAlias(std::string value)
    {
        db_alias_entry_.alias = std::move(value);
    }

    Config::DbAction db_action() const
    {
        return db_action_;
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
        cfg.set_db_icon_entry(db_icon_entry_);
        cfg.set_db_alias_entry(db_alias_entry_);
        if (cfg.paths().empty()) {
            if (cfg.db_action() == Config::DbAction::None) {
                cfg.mutable_paths().push_back(".");
            }
        }
        if (cfg.all()) {
            cfg.set_almost_all(false);
        }
        const ColorMode mode = color_mode_.value_or(ColorMode::Auto);
        cfg.set_color_mode(mode);
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
    Config::DbIconEntry db_icon_entry_{};
    Config::DbAliasEntry db_alias_entry_{};
    Config::DbAction db_action_ = Config::DbAction::None;
    bool db_mode_ = false;
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
    word = StringUtils::ToLower(word);
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
    program.set_version_flag("--version", Version::FullString());
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

    auto* db_command = program.add_subcommand("db", "Inspect configuration database tables");
    db_command->fallthrough(false);
    db_command->configurable(false);
    db_command->allow_extras(false);
    db_command->callback([&]() {
        builder.EnableDbMode();
        if (builder.paths().empty()) {
            return;
        }
        builder.paths().clear();
    });

    db_command->add_flag_callback("--show-files",
        [&]() { builder.SetDbAction(Config::DbAction::ShowFiles); },
        "list file icon metadata from the merged configuration database");
    db_command->add_flag_callback("--show-folders",
        [&]() { builder.SetDbAction(Config::DbAction::ShowFolders); },
        "list folder icon metadata from the merged configuration database");
    db_command->add_flag_callback("--show-file-aliases",
        [&]() { builder.SetDbAction(Config::DbAction::ShowFileAliases); },
        "list file alias metadata along with resolved icons");
    db_command->add_flag_callback("--show-folder-aliases",
        [&]() { builder.SetDbAction(Config::DbAction::ShowFolderAliases); },
        "list folder alias metadata along with resolved icons");

    db_command->add_flag_callback("--set-file",
        [&]() { builder.SetDbAction(Config::DbAction::SetFile); },
        "add or update a file icon entry; supply all metadata fields");
    db_command->add_flag_callback("--set-folder",
        [&]() { builder.SetDbAction(Config::DbAction::SetFolder); },
        "add or update a folder icon entry; supply all metadata fields");
    db_command->add_flag_callback("--set-file-aliases",
        [&]() { builder.SetDbAction(Config::DbAction::SetFileAlias); },
        "add, update, or remove a file alias entry");
    db_command->add_flag_callback("--set-folder-aliases",
        [&]() { builder.SetDbAction(Config::DbAction::SetFolderAlias); },
        "add, update, or remove a folder alias entry");

    auto name_option = db_command->add_option_function<std::string>("--name",
        [&](const std::string& value) { builder.SetDbName(value); },
        "entry name (extension or folder label)");
    name_option->type_name("TEXT");

    auto icon_option = db_command->add_option_function<std::string>("--icon",
        [&](const std::string& value) { builder.SetDbIcon(value); },
        "icon glyph to associate with the entry");
    icon_option->type_name("TEXT");

    auto icon_class_option = db_command->add_option_function<std::string>("--icon_class",
        [&](const std::string& value) { builder.SetDbIconClass(value); },
        "icon class identifier");
    icon_class_option->type_name("TEXT");

    auto icon_utf_option = db_command->add_option_function<std::string>("--icon_utf_16_codes",
        [&](const std::string& value) { builder.SetDbIconUtf16(value); },
        "icon UTF-16 codepoint (format \\uXXXX)");
    icon_utf_option->type_name("CODE");

    auto icon_hex_option = db_command->add_option_function<std::string>("--icon_hex_code",
        [&](const std::string& value) { builder.SetDbIconHex(value); },
        "icon hexadecimal codepoint (format 0xXXXX)");
    icon_hex_option->type_name("CODE");

    auto description_option = db_command->add_option_function<std::string>("--description",
        [&](const std::string& value) { builder.SetDbDescription(value); },
        "entry description");
    description_option->type_name("TEXT");

    auto used_by_option = db_command->add_option_function<std::string>("--used_by",
        [&](const std::string& value) { builder.SetDbUsedBy(value); },
        "entry usage notes");
    used_by_option->type_name("TEXT");

    auto alias_option = db_command->add_option_function<std::string>("--alias",
        [&](const std::string& value) { builder.SetDbAlias(value); },
        "alias to assign (empty string removes alias)");
    alias_option->type_name("TEXT");

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

    std::string tree_value;
    auto tree_option = layout->add_flag("--tree{0}", tree_value,
        "show tree view of directories, optionally limited to DEPTH (0 for unlimited)");
    tree_option->type_name("DEPTH");
    tree_option->option_text("[=DEPTH]");
    tree_option->expected(0, 1);
    tree_option->default_str("0");

    auto recursive_option = layout->add_flag_callback("-R,--recursive",
        [&]() { builder.SetRecursiveFlat(true); },
        "recursively list subdirectories in flat format (like ls -R)");
    recursive_option->excludes(tree_option);
    tree_option->excludes(recursive_option);

    std::string report_value;
    auto report_option = layout->add_flag("--report{long}", report_value,
        R"(show summary report: short, long (default: long)
)");
    report_option->type_name("WORD");
    report_option->option_text("[=WORD]");
    report_option->expected(0, 1);
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

    sorting->add_flag_callback("--sd,--sort-dirs,--group-directories-first", [&]() {
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

    auto theme_option = appearance->add_option_function<std::string>("--theme",
        [&](const std::string& raw_theme) {
            std::string theme = StringUtils::Trim(raw_theme);
            if (theme.empty()) {
                throw CLI::ValidationError("--theme", "theme name cannot be empty");
            }

            auto ends_with = [](const std::string& value, std::string_view suffix) {
                return value.size() >= suffix.size()
                    && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
            };

            if (ends_with(theme, ".yaml")) {
                theme.erase(theme.size() - 5);
            }
            if (ends_with(theme, "_theme")) {
                theme.erase(theme.size() - 6);
            }

            if (theme.empty()) {
                throw CLI::ValidationError("--theme", "theme name cannot be empty");
            }
            if (theme.find('/') != std::string::npos || theme.find('\\') != std::string::npos) {
                throw CLI::ValidationError("--theme", "theme name must not contain path separators");
            }

            builder.SetThemeName(std::move(theme));
        },
        "use theme NAME from the configuration database");
    theme_option->type_name("NAME");

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
        R"(use time display format: default, locale, local,
long-iso, full-iso, iso, iso8601,
FORMAT (default: local))");
    time_style_option->type_name("FORMAT");
    time_style_option->default_str("local");
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

    auto debug = program.add_option_group("Debug options");
    debug->add_flag_callback("--perf-debug", [&]() { builder.SetPerfLogging(true); },
        "enable performance diagnostics");

    std::vector<std::optional<std::string>> tree_arguments;
    std::vector<std::optional<std::string>> report_arguments;
    std::vector<std::string> arg_storage;
    std::vector<const char*> parse_argv;
    arg_storage.reserve(static_cast<std::size_t>(argc));
    parse_argv.reserve(static_cast<std::size_t>(argc));
    arg_storage.emplace_back(argv[0] != nullptr ? argv[0] : "");
    parse_argv.push_back(arg_storage.back().c_str());

    for (int i = 1; i < argc; ++i) {
        std::string_view current(argv[i] != nullptr ? argv[i] : "");
        if (current == "--") {
            arg_storage.emplace_back(current);
            parse_argv.push_back(arg_storage.back().c_str());
            for (int j = i + 1; j < argc; ++j) {
                arg_storage.emplace_back(argv[j] != nullptr ? argv[j] : "");
                parse_argv.push_back(arg_storage.back().c_str());
            }
            break;
        }

        if (current == "--report") {
            std::optional<std::string> value;
            if (i + 1 < argc) {
                std::string_view next(argv[i + 1] != nullptr ? argv[i + 1] : "");
                if (!next.empty() && next != "--" && next.front() != '-') {
                    std::string lowered = StringUtils::ToLower(next);
                    if (report_map.find(lowered) != report_map.end()) {
                        value = std::string(next);
                        ++i;
                    }
                }
            }
            report_arguments.push_back(value);
            std::string actual = "--report=" + (value ? StringUtils::ToLower(*value) : std::string("long"));
            arg_storage.emplace_back(std::move(actual));
            parse_argv.push_back(arg_storage.back().c_str());
            continue;
        }

        if (current.rfind("--report=", 0) == 0) {
            std::string original = std::string(current.substr(9));
            report_arguments.emplace_back(original);
            arg_storage.emplace_back(std::string("--report=") + StringUtils::ToLower(original));
            parse_argv.push_back(arg_storage.back().c_str());
            continue;
        }

        if (current == "--tree") {
            std::optional<std::string> value;
            if (i + 1 < argc) {
                std::string_view next(argv[i + 1] != nullptr ? argv[i + 1] : "");
                if (!next.empty() && next != "--" && next.front() != '-') {
                    bool numeric = std::all_of(next.begin(), next.end(), [](unsigned char ch) {
                        return std::isdigit(ch) != 0;
                    });
                    if (numeric) {
                        value = std::string(next);
                        ++i;
                    }
                }
            }
            tree_arguments.push_back(value);
            std::string actual = "--tree=" + (value ? *value : std::string("0"));
            arg_storage.emplace_back(std::move(actual));
            parse_argv.push_back(arg_storage.back().c_str());
            continue;
        }

        if (current.rfind("--tree=", 0) == 0) {
            std::string value = std::string(current.substr(7));
            tree_arguments.emplace_back(value);
            arg_storage.emplace_back(std::string("--tree=") + value);
            parse_argv.push_back(arg_storage.back().c_str());
            continue;
        }

        arg_storage.emplace_back(current);
        parse_argv.push_back(arg_storage.back().c_str());
    }

    int patched_argc = static_cast<int>(parse_argv.size());

    try {
        program.parse(patched_argc, parse_argv.data());
    } catch (const CLI::ParseError& e) {
        std::exit(program.exit(e));
    }

    auto ensure_missing = [](std::vector<std::string>& missing, CLI::Option* opt, const char* label) {
        if (opt->count() == 0) {
            missing.emplace_back(label);
        }
    };

    auto join_labels = [](const std::vector<std::string>& labels) {
        std::ostringstream oss;
        for (size_t i = 0; i < labels.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << labels[i];
        }
        return oss.str();
    };

    bool db_mode = builder.db_mode();
    auto db_action = builder.db_action();
    if (!db_mode) {
        if (db_action != Config::DbAction::None) {
            throw CLI::ValidationError("db", "db options require the 'db' subcommand");
        }
    } else if (db_action == Config::DbAction::None) {
        throw CLI::ValidationError("db", "one of --show-* or --set-* flags must be provided");
    }

    if (db_mode && (db_action == Config::DbAction::SetFile || db_action == Config::DbAction::SetFolder)) {
        std::vector<std::string> missing;
        ensure_missing(missing, name_option, "--name");
        ensure_missing(missing, icon_option, "--icon");
        ensure_missing(missing, icon_class_option, "--icon_class");
        ensure_missing(missing, icon_utf_option, "--icon_utf_16_codes");
        ensure_missing(missing, icon_hex_option, "--icon_hex_code");
        ensure_missing(missing, description_option, "--description");
        ensure_missing(missing, used_by_option, "--used_by");
        if (!missing.empty()) {
            std::ostringstream oss;
            oss << "missing required option(s) for "
                << (db_action == Config::DbAction::SetFile ? "--set-file" : "--set-folder")
                << ": " << join_labels(missing);
            throw CLI::ValidationError("db", oss.str());
        }
        if (alias_option->count() > 0) {
            throw CLI::ValidationError("db", "--alias is not valid with --set-file/--set-folder");
        }
    } else if (db_mode && (db_action == Config::DbAction::SetFileAlias || db_action == Config::DbAction::SetFolderAlias)) {
        std::vector<std::string> missing;
        ensure_missing(missing, name_option, "--name");
        ensure_missing(missing, alias_option, "--alias");
        if (!missing.empty()) {
            std::ostringstream oss;
            oss << "missing required option(s) for "
                << (db_action == Config::DbAction::SetFileAlias ? "--set-file-aliases" : "--set-folder-aliases")
                << ": " << join_labels(missing);
            throw CLI::ValidationError("db", oss.str());
        }
        if (icon_option->count() > 0 || icon_class_option->count() > 0 ||
            icon_utf_option->count() > 0 || icon_hex_option->count() > 0 ||
            description_option->count() > 0 || used_by_option->count() > 0) {
            throw CLI::ValidationError("db", "icon metadata options are not valid with alias commands");
        }
    } else if (db_mode) {
        const bool allow_name_filter =
            db_action == Config::DbAction::ShowFiles ||
            db_action == Config::DbAction::ShowFolders ||
            db_action == Config::DbAction::ShowFileAliases ||
            db_action == Config::DbAction::ShowFolderAliases;

        const bool name_used = name_option->count() > 0;
        const bool metadata_used = icon_option->count() > 0 || icon_class_option->count() > 0 ||
                                   icon_utf_option->count() > 0 || icon_hex_option->count() > 0 ||
                                   description_option->count() > 0 || used_by_option->count() > 0;
        const bool alias_used = alias_option->count() > 0;

        if (metadata_used || alias_used || (!allow_name_filter && name_used)) {
            throw CLI::ValidationError(
                "db",
                allow_name_filter
                    ? "only --name may accompany --show-* commands"
                    : "metadata options require a --set-* command");
        }
    }

    if (!tree_arguments.empty()) {
        std::optional<std::size_t> depth;
        for (const auto& value : tree_arguments) {
            if (!value || value->empty()) {
                depth.reset();
                continue;
            }
            try {
                size_t idx = 0;
                unsigned long long parsed = std::stoull(*value, &idx, 10);
                if (idx != value->size()) {
                    throw CLI::ValidationError("--tree", "invalid value '" + *value + "'");
                }
                if (parsed == 0) {
                    depth.reset();
                } else {
                    depth = static_cast<std::size_t>(parsed);
                }
            } catch (const std::exception&) {
                throw CLI::ValidationError("--tree", "invalid value '" + *value + "'");
            }
        }
        builder.EnableTree(depth);
    }

    if (!report_arguments.empty()) {
        Config::Report report = Config::Report::Long;
        for (const auto& value : report_arguments) {
            if (!value || value->empty()) {
                report = Config::Report::Long;
                continue;
            }
            std::string normalized = StringUtils::ToLower(*value);
            auto it = report_map.find(normalized);
            if (it == report_map.end()) {
                throw CLI::ValidationError("--report", "invalid value '" + *value + "'");
            }
            report = it->second;
        }
        builder.SetReport(report);
    }

    Config& options = builder.Build();
    return options;
}

} // namespace nls
