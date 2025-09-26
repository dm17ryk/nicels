#include "nicels/app.hpp"

#include "nicels/cli.hpp"
#include "nicels/config.hpp"
#include "nicels/fs_scanner.hpp"
#include "nicels/git_status.hpp"
#include "nicels/logger.hpp"
#include "nicels/formatter.hpp"
#include "nicels/perf.hpp"
#include "nicels/platform.hpp"
#include "nicels/renderer.hpp"
#include "nicels/theme.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <iostream>

namespace nicels {

App::App() = default;

int App::run(int argc, char** argv) {
    platform::enable_virtual_terminal_processing();

    Cli cli;
    Config& config = Config::instance();
    const int parse_code = cli.parse(argc, argv, config);
    if (parse_code != 0) {
        return parse_code;
    }

    const ListingOptions& options = config.listing();
    GitStatusCache git_cache;
    git_cache.configure(options.git_status != GitStatusMode::Never);

    Theme theme(options.enable_color && platform::stdout_is_tty(), options.enable_icons);

    FileSystemScanner scanner;
    Renderer renderer(options, theme, git_cache);

    const auto& paths = config.paths();
    bool first_path = true;
    for (const auto& listing_path : paths) {
        std::error_code ec;
        auto entry = scanner.stat_path(listing_path.path, false, ec);
        if (ec) {
            std::cerr << "nicels: " << listing_path.path << ": " << ec.message() << '\n';
            continue;
        }

        if (!first_path) {
            std::cout << '\n';
        }
        first_path = false;

        const bool print_header = paths.size() > 1 || listing_path.explicit_path;
        if (print_header && !options.tree) {
            std::cout << listing_path.path.string() << ":\n";
        }

        if (options.tree && entry.is_directory) {
            if (print_header) {
                std::cout << listing_path.path.string() << '\n';
            }
            renderer.render_tree(entry, scanner, options.tree_depth);
            continue;
        }

        if (entry.is_directory && !options.directories_only) {
            auto children = scanner.scan(listing_path.path, false, ec);
            if (ec) {
                std::cerr << "nicels: " << listing_path.path << ": " << ec.message() << '\n';
                continue;
            }
            renderer.render_entries(children);
            if (options.report_mode) {
                auto prepared = renderer.prepare_entries(std::move(children));
                std::size_t file_count = 0;
                std::size_t dir_count = 0;
                uintmax_t total_size = 0;
                for (const auto& child : prepared) {
                    if (child.is_directory) {
                        ++dir_count;
                    } else {
                        ++file_count;
                        total_size += child.size;
                    }
                }
                std::string mode = *options.report_mode;
                std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (mode == "short") {
                    std::cout << std::format("\n{} files, {} dirs\n", file_count, dir_count);
                } else {
                    std::cout << std::format("\n{} files, {} dirs, total size {}\n", file_count, dir_count,
                        formatter::file_size(total_size, options.size_style));
                }
            }
            continue;
        }

        renderer.render_single(entry);
    }

    return 0;
}

} // namespace nicels
