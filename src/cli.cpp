#include "nicels/cli.hpp"

#include "nicels/logger.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace nicels {

Cli::Cli() : app_{"nicels", "Modern, colorful ls clone"} {
    configure();
}

void Cli::configure() {
    app_.set_help_all_flag("--help-all", "Print all help");
    app_.allow_windows_style_options();
    app_.option_defaults()->always_capture_default(true);
    app_.formatter(std::make_shared<CLI::Formatter>());

    auto* verbose = app_.add_flag("-v,--verbose", "Increase logging verbosity");
    verbose->callback([]() { Logger::instance().set_level(LogLevel::Info); });

    auto* debug = app_.add_flag("--debug", "Enable debug logging");
    debug->callback([]() { Logger::instance().set_level(LogLevel::Debug); });

    app_.add_flag("-a,--all", "Include directory entries whose names begin with a dot");
    app_.add_flag("-A,--almost-all", "Like --all but ignore '.' and '..'");
    app_.add_flag("-d,--directory", "List directories themselves, not their contents");
    app_.add_flag("-f,--files", "List files only");
    app_.add_flag("--group-directories-first", "Group directories before files");
    app_.add_flag("--group-directories-last", "Group directories after files");
    app_.add_flag("-l", "Use a long listing format");
    app_.add_flag("-1", "List one file per line");
    app_.add_flag("-r,--reverse", "Reverse order while sorting");
    app_.add_flag("-t", "Sort by modification time");
    app_.add_flag("-S", "Sort by file size");
    app_.add_flag("-X", "Sort by file extension");
    app_.add_flag("--tree", "Show a tree view");
    app_.add_option("--tree-depth", "Limit tree depth", true);
    app_.add_flag("--no-icons", "Disable icons");
    app_.add_flag("--no-color", "Disable ANSI colors");
    app_.add_flag("--color", "Always enable ANSI colors");
    app_.add_flag("--git-status", "Display git status prefixes when inside a repo");
    app_.add_flag("-I,--ignore-backups", "Ignore files ending with ~");
    app_.add_flag("--hyperlink", "Print clickable hyperlinks when supported");
    app_.add_flag("--header", "Print directory headers");
    app_.add_flag("--inode", "Print inode numbers");
    app_.add_flag("--numeric-ids", "List numeric user and group IDs");
    app_.add_flag("--dereference", "Follow symbolic links");
    app_.add_flag("--hide-control-chars", "Replace control characters with '?'");
    app_.add_flag("-0,--zero", "End each output line with NUL, not newline");
    app_.add_flag("--block-size", "Show block size instead of bytes");

    app_.add_option("--width", "Assume screen width", true);
    app_.add_option("--time-style", "Time display style", true);
    app_.add_option("--hide", "Glob of files to hide")->option_text("PATTERN");
    app_.add_option("--ignore", "Glob of files to ignore")->option_text("PATTERN");

    app_.add_flag("--dump-cli-markdown", "Print CLI markdown documentation and exit");

    auto* paths = app_.add_option("paths", "Paths to list");
    paths->type_size(-1);
    paths->expected(-1);
    paths->take_all();
}

Options Cli::parse(int argc, const char* const argv[]) {
    Options opts;

    std::vector<std::string> positional{};

    bool dump_markdown = false;
    app_.set_callback([&]() {
        dump_markdown = app_.get_option("--dump-cli-markdown")->count() > 0;
        opts.all = app_.get_option("-a")->count() > 0;
        opts.almost_all = app_.get_option("-A")->count() > 0;
        opts.directories_only = app_.get_option("-d")->count() > 0;
        opts.files_only = app_.get_option("-f")->count() > 0;
        opts.group_directories_first = app_.get_option("--group-directories-first")->count() > 0;
        opts.sort_directories_last = app_.get_option("--group-directories-last")->count() > 0;
        opts.format = app_.get_option("-l")->count() ? FormatStyle::Long : opts.format;
        opts.format = app_.get_option("-1")->count() ? FormatStyle::Single : opts.format;
        if (app_.get_option("--tree")->count()) {
            opts.format = FormatStyle::Tree;
        }
        opts.reverse = app_.get_option("-r")->count() > 0;
        if (app_.get_option("-t")->count()) {
            opts.sort = SortField::Time;
        } else if (app_.get_option("-S")->count()) {
            opts.sort = SortField::Size;
        } else if (app_.get_option("-X")->count()) {
            opts.sort = SortField::Extension;
        }
        if (const auto opt = app_.get_option("--tree-depth"); opt && opt->count()) {
            opts.tree_depth = opt->as<std::size_t>();
        }
        if (const auto opt = app_.get_option("--width"); opt && opt->count()) {
            opts.output_width = opt->as<int>();
        }
        if (const auto opt = app_.get_option("--time-style"); opt && opt->count()) {
            opts.time_style = opt->as<std::string>();
        }
        opts.icons = app_.get_option("--no-icons")->count() == 0;
        if (app_.get_option("--no-color")->count()) {
            opts.colors = false;
            opts.color_policy = ColorPolicy::Never;
        }
        if (app_.get_option("--color")->count()) {
            opts.colors = true;
            opts.color_policy = ColorPolicy::Always;
        }
        opts.git_status = app_.get_option("--git-status")->count() > 0;
        opts.hyperlink = app_.get_option("--hyperlink")->count() > 0;
        opts.show_header = app_.get_option("--header")->count() > 0;
        opts.show_inode = app_.get_option("--inode")->count() > 0;
        opts.numeric_ids = app_.get_option("--numeric-ids")->count() > 0;
        opts.follow_symlinks = app_.get_option("--dereference")->count() > 0;
        opts.hide_control_chars = app_.get_option("--hide-control-chars")->count() > 0;
        opts.zero_terminate = app_.get_option("-0")->count() > 0;
        opts.show_block_size = app_.get_option("--block-size")->count() > 0;
        opts.ignore_backups = app_.get_option("-I")->count() > 0;

        if (const auto opt = app_.get_option("--hide")) {
            for (const auto& value : opt->as<std::vector<std::string>>()) {
                opts.hide_patterns.push_back(value);
            }
        }
        if (const auto opt = app_.get_option("--ignore")) {
            for (const auto& value : opt->as<std::vector<std::string>>()) {
                opts.ignore_patterns.push_back(value);
            }
        }
    });

    try {
        app_.parse(argc, argv);
    } catch (const CLI::ParseError& err) {
        std::exit(app_.exit(err));
    }

    positional = app_.remaining();
    if (!positional.empty()) {
        opts.paths = positional;
    }

    if (dump_markdown) {
        write_markdown(std::cout);
        std::exit(0);
    }

    return opts;
}

void Cli::write_markdown(std::ostream& os) const {
    os << "| Option | Description |\n";
    os << "| --- | --- |\n";
    for (const auto& opt : app_.get_options({})) {
        if (!opt->get_description().empty()) {
            os << "| ``" << opt->get_name(false, true) << "`` | " << opt->get_description() << " |\n";
        }
    }
}

} // namespace nicels
