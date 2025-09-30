#include "app.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "file_ownership_resolver.h"
#include "git_status.h"
#include "platform.h"
#include "resources.h"
#include "string_utils.h"
#include "symlink_resolver.h"
#include "theme.h"

namespace fs = std::filesystem;

namespace nls {
namespace {

SymlinkResolver& GetSymlinkResolver() {
    static SymlinkResolver resolver;
    return resolver;
}

FileOwnershipResolver& GetFileOwnershipResolver() {
    static FileOwnershipResolver resolver;
    return resolver;
}

GitStatus& GetGitStatus() {
    static GitStatus status;
    return status;
}

const std::set<std::string>* StatusModesFor(const GitStatusResult& status,
                                            const std::string& rel) {
    std::string key = rel;
    auto slash = key.find('/');
    if (slash != std::string::npos) key = key.substr(0, slash);
    if (key.empty()) {
        return status.default_modes.empty() ? nullptr : &status.default_modes;
    }
    auto it = status.entries.find(key);
    if (it != status.entries.end()) return &it->second;
    return nullptr;
}

std::string FormatGitPrefix(bool has_repo,
                            const std::set<std::string>* modes,
                            bool is_dir,
                            bool is_empty_dir,
                            bool no_color) {
    if (!has_repo) return {};
    bool saw_code = false;
    bool saw_visible = false;
    std::set<char> glyphs;
    Theme& theme_manager = Theme::instance();
    const ThemeColors& theme = theme_manager.colors();
    std::string col_add = theme.color_or("addition", "\x1b[32m");
    std::string col_mod = theme.color_or("modification", "\x1b[33m");
    std::string col_del = theme.color_or("deletion", "\x1b[31m");
    std::string col_untracked = theme.color_or("untracked", "\x1b[35m");
    std::string col_clean = theme.color_or("unchanged", "\x1b[32m");
    std::string col_conflict = theme.color_or("error", "\x1b[31m");

    if (modes) {
        for (const auto& code : *modes) {
            if (!code.empty()) saw_code = true;
            for (char ch : code) {
                if (ch == ' ' || ch == '!') continue;
                saw_visible = true;
                glyphs.insert(ch);
            }
        }
    }

    if (!saw_code) {
        if (!has_repo) {
            return {};
        }
        if (is_dir && is_empty_dir) return std::string(4, ' ');
        std::string clean = "  \xe2\x9c\x93 ";
        if (no_color || col_clean.empty()) return clean;
        return col_clean + clean + theme.reset;
    }

    if (!saw_visible) {
        return std::string(4, ' ');
    }

    std::string symbols;
    for (char ch : glyphs) symbols.push_back(ch);
    if (symbols.size() < 3) symbols.insert(symbols.begin(), 3 - symbols.size(), ' ');
    symbols.push_back(' ');

    if (no_color) return symbols;

    std::string out;
    out.reserve(symbols.size() + 16);
    for (char ch : symbols) {
        if (ch == ' ') {
            out.push_back(' ');
            continue;
        }
        const std::string* col = nullptr;
        switch (ch) {
            case '?':
                col = &col_untracked;
                break;
            case 'A':
                col = &col_add;
                break;
            case 'M':
                col = &col_mod;
                break;
            case 'D':
                col = &col_del;
                break;
            case 'R':
            case 'T':
                col = &col_mod;
                break;
            case 'U':
                col = &col_conflict;
                break;
            default:
                break;
        }
        if (col) {
            if (!col->empty()) {
                out += *col;
                out.push_back(ch);
                out += theme.reset;
            } else {
                out.push_back(ch);
            }
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace

int App::run(int argc, char** argv) {
    const bool virtual_terminal_enabled = Platform::enableVirtualTerminal();
    init_resource_paths(argc > 0 ? argv[0] : nullptr);

    config_ = &parser_.Parse(argc, argv);
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

    scanner_ = std::make_unique<FileScanner>(options(), GetFileOwnershipResolver(), GetSymlinkResolver());
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

    auto status = GetGitStatus().GetStatus(dir);
    for (auto& entry : items) {
        std::error_code ec;
        fs::path base = fs::is_directory(dir) ? dir : dir.parent_path();
        fs::path relp = fs::relative(entry.info.path, base, ec);
        std::string rel = ec ? entry.info.path.filename().generic_string() : relp.generic_string();

        const std::set<std::string>* modes = StatusModesFor(status, rel);
        bool is_empty_dir = false;
        if (entry.info.is_dir) {
            is_empty_dir = fs::is_empty(entry.info.path, ec);
            if (ec) is_empty_dir = false;
        }

        entry.info.git_prefix =
            FormatGitPrefix(status.repository_found, modes, entry.info.is_dir, is_empty_dir, options().no_color());
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

