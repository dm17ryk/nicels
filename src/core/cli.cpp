#include "nicels/cli.h"
#include "nicels/cli_formatter.h"

#include <algorithm>
#include <sstream>

#include "nicels/config.h"

namespace nicels {

namespace {
constexpr std::string_view kDescription =
    "nicels — a modern, colorful, cross-platform ls with git status and icons";
}

Cli::Cli()
    : app_{std::make_unique<CLI::App>(std::string{kDescription})} {
    app_->formatter(std::make_shared<ColorFormatter>());
    app_->set_version_flag("--version", "1.0.0");
    app_->footer(R"(The SIZE argument is an integer and optional unit (example: 10K is 10*1024).
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

    add_layout_options();
    add_filter_options();
    add_sort_options();
    add_appearance_options();
    add_information_options();

    auto* width = app_->add_option("--width", options_.output_width, "Override detected terminal width");
    width->check(CLI::Range(20, 400));
    document_option(width);

    auto* tab_size = app_->add_option("--tabsize", options_.tab_size, "Set tab size when aligning columns");
    tab_size->check(CLI::Range(2, 16));
    document_option(tab_size);

    auto* time_style = app_->add_option("--time-style", options_.time_style,
                                         "Specify time display style (full-iso, long-iso, iso, locale)");
    document_option(time_style);

    auto* hide_patterns = app_->add_option("--hide", options_.hide_patterns, "Hide entries matching glob pattern");
    hide_patterns->expected(0, -1);
    document_option(hide_patterns);

    auto* ignore_patterns =
        app_->add_option("--ignore", options_.ignore_patterns, "Ignore entries matching glob pattern");
    ignore_patterns->expected(0, -1);
    document_option(ignore_patterns);

    auto* block_size =
        app_->add_option("--block-size", options_.block_size_suffix, "Scale block sizes using suffix (K,M,G,Ki,Mi,...)");
    document_option(block_size);

    auto* paths = app_->add_option("paths", options_.paths, "Directories or files to display")->expected(0, -1);
    paths->check(CLI::ExistingPath);
    document_option(paths);

    auto* dump = app_->add_flag("--dump-markdown", options_.dump_markdown,
                                "Print CLI options as markdown and exit");
    dump->configurable(false);
    document_option(dump);
}

Cli::~Cli() = default;

void Cli::add_layout_options() {
    auto layout = app_->add_option_group("Layout");

    document_option(layout->add_flag_callback("-l,--long", [&]() { options_.format = Config::FormatStyle::Long; },
                                             "Use long listing format"));

    document_option(layout->add_flag_callback("-1,--one-per-line",
                                              [&]() { options_.format = Config::FormatStyle::SingleColumn; },
                                              "List one entry per line"));

    document_option(layout->add_flag_callback("-x",
                                              [&]() { options_.format = Config::FormatStyle::ColumnsHorizontal; },
                                              "List entries by lines instead of columns"));

    document_option(layout->add_flag_callback("-C",
                                              [&]() { options_.format = Config::FormatStyle::Columns; },
                                              "List entries in columns"));

    document_option(layout->add_flag_callback("-m",
                                              [&]() { options_.format = Config::FormatStyle::CommaSeparated; },
                                              "List entries separated by commas"));

    document_option(layout->add_flag_callback("--header", [&]() { options_.header = true; },
                                              "Show header row for long listings"));

    document_option(layout->add_flag_callback("--tree",
                                              [&]() {
                                                  options_.tree = true;
                                                  options_.format = Config::FormatStyle::SingleColumn;
                                              },
                                              "Render directories as a tree"));

    auto* depth = layout->add_option("--tree-depth", options_.tree_depth, "Limit depth when using --tree");
    depth->check(CLI::Range(1u, 64u));
    document_option(depth);

    document_option(layout->add_flag_callback("--zero", [&]() { options_.zero_terminate = true; },
                                              "Terminate lines with NUL"));
}

void Cli::add_filter_options() {
    auto filtering = app_->add_option_group("Filtering");

    document_option(filtering->add_flag_callback("-a,--all", [&]() { options_.show_all = true; },
                                                 "Include dot files"));

    document_option(filtering->add_flag_callback("-A,--almost-all", [&]() { options_.show_almost_all = true; },
                                                 "Include dot files except . and .."));

    document_option(filtering->add_flag_callback("-d,--dirs", [&]() {
                        options_.directories_only = true;
                        options_.files_only = false;
                    },
                    "List directories only"));

    document_option(filtering->add_flag_callback("-f,--files", [&]() {
                        options_.files_only = true;
                        options_.directories_only = false;
                    },
                    "List files only"));

    document_option(filtering->add_flag_callback("-B,--ignore-backups", [&]() { options_.ignore_backups = true; },
                                                 "Ignore entries ending with ~"));
}

void Cli::add_sort_options() {
    auto sorting = app_->add_option_group("Sorting");

    document_option(sorting->add_flag_callback("-t", [&]() { options_.sort_mode = Config::SortMode::Time; },
                                              "Sort by modification time"));

    document_option(sorting->add_flag_callback("-S", [&]() { options_.sort_mode = Config::SortMode::Size; },
                                              "Sort by size"));

    document_option(sorting->add_flag_callback("-X", [&]() { options_.sort_mode = Config::SortMode::Extension; },
                                              "Sort by file extension"));

    document_option(sorting->add_flag_callback("-U", [&]() { options_.sort_mode = Config::SortMode::None; },
                                              "Do not sort"));

    document_option(sorting->add_flag_callback("-r,--reverse", [&]() { options_.reverse = true; },
                                              "Reverse order"));

    document_option(sorting->add_flag_callback("--sd,--sort-dirs,--group-directories-first", [&]() {
                        options_.group_directories_first = true;
                    },
                    "Group directories before files"));

    document_option(sorting->add_flag_callback("--sf,--sort-files", [&]() { options_.sort_files_first = true; },
                                              "Place files before directories"));

    document_option(sorting->add_flag_callback("--df,--dots-first", [&]() { options_.dots_first = true; },
                                              "Place dot entries first"));
}

void Cli::add_appearance_options() {
    auto appearance = app_->add_option_group("Appearance");

    document_option(appearance->add_flag_callback("--no-icons,--without-icons", [&]() { options_.icons_enabled = false; },
                                                 "Disable icons"));

    document_option(appearance->add_flag_callback("--no-color", [&]() { options_.color_enabled = false; },
                                                 "Disable ANSI colors"));

    document_option(appearance->add_flag_callback("--light", [&]() { options_.color_theme = Config::ColorTheme::Light; },
                                                 "Use light color theme"));

    document_option(appearance->add_flag_callback("--dark", [&]() { options_.color_theme = Config::ColorTheme::Dark; },
                                                 "Use dark color theme"));

    document_option(appearance->add_flag_callback("-q,--hide-control-chars", [&]() { options_.hide_control_chars = true; },
                                                 "Hide control characters in filenames"));

    document_option(appearance->add_flag_callback("--show-control-chars", [&]() { options_.hide_control_chars = false; },
                                                 "Show control characters"));
}

void Cli::add_information_options() {
    auto info = app_->add_option_group("Information");

    document_option(info->add_flag_callback("-i,--inode", [&]() { options_.show_inode = true; },
                                           "Display inode numbers"));

    document_option(info->add_flag_callback("-o", [&]() { options_.show_group = false; },
                                           "Do not show group"));

    document_option(info->add_flag_callback("-g", [&]() { options_.show_owner = false; },
                                           "Do not show owner"));

    document_option(info->add_flag_callback("-G,--no-group", [&]() { options_.show_group = false; },
                                           "Do not show group"));

    document_option(info->add_flag_callback("-n,--numeric-uid-gid", [&]() { options_.numeric_uid_gid = true; },
                                           "Show numeric IDs"));

    document_option(info->add_flag_callback("--bytes,--non-human-readable", [&]() { options_.show_bytes = true; },
                                           "Show sizes in bytes"));

    document_option(info->add_flag_callback("-s,--size", [&]() { options_.show_block_size = true; },
                                           "Show allocated blocks"));

    document_option(info->add_flag_callback("--gs,--git-status", [&]() { options_.show_git_status = true; },
                                           "Include git status column"));

    document_option(info->add_flag_callback("--hyperlink", [&]() { options_.hyperlink = true; },
                                           "Emit OSC 8 hyperlinks"));

    document_option(info->add_flag_callback("-L,--dereference", [&]() { options_.dereference = true; },
                                           "Follow symlinks"));
}

Config::Options Cli::parse(int argc, char** argv) {
    options_ = Config::Options{};
    try {
        app_->parse(argc, argv);
    } catch (const CLI::ParseError& ex) {
        std::exit(app_->exit(ex));
    }
    // app_->parse(argc, argv);
    return options_;
}

std::string Cli::usage_markdown() const {
    std::ostringstream out;
    out << "### Command line options\n\n";
    out << "| Option | Description | Default |\n";
    out << "| ------ | ----------- | ------- |\n";
    for (const auto& doc : docs_) {
        out << "| `" << doc.name << "` | " << doc.description << " | "
            << (doc.default_value.empty() ? "" : doc.default_value) << " |\n";
    }
    out << '\n';
    return out.str();
}

} // namespace nicels
