#include "app.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

#include "file_ownership_resolver.h"
#include "git_status.h"
#include "path_processor.h"
#include "perf.h"
#include "platform.h"
#include "resources.h"
#include "symlink_resolver.h"
#include "theme.h"

namespace fs = std::filesystem;

namespace nls {

int App::run(int argc, char** argv) {
    const bool virtual_terminal_enabled = Platform::enableVirtualTerminal();
    ResourceManager::initPaths(argc > 0 ? argv[0] : nullptr);

    config_ = &parser_.Parse(argc, argv);

    if (config_->copy_config_only()) {
        ResourceManager::CopyResult copy_result;
        std::error_code copy_ec = ResourceManager::copyDefaultsToUserConfig(copy_result);
        if (copy_ec) {
            std::cerr << "nls: error: failed to copy configuration files: " << copy_ec.message() << "\n";
            config_ = nullptr;
            return 1;
        }

        if (copy_result.copied.empty() && copy_result.skipped.empty()) {
            std::cout << "nls: no configuration files found to copy\n";
        } else {
            for (const auto& path : copy_result.copied) {
                std::cout << "nls: copied " << path << "\n";
            }
            for (const auto& path : copy_result.skipped) {
                std::cout << "nls: skipped (already exists) " << path << "\n";
            }
        }

        config_ = nullptr;
        return 0;
    }

    perf::Manager& perf_manager = perf::Manager::Instance();
    perf_manager.set_enabled(config_->perf_logging());
    std::optional<perf::Timer> run_timer;
    if (perf_manager.enabled()) {
        run_timer.emplace("app::run");
    }
    if (!virtual_terminal_enabled) {
        config_->set_no_color(true);
    }

    ColorScheme scheme = ColorScheme::Dark;
    switch (options().color_theme()) {
        case Config::ColorTheme::Light:
            scheme = ColorScheme::Light;
            break;
        case Config::ColorTheme::Dark:
        case Config::ColorTheme::Default:
        default:
            scheme = ColorScheme::Dark;
            break;
    }
    Theme::instance().initialize(scheme, options().theme_name());

    scanner_ = std::make_unique<FileScanner>(options(), ownership_resolver_, symlink_resolver_);
    renderer_ = std::make_unique<Renderer>(options());
    PathProcessor processor{options(), *scanner_, *renderer_, git_status_};

    VisitResult rc = VisitResult::Ok;
    for (const auto& path : options().paths()) {
        VisitResult path_result = VisitResult::Ok;
        try {
            path_result = processor.process(fs::path(path));
        } catch (const std::exception& e) {
            std::cerr << "nls: error: " << e.what() << "\n";
            path_result = VisitResult::Serious;
        }
        rc = VisitResultAggregator::Combine(rc, path_result);
    }

    renderer_.reset();
    scanner_.reset();
    config_ = nullptr;

    if (perf_manager.enabled()) {
        run_timer.reset();
        perf_manager.Report(std::cerr);
    }

    return static_cast<int>(rc);
}

}  // namespace nls

