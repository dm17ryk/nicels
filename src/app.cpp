#include "nicels/app.hpp"

#include "nicels/fs_scanner.hpp"
#include "nicels/logger.hpp"
#include "nicels/renderer.hpp"

#include <filesystem>
#include <iostream>

namespace nicels {

App::App() = default;

int App::run(int argc, char** argv) {
    Config config = cli_.parse(argc, argv);
    auto& options = config.data();

    Logger::instance().set_level(Logger::Level::Error);

    FileSystemScanner scanner(options);
    Renderer renderer(options, std::cout);

    int exit_code = 0;

    for (const auto& path_text : options.paths) {
        std::filesystem::path path = path_text;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::cerr << "nls: " << path << ": " << ec.message() << '\n';
            exit_code = 1;
            continue;
        }

        if (options.header && std::filesystem::is_directory(path)) {
            std::cout << path << ':' << '\n';
        }

        auto entries = scanner.scan(path);
        renderer.render(path, entries);
    }

    return exit_code;
}

} // namespace nicels
