#include "nicels/cli.h"

#include <algorithm>
#include <sstream>

#include "nicels/config.h"
#include "nicels/logger.h"

namespace nicels {

namespace {
constexpr std::string_view kDescription =
    "nicels — a modern, colorful, cross-platform ls with git status and icons";
}

Cli::Cli()
    : app_{std::make_unique<CLI::App>(std::string{kDescription})} {
    app_->set_help_all_flag("--help-all", "Show help with all grouped options");
    app_->allow_extras();
    app_->set_help_all_flag("--help-all", "Show all help information");

    register_layout_options();
    register_filter_options();
    register_sort_options();
    register_appearance_options();
    register_information_options();

    auto* width = app_->add_option("--width", options_.output_width, "Override detected terminal width");
    width->check(CLI::Range(20, 400));
    remember_option(width, "Override detected terminal width");

    auto* tab_size = app_->add_option("--tabsize", options_.tab_size, "Set tab size when aligning columns");
    tab_size->check(CLI::Range(2, 16));
    remember_option(tab_size, "Set tab size when aligning columns");

    auto* time_style = app_->add_option("--time-style", options_.time_style,
                                         "Specify time display style (full-iso, long-iso, iso, locale)");
    remember_option(time_style, "Specify time display style (full-iso, long-iso, iso, locale)");

    auto* hide_patterns =
        app_->add_option("--hide", options_.hide_patterns, "Hide entries matching glob pattern")->allow_extra_vals();
    remember_option(hide_patterns, "Hide entries matching glob pattern");

    auto* ignore_patterns = app_->add_option("--ignore", options_.ignore_patterns,
                                             "Ignore entries matching glob pattern")
                                  ->allow_extra_vals();
    remember_option(ignore_patterns, "Ignore entries matching glob pattern");

    auto* block_size =
        app_->add_option("--block-size", options_.block_size_suffix, "Scale block sizes using suffix (K,M,G,Ki,Mi,...)");
    remember_option(block_size, "Scale block sizes using suffix (K,M,G,Ki,Mi,...)");

    auto* paths = app_->add_option("paths", options_.paths, "Directories or files to display")->expected(0, -1);
    paths->check(CLI::ExistingPath);
    remember_option(paths, "Directories or files to display");
}

Cli::~Cli() = default;

void Cli::register_layout_options() {
    auto layout = app_->add_option_group("Layout");
    layout->require_option(0);

    remember_option(layout->add_flag_callback("-l,--long", [&]() { options_.format = Config::FormatStyle::Long; },
                                             "Use long listing format"),
                    "Use long listing format");

    remember_option(layout->add_flag_callback("-1,--one-per-line",
                                              [&]() { options_.format = Config::FormatStyle::SingleColumn; },
                                              "List one entry per line"),
                    "List one entry per line");

    remember_option(layout->add_flag_callback("-x",
                                              [&]() { options_.format = Config::FormatStyle::ColumnsHorizontal; },
                                              "List entries by lines instead of columns"),
                    "List entries by lines instead of columns");

    remember_option(layout->add_flag_callback("-C",
                                              [&]() { options_.format = Config::FormatStyle::Columns; },
                                              "List entries in columns"),
                    "List entries in columns");

    remember_option(layout->add_flag_callback("-m",
                                              [&]() { options_.format = Config::FormatStyle::CommaSeparated; },
                                              "List entries separated by commas"),
                    "List entries separated by commas");

    remember_option(layout->add_flag_callback("--header", [&]() { options_.header = true; },
                                              "Show headers in long listing"),
                    "Show headers in long listing");

    remember_option(layout->add_flag_callback("--tree",
                                              [&]() {
                                                  options_.tree = true;
                                                  options_.format = Config::FormatStyle::SingleColumn;
                                              },
                                              "Display tree view"),
                    "Display tree view");

    auto* depth = layout->add_option("--tree-depth", options_.tree_depth, "Limit tree depth");
    depth->check(CLI::Range(1u, 64u));
    remember_option(depth, "Limit tree depth");

    remember_option(layout->add_flag_callback("--zero", [&]() { options_.zero_terminate = true; },
                                              "Terminate lines with NUL"),
                    "Terminate lines with NUL");
}

void Cli::register_filter_options() {
    auto filtering = app_->add_option_group("Filtering");
    filtering->require_option(0);

    remember_option(filtering->add_flag_callback("-a,--all", [&]() { options_.show_all = true; },
                                                 "Include directory entries whose names begin with a dot"),
                    "Include directory entries whose names begin with a dot");

    remember_option(filtering->add_flag_callback("-A,--almost-all", [&]() { options_.show_almost_all = true; },
                                                 "Include hidden entries except . and .."),
                    "Include hidden entries except . and ..");

    remember_option(filtering->add_flag_callback("-d,--dirs", [&]() {
                        options_.directories_only = true;
                        options_.files_only = false;
                    },
                    "List directories only"),
                    "List directories only");

    remember_option(filtering->add_flag_callback("-f,--files", [&]() {
                        options_.files_only = true;
                        options_.directories_only = false;
                    },
                    "List regular files only"),
                    "List regular files only");

    remember_option(filtering->add_flag_callback("-B,--ignore-backups", [&]() { options_.ignore_backups = true; },
                                                 "Ignore entries ending with ~"),
                    "Ignore entries ending with ~");
}

void Cli::register_sort_options() {
    auto sorting = app_->add_option_group("Sorting");
    sorting->require_option(0);

    remember_option(sorting->add_flag_callback("-t", [&]() { options_.sort_mode = Config::SortMode::Time; },
                                              "Sort by modification time"),
                    "Sort by modification time");

    remember_option(sorting->add_flag_callback("-S", [&]() { options_.sort_mode = Config::SortMode::Size; },
                                              "Sort by file size"),
                    "Sort by file size");

    remember_option(sorting->add_flag_callback("-X", [&]() { options_.sort_mode = Config::SortMode::Extension; },
                                              "Sort by file extension"),
                    "Sort by file extension");

    remember_option(sorting->add_flag_callback("-U", [&]() { options_.sort_mode = Config::SortMode::None; },
                                              "Do not sort; list entries in directory order"),
                    "Do not sort; list entries in directory order");

    remember_option(sorting->add_flag_callback("-r,--reverse", [&]() { options_.reverse = true; },
                                              "Reverse the order while sorting"),
                    "Reverse the order while sorting");

    remember_option(sorting->add_flag_callback("--group-directories-first,--sd,--sort-dirs", [&]() {
                        options_.group_directories_first = true;
                    },
                    "Group directories before files"),
                    "Group directories before files");

    remember_option(sorting->add_flag_callback("--sf,--sort-files", [&]() {
                        options_.sort_files_first = true;
                    },
                    "Sort files before directories"),
                    "Sort files before directories");

    remember_option(sorting->add_flag_callback("--df,--dots-first", [&]() { options_.dots_first = true; },
                                              "Show dot entries before others"),
                    "Show dot entries before others");
}

void Cli::register_appearance_options() {
    auto appearance = app_->add_option_group("Appearance");
    appearance->require_option(0);

    remember_option(appearance->add_flag_callback("--no-icons,--without-icons", [&]() { options_.icons_enabled = false; },
                                                 "Disable file type icons"),
                    "Disable file type icons");

    remember_option(appearance->add_flag_callback("--no-color", [&]() { options_.color_enabled = false; },
                                                 "Disable ANSI colors"),
                    "Disable ANSI colors");

    remember_option(appearance->add_flag_callback("--light", [&]() { options_.color_theme = Config::ColorTheme::Light; },
                                                 "Use light color theme"),
                    "Use light color theme");
}

void Cli::register_information_options() {
    auto info = app_->add_option_group("Information");
    info->require_option(0);

    remember_option(info->add_flag_callback("-i,--inode", [&]() { options_.show_inode = true; },
                                           "Print index number of each file"),
                    "Print index number of each file");

    remember_option(info->add_flag_callback("-o", [&]() { options_.show_group = false; },
                                           "Do not list group information"),
                    "Do not list group information");

    remember_option(info->add_flag_callback("-g", [&]() { options_.show_owner = false; },
                                           "Do not list owner"),
                    "Do not list owner");

    remember_option(info->add_flag_callback("-G,--no-group", [&]() { options_.show_group = false; },
                                           "Do not list group information"),
                    "Do not list group information");

    remember_option(info->add_flag_callback("-n,--numeric-uid-gid", [&]() { options_.numeric_uid_gid = true; },
                                           "List numeric user and group IDs"),
                    "List numeric user and group IDs");

    remember_option(info->add_flag_callback("--bytes,--non-human-readable", [&]() { options_.show_bytes = true; },
                                           "List sizes in bytes"),
                    "List sizes in bytes");

    remember_option(info->add_flag_callback("-s,--size", [&]() { options_.show_block_size = true; },
                                           "Print allocated size of each file"),
                    "Print allocated size of each file");

    remember_option(info->add_flag_callback("--gs,--git-status", [&]() { options_.show_git_status = true; },
                                           "Display git status alongside file entries"),
                    "Display git status alongside file entries");

    remember_option(info->add_flag_callback("--hyperlink", [&]() { options_.hyperlink = true; },
                                           "Emit OSC 8 hyperlinks for entries"),
                    "Emit OSC 8 hyperlinks for entries");
}

Config::Options Cli::parse(int argc, char** argv) {
    options_ = Config::Options{};
    app_->parse(argc, argv);
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
