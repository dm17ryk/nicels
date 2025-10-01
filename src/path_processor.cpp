#include "path_processor.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "perf.h"
#include "string_utils.h"

namespace fs = std::filesystem;

namespace nls {

PathProcessor::PathProcessor(const Config& config,
                             FileScanner& scanner,
                             Renderer& renderer,
                             GitStatus& git_status) noexcept
    : config_(config), scanner_(scanner), renderer_(renderer), git_status_(git_status) {}

VisitResult PathProcessor::process(const fs::path& path) {
    return listPath(path);
}

VisitResult PathProcessor::listPath(const fs::path& path) {
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
            status = VisitResultAggregator::Combine(status, tree_status);
            if (tree_status == VisitResult::Serious) {
                return status;
            }
            renderer().RenderTree(nodes, flat);
        } else {
            std::vector<Entry> single;
            VisitResult collect_status = scanner().collect_entries(path, single, true);
            status = VisitResultAggregator::Combine(status, collect_status);
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
    status = VisitResultAggregator::Combine(status, collect_status);
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

std::vector<TreeItem> PathProcessor::buildTreeItems(const fs::path& dir,
                                                    std::size_t depth,
                                                    std::vector<Entry>& flat,
                                                    VisitResult& status) {
    std::vector<TreeItem> nodes;
    std::vector<Entry> items;
    VisitResult local = scanner().collect_entries(dir, items, depth == 0);
    status = VisitResultAggregator::Combine(status, local);
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

        const bool is_dir = node.entry.info.is_dir && !node.entry.info.is_symlink;
        const bool is_self = node.entry.info.name == "." || node.entry.info.name == "..";
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

void PathProcessor::applyGitStatus(std::vector<Entry>& items, const fs::path& dir) {
    if (!options().git_status()) return;

    GitStatusResult status;
    auto& perf_manager = perf::Manager::Instance();
    {
        std::optional<perf::Timer> timer;
        if (perf_manager.enabled()) {
            timer.emplace("git_status::GetStatus");
        }
        status = gitStatus().GetStatus(dir, options().tree());
    }
    if (perf_manager.enabled()) {
        perf_manager.IncrementCounter("git_status_requests");
        if (status.repository_found) {
            perf_manager.IncrementCounter("git_repositories_found");
        }
    }
    for (auto& entry : items) {
        std::error_code ec;
        const fs::path base = fs::is_directory(dir) ? dir : dir.parent_path();
        const fs::path relp = fs::relative(entry.info.path, base, ec);
        const std::string rel = ec ? entry.info.path.filename().generic_string()
                                   : relp.generic_string();

        bool is_empty_dir = false;
        if (entry.info.is_dir) {
            is_empty_dir = fs::is_empty(entry.info.path, ec);
            if (ec) is_empty_dir = false;
        }

        entry.info.git_prefix = status.FormatPrefixFor(
            rel, entry.info.is_dir, is_empty_dir, options().no_color());
    }
}

void PathProcessor::sortEntries(std::vector<Entry>& entries) const {
    using std::ranges::reverse;
    using std::ranges::stable_sort;

    const auto cmp_name = [](const Entry& a, const Entry& b) {
        return StringUtils::ToLower(a.info.name) < StringUtils::ToLower(b.info.name);
    };
    const auto cmp_time = [](const Entry& a, const Entry& b) {
        return a.info.mtime > b.info.mtime;
    };
    const auto cmp_size = [](const Entry& a, const Entry& b) {
        return a.info.size > b.info.size;
    };
    const auto cmp_ext = [](const Entry& a, const Entry& b) {
        const auto pa = a.info.path.extension().string();
        const auto pb = b.info.path.extension().string();
        return StringUtils::ToLower(pa) < StringUtils::ToLower(pb);
    };

    switch (options().sort()) {
        case Config::Sort::Time:
            stable_sort(entries, cmp_time);
            break;
        case Config::Sort::Size:
            stable_sort(entries, cmp_size);
            break;
        case Config::Sort::Extension:
            stable_sort(entries, cmp_ext);
            break;
        case Config::Sort::None:
            break;
        case Config::Sort::Name:
        default:
            stable_sort(entries, cmp_name);
            break;
    }

    if (options().reverse()) reverse(entries);

    if (options().group_dirs_first()) {
        stable_sort(entries, [](const Entry& a, const Entry& b) {
            return a.info.is_dir && !b.info.is_dir;
        });
    }
    if (options().sort_files_first()) {
        stable_sort(entries, [](const Entry& a, const Entry& b) {
            return !a.info.is_dir && b.info.is_dir;
        });
    }
    if (options().dots_first()) {
        stable_sort(entries, [](const Entry& a, const Entry& b) {
            const bool da = StringUtils::IsHidden(a.info.name);
            const bool db = StringUtils::IsHidden(b.info.name);
            return da && !db;
        });
    }
}

}  // namespace nls

