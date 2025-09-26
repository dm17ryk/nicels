#include "nicels/app.hpp"

#include "nicels/cli.hpp"
#include "nicels/config.hpp"
#include "nicels/filesystem_scanner.hpp"
#include "nicels/git_status.hpp"
#include "nicels/logger.hpp"
#include "nicels/perf.hpp"
#include "nicels/platform.hpp"
#include "nicels/renderer.hpp"

#include <exception>
#include <iostream>

namespace nicels {

int App::run(int argc, char* argv[]) {
    try {
        Cli cli;
        Options options = cli.parse(argc, const_cast<const char* const*>(argv));
        if (options.color_policy == ColorPolicy::Auto && !supports_color(ColorPolicy::Auto)) {
            options.colors = false;
        }
        Config::instance().set_options(options);

        FilesystemScanner scanner;
        ScopedTimer timer{"scan", Logger::instance().level() >= LogLevel::Debug};
        auto results = scanner.scan(options);

        GitStatusCache git_cache{options.git_status};
        Renderer renderer{options, git_cache};
        renderer.render(results, std::cout);
        return 0;
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error, ex.what());
        return 1;
    }
}

} // namespace nicels
