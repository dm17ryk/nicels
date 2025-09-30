#include "app.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "file_ownership_resolver.h"
#include "git_status.h"
#include "perf.h"
#include "platform.h"
#include "resources.h"
#include "string_utils.h"
#include "symlink_resolver.h"
#include "theme.h"

namespace fs = std::filesystem;

namespace nls {

int App::run(int argc, char** argv) {
    const bool virtual_terminal_enabled = Platform::enableVirtualTerminal();
    ResourceManager::initPaths(argc > 0 ? argv[0] : nullptr);

    config_ = &parser_.Parse(argc, argv);
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
    Theme::instance().initialize(scheme);

    scanner_ = std::make_unique<FileScanner>(options(), ownership_resolver_, symlink_resolver_);
    renderer_ = std::make_unique<Renderer>(options());

    VisitResult rc = VisitResult::Ok;
    for (const auto& path : options().paths()) {
        VisitResult path_result = VisitResult::Ok;
        try {
            path_result = processPath(fs::path(path));
        } catch (const std::exception& e) {
            std::cerr << "nls: error: " << e.what() << "\n";
            path_result = VisitResult::Serious;
        }
        rc = combine_visit_result(rc, path_result);
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

VisitResult App::processPath(const fs::path& path) {
    return listPath(path);
}

VisitResult App::listPath(const fs::path& path) {
    std::error_code dir_ec;
    const bool is_directory = fs::is_directory(path, dir_ec);
    (void)dir_ec;

    VisitResult status = VisitResult::Ok;

    if (options().tree()) {
        std::vector<Entry> flat;
        if (is_directory) {
            if (options().paths().size() > 1) {
                renderer().PrintPathHeader(path);
            }
            VisitResult tree_status = VisitResult::Ok;
            auto nodes = buildTreeItems(path, 0, flat, tree_status);
            status = combine_visit_result(status, tree_status);
            if (tree_status == VisitResult::Serious) {
                return status;
            }
            renderer().RenderTree(nodes, flat);
        } else {
            std::vector<Entry> single;
            VisitResult collect_status = scanner().collect_entries(path, single, true);
            status = combine_visit_result(status, collect_status);
            if (collect_status == VisitResult::Serious) {
                return status;
            }
            applyGitStatus(single, is_directory ? path : path.parent_path());
            sortEntries(single);
            flat = single;
            renderer().RenderEntries(single);
        }

        renderer().RenderReport(flat);
        if (options().paths().size() > 1) renderer().TerminateLine();
        return status;
    }

    std::vector<Entry> items;
    VisitResult collect_status = scanner().collect_entries(path, items, true);
    status = combine_visit_result(status, collect_status);
    if (collect_status == VisitResult::Serious) {
        return status;
    }
    applyGitStatus(items, is_directory ? path : path.parent_path());
    sortEntries(items);

    if (options().header() && options().format() == Config::Format::Long) {
        renderer().PrintDirectoryHeader(path, is_directory);
    } else if (options().paths().size() > 1 && is_directory) {
        renderer().PrintPathHeader(path);
    }

    renderer().RenderEntries(items);
    renderer().RenderReport(items);
    if (options().paths().size() > 1) renderer().TerminateLine();
    return status;
}

std::vector<TreeItem> App::buildTreeItems(const fs::path& dir,
                                          std::size_t depth,
                                          std::vector<Entry>& flat,
                                          VisitResult& status) {
    std::vector<TreeItem> nodes;
    std::vector<Entry> items;
    VisitResult local = scanner().collect_entries(dir, items, depth == 0);
    status = combine_visit_result(status, local);
    if (local == VisitResult::Serious) {
        return nodes;
    }
    applyGitStatus(items, dir);
    sortEntries(items);

    nodes.reserve(items.size());
    for (const auto& item : items) {
        TreeItem node;
        node.entry = item;
        flat.push_back(item);

        bool is_dir = node.entry.info.is_dir && !node.entry.info.is_symlink;
        bool is_self = (node.entry.info.name == "." || node.entry.info.name == "..");
        bool within_limit = true;
        if (options().tree_depth().has_value()) {
            within_limit = depth + 1 < *options().tree_depth();
        }
        if (is_dir && within_limit && !is_self) {
            node.children = buildTreeItems(node.entry.info.path, depth + 1, flat, status);
        }

        nodes.push_back(std::move(node));
    }

    return nodes;
}

void App::applyGitStatus(std::vector<Entry>& items, const fs::path& dir) {
    if (!options().git_status()) return;

    GitStatusResult status;
    auto& perf_manager = perf::Manager::Instance();
    {
        std::optional<perf::Timer> timer;
        if (perf_manager.enabled()) {
            timer.emplace("git_status::GetStatus");
        }
        status = git_status_.GetStatus(dir);
    }
    if (perf_manager.enabled()) {
        perf_manager.IncrementCounter("git_status_requests");
        if (status.repository_found) {
            perf_manager.IncrementCounter("git_repositories_found");
        }
    }
    for (auto& entry : items) {
        std::error_code ec;
        fs::path base = fs::is_directory(dir) ? dir : dir.parent_path();
        fs::path relp = fs::relative(entry.info.path, base, ec);
        std::string rel = ec ? entry.info.path.filename().generic_string() : relp.generic_string();

        bool is_empty_dir = false;
        if (entry.info.is_dir) {
            is_empty_dir = fs::is_empty(entry.info.path, ec);
            if (ec) is_empty_dir = false;
        }

        entry.info.git_prefix =
            status.FormatPrefixFor(rel, entry.info.is_dir, is_empty_dir, options().no_color());
    }
}

void App::sortEntries(std::vector<Entry>& entries) {
    auto cmp_name = [](const Entry& a, const Entry& b) {
        return StringUtils::ToLower(a.info.name) < StringUtils::ToLower(b.info.name);
    };
    auto cmp_time = [](const Entry& a, const Entry& b) {
        return a.info.mtime > b.info.mtime;
    };
    auto cmp_size = [](const Entry& a, const Entry& b) {
        return a.info.size > b.info.size;
    };
    auto cmp_ext = [](const Entry& a, const Entry& b) {
        auto pa = a.info.path.extension().string();
        auto pb = b.info.path.extension().string();
        return StringUtils::ToLower(pa) < StringUtils::ToLower(pb);
    };

    switch (options().sort()) {
        case Config::Sort::Time:
            std::stable_sort(entries.begin(), entries.end(), cmp_time);
            break;
        case Config::Sort::Size:
            std::stable_sort(entries.begin(), entries.end(), cmp_size);
            break;
        case Config::Sort::Extension:
            std::stable_sort(entries.begin(), entries.end(), cmp_ext);
            break;
        case Config::Sort::None:
            break;
        case Config::Sort::Name:
        default:
            std::stable_sort(entries.begin(), entries.end(), cmp_name);
            break;
    }
    if (options().reverse()) std::reverse(entries.begin(), entries.end());

    if (options().group_dirs_first()) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.info.is_dir && !b.info.is_dir;
        });
    }
    if (options().sort_files_first()) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return !a.info.is_dir && b.info.is_dir;
        });
    }
    if (options().dots_first()) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            bool da = StringUtils::IsHidden(a.info.name);
            bool db = StringUtils::IsHidden(b.info.name);
            return da && !db;
        });
    }
}

}  // namespace nls

