#include "nicels/cli.hpp"

#include "nicels/config.hpp"
#include "nicels/logger.hpp"

#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Option.hpp>
#include <CLI/Validators.hpp>

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace nicels {

namespace {
struct CliState {
    ListingOptions options;
    std::vector<std::string> raw_paths;
    std::string locale;
    std::string log_level{"error"};
    std::string git_mode{"auto"};
    bool markdown{false};
};

LogLevel parse_log_level(const std::string& value) {
    static const std::map<std::string, LogLevel, std::less<>> table{
        {"error", LogLevel::Error},
        {"warn", LogLevel::Warn},
        {"warning", LogLevel::Warn},
        {"info", LogLevel::Info},
        {"debug", LogLevel::Debug},
        {"trace", LogLevel::Trace},
    };
    auto it = table.find(value);
    if (it == table.end()) {
        throw CLI::ValidationError("--log-level", "invalid log level: " + value);
    }
    return it->second;
}

GitStatusMode parse_git_mode(const std::string& value) {
    static const std::map<std::string, GitStatusMode, std::less<>> table{
        {"auto", GitStatusMode::Auto},
        {"always", GitStatusMode::Always},
        {"never", GitStatusMode::Never},
    };
    auto it = table.find(value);
    if (it == table.end()) {
        throw CLI::ValidationError("--git-status", "invalid mode: " + value);
    }
    return it->second;
}

} // namespace

Cli::Cli()
    : app_(std::make_unique<CLI::App>("Modern, cross-platform nicels listing utility")) {
    app_->set_help_all_flag("--help-all", "Print all help, including hidden options");
}

Cli::~Cli() = default;

int Cli::parse(int argc, char** argv, Config& config) {
    CliState state{};
    app_ = std::make_unique<CLI::App>("Modern, cross-platform nicels listing utility");
    app_->require_subcommand(0);
    app_->set_help_all_flag("--help-all", "Print all help, including hidden options");

    auto& logger = Logger::instance();

    auto* version_flag = app_->add_flag("-V,--version", "Print version information and exit");
    auto* markdown_flag = app_->add_flag("--help-markdown", state.markdown,
        "Print CLI options in Markdown table format");
    markdown_flag->configurable(false);

    app_->add_option("paths", state.raw_paths, "Paths to inspect")->type_name("PATH")->expected(-1);

    app_->add_flag("-l,--long", state.options.long_format, "Use a long listing format");
    app_->add_flag("-1", state.options.single_column, "List one entry per line");
    app_->add_flag("-a,--all", state.options.include_all, "Include directory entries whose names begin with a dot (.)");
    app_->add_flag("-A,--almost-all", state.options.almost_all,
        "Include almost all entries, excluding '.' and '..'");
    app_->add_flag("-d,--dirs", state.options.directories_only, "List directories themselves, not their contents");
    app_->add_flag("-f,--files", state.options.files_only, "List only files (omit directories)");
    app_->add_flag("-t,--time", state.options.sort_time, "Sort by modification time, newest first");
    app_->add_flag("-S,--size", state.options.sort_size, "Sort by file size, largest first");
    app_->add_flag("-r,--reverse", state.options.reverse_sort, "Reverse the order while sorting");
    app_->add_flag("--group-directories-first", state.options.group_directories_first,
        "Group directories before files");
    app_->add_flag_callback("--no-icons", [&]() { state.options.enable_icons = false; }, "Disable icons in output");
    app_->add_flag_callback("--no-color", [&]() { state.options.enable_color = false; }, "Disable ANSI colors");
    app_->add_flag("--classify", state.options.classify, "Append indicator (one of */=>@|) to entries");
    app_->add_flag_callback("--no-control-char-filter", [&]() { state.options.hide_control_chars = false; },
        "Show control characters rather than replacing them")->configurable(false);

    app_->add_flag("--tree", state.options.tree, "Display directories as a tree");
    app_->add_option("--tree-depth", state.options.tree_depth, "Limit tree depth (requires --tree)")
        ->check(CLI::PositiveNumber);

    app_->add_option("--git-status", state.git_mode, "Control git status retrieval (auto, always, never)")
        ->type_name("MODE")
        ->default_str("auto");

    app_->add_option("--report", state.options.report_mode,
        "Emit a summary report (values: short, long)")
        ->type_name("WORD");

    app_->add_option("--time-style", state.options.time_style,
        "Control timestamp formatting (default, iso, long-iso, full-iso)")
        ->type_name("STYLE");
    app_->add_option("--size-style", state.options.size_style,
        "Control size formatting (binary, si)")
        ->type_name("STYLE");

    app_->add_flag("--hyperlink", state.options.hyperlink_paths, "Emit hyperlinks for supported terminals");

    app_->add_option("--locale", state.locale, "Override locale (LANG style)");
    verbosity_option_ = app_->add_option("-v,--log-level", state.log_level, "Set log verbosity")
                             ->type_name("LEVEL")
                             ->default_str("error");

    try {
        app_->parse(argc, argv);
    } catch (const CLI::CallForHelp& e) {
        return app_->exit(e);
    } catch (const CLI::ParseError& e) {
        return app_->exit(e);
    }

    if (*version_flag) {
        std::cout << "nicels version 1.0.0" << std::endl;
        return 0;
    }

    if (state.markdown) {
        print_markdown(std::cout);
        return 0;
    }

    logger.set_level(parse_log_level(state.log_level));
    state.options.git_status = parse_git_mode(state.git_mode);

    if (state.options.include_all) {
        state.options.almost_all = false;
    }

    if (!state.options.tree && state.options.tree_depth) {
        throw CLI::ParseError("--tree-depth requires --tree", 1);
    }

    std::vector<ListingPath> paths;
    if (state.raw_paths.empty()) {
        paths.push_back({std::filesystem::current_path(), false});
    } else {
        for (const auto& raw : state.raw_paths) {
            paths.push_back({std::filesystem::path(raw), true});
        }
    }
    config.set_paths(std::move(paths));
    config.set_listing_options(std::move(state.options));
    if (!state.locale.empty()) {
        config.set_locale(state.locale);
    }

    return 0;
}

void Cli::print_markdown(std::ostream& os) const {
    os << "| Option | Description |\n";
    os << "|--------|-------------|\n";
    for (const auto* option : app_->get_options()) {
        if (option->get_lnames().empty() && option->get_snames().empty()) {
            continue;
        }
        if (!option->get_description().empty()) {
            std::string names;
            bool first = true;
            for (const auto& sname : option->get_snames()) {
                if (!first) {
                    names += ", ";
                }
                names += "-" + sname;
                first = false;
            }
            for (const auto& lname : option->get_lnames()) {
                if (!names.empty()) {
                    names += ", ";
                }
                names += "--" + lname;
            }
            if (!names.empty()) {
                os << "| " << names << " | " << option->get_description() << " |\n";
            }
        }
    }
}

} // namespace nicels
