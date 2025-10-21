#include "app.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

#include "db_command.h"
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

    if (config_->db_action() != Config::DbAction::None) {
        int db_rc = runDatabaseCommand(config_->db_action());
        config_ = nullptr;
        return db_rc;
    }

    perf::Manager& perf_manager = perf::Manager::Instance();
    perf_manager.set_enabled(config_->perf_logging());
    std::optional<perf::Timer> run_timer;
    if (perf_manager.enabled()) {
        run_timer.emplace("app::run");
    }
    if (!virtual_terminal_enabled && config_->color_mode() != Config::ColorMode::Always) {
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

int App::runDatabaseCommand(Config::DbAction action)
{
    DatabaseInspector inspector = DatabaseInspector::CreateFromResourceManager();
    return inspector.Execute(action, options().db_icon_entry(), options().db_alias_entry());
}

}  // namespace nls
