#include "nicels/app.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "nicels/cli.h"
#include "nicels/config.h"
#include "nicels/fs.h"
#include "nicels/git_status.h"
#include "nicels/perf.h"
#include "nicels/platform.h"
#include "nicels/renderer.h"

namespace nicels {

class App::Impl {
public:
    int run(int argc, char** argv) {
        Cli cli;
        auto options = cli.parse(argc, argv);
        Config::instance().set_program_name(argc > 0 && argv ? argv[0] : "nicels");
        if (!platform::stdout_supports_color()) {
            options.color_enabled = false;
        }
        Config::instance().set_options(options);

        if (options.dump_markdown) {
            std::cout << cli.usage_markdown();
            return 0;
        }

        FileSystemScanner scanner{options};
        GitStatusCache git_cache{options.show_git_status};
        std::vector<std::filesystem::path> targets = options.paths;
        if (targets.empty()) {
            targets.emplace_back(".");
        }

        Renderer renderer{options, std::cout};
        bool multiple_targets = targets.size() > 1;

        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto& target = targets[index];
            perf::ScopedTimer timer{std::string{"scan:"} + target.string()};
            auto entries = scanner.collect({target});

            if (options.show_git_status) {
                for (auto& entry : entries) {
                    entry.git_status = git_cache.status_for(entry.path);
                }
            }

            std::error_code ec;
            bool is_dir = std::filesystem::is_directory(target, ec);
            if (multiple_targets && is_dir) {
                if (index > 0) {
                    std::cout << '\n';
                }
                std::cout << target.string() << ":" << '\n';
            }

            renderer.render(std::move(entries));
        }

        return 0;
    }
};

App::App()
    : impl_{std::make_unique<Impl>()} {}

App::~App() = default;

int App::run(int argc, char** argv) {
    return impl_->run(argc, argv);
}

} // namespace nicels
