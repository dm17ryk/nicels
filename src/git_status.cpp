#include "git_status.h"

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "perf.h"
#include "theme.h"

namespace fs = std::filesystem;

#if defined(USE_LIBGIT2)
#  if defined(__has_include)
#    if __has_include(<git2.h>)
#      include <git2.h>
#      define NLS_USE_LIBGIT2 1
#    endif
#  else
#    include <git2.h>
#    define NLS_USE_LIBGIT2 1
#  endif
#endif

#ifndef NLS_USE_LIBGIT2
#  define NLS_USE_LIBGIT2 0
#endif

namespace nls {

const std::set<std::string>* GitStatusResult::ModesFor(const std::string& rel_path) const {
    std::string key = rel_path;
    auto slash = key.find('/');
    if (slash != std::string::npos) key = key.substr(0, slash);
    if (key.empty()) {
        return default_modes.empty() ? nullptr : &default_modes;
    }
    auto it = entries.find(key);
    if (it != entries.end()) return &it->second;
    if (!default_modes.empty()) return &default_modes;
    return nullptr;
}

std::string GitStatusResult::FormatPrefixFor(const std::string& rel_path,
                                             bool is_dir,
                                             bool is_empty_dir,
                                             bool no_color) const {
    const std::set<std::string>* modes = ModesFor(rel_path);
    return FormatPrefix(modes, is_dir, is_empty_dir, no_color);
}

std::string GitStatusResult::FormatPrefix(const std::set<std::string>* modes,
                                          bool is_dir,
                                          bool is_empty_dir,
                                          bool no_color) const {
    if (!repository_found) return {};

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

class GitStatusImpl {
public:
    virtual ~GitStatusImpl() = default;
    virtual GitStatusResult GetStatus(const fs::path& dir, bool recursive) = 0;
};

#if NLS_USE_LIBGIT2
namespace {

struct RepositoryDeleter {
    void operator()(git_repository* repo) const noexcept { git_repository_free(repo); }
};

struct StatusListDeleter {
    void operator()(git_status_list* list) const noexcept { git_status_list_free(list); }
};

class LibGit2StatusImpl : public GitStatusImpl {
public:
    LibGit2StatusImpl() { git_libgit2_init(); }

    ~LibGit2StatusImpl() override {
        cached_repo_.reset();
        git_libgit2_shutdown();
    }

    GitStatusResult GetStatus(const fs::path& dir, bool recursive) override {
        GitStatusResult result;
        Repository* repository = EnsureRepository(dir);
        if (!repository) {
            return result;
        }

        result.repository_found = true;

        fs::path base_dir = DetermineBaseDir(dir);
        fs::path dir_abs = Canonicalize(base_dir);
        if (dir_abs.empty()) dir_abs = base_dir;

        git_status_options options = GIT_STATUS_OPTIONS_INIT;
        options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
        options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                        GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                        GIT_STATUS_OPT_INCLUDE_IGNORED;

        std::string dir_string = dir_abs.generic_string();
        const bool dir_is_repo_root = dir_string == repository->root_generic;

        std::string rel_dir_str;
        char* pathspec_storage[1] = {nullptr};
        if (IsWithin(repository->root_generic, dir_string)) {
            std::error_code ec;
            fs::path rel_dir = fs::relative(dir_abs, repository->root, ec);
            if (!ec) {
                rel_dir_str = rel_dir.generic_string();
                if (rel_dir_str.empty() || rel_dir_str == ".") {
                    rel_dir_str.clear();
                }
            } else {
                ec.clear();
            }

            if (!rel_dir_str.empty()) {
                pathspec_storage[0] = const_cast<char*>(rel_dir_str.c_str());
                options.pathspec.strings = pathspec_storage;
                options.pathspec.count = 1;

                int ignored = 0;
                if (git_ignore_path_is_ignored(&ignored,
                                               repository->handle.get(),
                                               rel_dir_str.c_str()) == 0 &&
                    ignored) {
                    result.default_modes.insert("!!");
                }
            }
        }

        if (!recursive) {
            // For very small directories it is faster to query paths individually
            // instead of constructing a full status list. Ten entries keeps the
            // break-even point favourable for the single-directory listing path.
            constexpr std::size_t kSmallDirectoryThreshold = 10;
            std::error_code is_dir_ec;
            if (fs::is_directory(dir_abs, is_dir_ec) && !is_dir_ec) {
                std::size_t entry_count = 0;
                std::error_code iter_ec;
                for (fs::directory_iterator it(dir_abs, iter_ec), end; !iter_ec && it != end;
                     it.increment(iter_ec)) {
                    ++entry_count;
                    if (entry_count >= kSmallDirectoryThreshold) break;
                }
                if (!iter_ec && entry_count < kSmallDirectoryThreshold) {
                    if (ApplySmallDirectoryFastPath(*repository,
                                                    dir_abs,
                                                    dir_string,
                                                    dir_is_repo_root,
                                                    result,
                                                    options.flags,
                                                    options.show)) {
                        return result;
                    }
                }
            }
        }

        git_status_list* raw_list = nullptr;
        if (git_status_list_new(&raw_list, repository->handle.get(), &options) != 0) {
            return result;
        }

        StatusListHandle list(raw_list);
        const size_t count = git_status_list_entrycount(list.get());

        for (size_t i = 0; i < count; ++i) {
            const git_status_entry* entry = git_status_byindex(list.get(), i);
            if (!entry) continue;

            std::string relative_from_repo = ExtractPathFromEntry(*entry);
            if (relative_from_repo.empty()) continue;

            ProcessStatus(result,
                          entry->status,
                          relative_from_repo,
                          repository->root_generic,
                          dir_string,
                          dir_is_repo_root);
        }

        return result;
    }

private:
    struct Repository {
        using RepositoryHandle = std::unique_ptr<git_repository, RepositoryDeleter>;
        RepositoryHandle handle;
        fs::path root;
        std::string root_generic;
    };

    struct StatusForeachPayload {
        const LibGit2StatusImpl* self;
        GitStatusResult* result;
        const std::string* repo_root_generic;
        const std::string* dir_string;
        bool dir_is_repo_root;
    };

    using RepositoryHandle = typename Repository::RepositoryHandle;
    using StatusListHandle = std::unique_ptr<git_status_list, StatusListDeleter>;

    static std::string ExtractPathFromEntry(const git_status_entry& entry) {
        auto extract_path = [](const git_diff_delta* delta) -> std::string {
            if (!delta) return {};
            if (delta->new_file.path && delta->new_file.path[0]) {
                return delta->new_file.path;
            }
            if (delta->old_file.path && delta->old_file.path[0]) {
                return delta->old_file.path;
            }
            return {};
        };

        std::string relative = extract_path(entry.head_to_index);
        if (relative.empty()) {
            relative = extract_path(entry.index_to_workdir);
        }
        return relative;
    }

    static fs::path Canonicalize(const fs::path& input) {
        std::error_code ec;
        fs::path canonical = fs::weakly_canonical(input, ec);
        if (ec) canonical = fs::absolute(input, ec);
        if (ec) return {};
        return canonical;
    }

    static fs::path DetermineBaseDir(const fs::path& path) {
        std::error_code ec;
        if (fs::is_directory(path, ec) && !ec) {
            return path;
        }
        return path.parent_path();
    }

    void ProcessStatus(GitStatusResult& result,
                       unsigned status,
                       const std::string& relative_from_repo,
                       const std::string& repo_root_generic,
                       const std::string& dir_string,
                       bool dir_is_repo_root) const {
        const std::string code = ToPorcelainCode(status);
        if (code.empty()) return;

        std::string abs_string = repo_root_generic;
        if (!abs_string.empty() && abs_string.back() != '/' && !relative_from_repo.empty()) {
            abs_string.push_back('/');
        }
        abs_string += relative_from_repo;

        const std::string normalized_abs = fs::path(abs_string).lexically_normal().generic_string();
        if (!IsWithin(dir_string, normalized_abs)) {
            if (IsWithin(normalized_abs, dir_string)) {
                result.default_modes.insert(code);
            }
            return;
        }

        std::string relative;
        if (!dir_is_repo_root) {
            relative = normalized_abs.substr(dir_string.size());
            if (!relative.empty() && relative.front() == '/') {
                relative.erase(0, 1);
            }
        } else {
            relative = relative_from_repo;
        }

        if (!relative.empty()) {
            relative = fs::path(relative).lexically_normal().generic_string();
        }

        if (relative.empty() || relative == ".") {
            result.default_modes.insert(code);
            return;
        }

        if (!relative.empty() && relative.back() == '/') {
            relative.pop_back();
        }

        auto slash = relative.find('/');
        std::string key = (slash == std::string::npos) ? relative : relative.substr(0, slash);
        if (!key.empty()) {
            result.entries[key].insert(code);
        }
    }

    bool ApplySmallDirectoryFastPath(Repository& repository,
                                     const fs::path& dir_abs,
                                     const std::string& dir_string,
                                     bool dir_is_repo_root,
                                     GitStatusResult& result,
                                     unsigned int flags,
                                     git_status_show_t show) const {
        std::error_code iter_ec;
        fs::directory_iterator it(dir_abs, iter_ec);
        if (iter_ec) {
            return false;
        }

        fs::directory_iterator end;
        for (; it != end; it.increment(iter_ec)) {
            if (iter_ec) break;

            const fs::path& entry_path = it->path();
            std::string entry_name = entry_path.filename().generic_string();
            if (entry_name.empty() || entry_name == "." || entry_name == "..") continue;

            std::error_code rel_ec;
            fs::path rel_path = fs::relative(entry_path, repository.root, rel_ec);
            if (rel_ec) {
                rel_ec.clear();
                continue;
            }
            std::string entry_relative_repo = rel_path.generic_string();
            if (entry_relative_repo.empty() || entry_relative_repo == ".") continue;
            if (entry_relative_repo.size() >= 2 && entry_relative_repo[0] == '.' &&
                entry_relative_repo[1] == '.') {
                continue;
            }

            std::error_code type_ec;
            const bool is_dir = it->is_directory(type_ec) && !type_ec;

            if (!is_dir) {
                unsigned status_bits = 0;
                int rc = git_status_file(&status_bits,
                                         repository.handle.get(),
                                         entry_relative_repo.c_str());
                if (rc == 0) {
                    ProcessStatus(result,
                                  status_bits,
                                  entry_relative_repo,
                                  repository.root_generic,
                                  dir_string,
                                  dir_is_repo_root);
                } else {
                    MaybeMarkIgnored(repository, entry_relative_repo, entry_name, result);
                }
                continue;
            }

            int ignored = 0;
            if (git_ignore_path_is_ignored(&ignored,
                                           repository.handle.get(),
                                           entry_relative_repo.c_str()) == 0 &&
                ignored) {
                result.entries[entry_name].insert("!!");
                continue;
            }

            if (!CollectStatusesForPathspec(repository,
                                            entry_relative_repo,
                                            dir_string,
                                            dir_is_repo_root,
                                            flags,
                                            show,
                                            result)) {
                return false;
            }
        }

        if (iter_ec) {
            return false;
        }

        return true;
    }

    bool CollectStatusesForPathspec(Repository& repository,
                                    const std::string& relative_path,
                                    const std::string& dir_string,
                                    bool dir_is_repo_root,
                                    unsigned int flags,
                                    git_status_show_t show,
                                    GitStatusResult& result) const {
        if (relative_path.empty()) return true;

        git_status_options options = GIT_STATUS_OPTIONS_INIT;
        options.show = show;
        options.flags = flags;

        char* pathspec_strings[1];
        pathspec_strings[0] = const_cast<char*>(relative_path.c_str());
        options.pathspec.strings = pathspec_strings;
        options.pathspec.count = 1;

        StatusForeachPayload payload{this,
                                     &result,
                                     &repository.root_generic,
                                     &dir_string,
                                     dir_is_repo_root};

        int rc = git_status_foreach_ext(repository.handle.get(),
                                        &options,
                                        &LibGit2StatusImpl::StatusForeachCallback,
                                        &payload);
        return rc == 0;
    }

    void MaybeMarkIgnored(Repository& repository,
                          const std::string& relative_path,
                          const std::string& entry_name,
                          GitStatusResult& result) const {
        if (relative_path.empty()) return;
        int ignored = 0;
        if (git_ignore_path_is_ignored(&ignored,
                                       repository.handle.get(),
                                       relative_path.c_str()) == 0 &&
            ignored) {
            result.entries[entry_name].insert("!!");
        }
    }

    static std::string ToPorcelainCode(unsigned status) {
        if (status & GIT_STATUS_CONFLICTED) {
            return "UU";
        }
        if (status & GIT_STATUS_IGNORED) {
            return "!!";
        }

        char index_state = ' ';
        if      (status & GIT_STATUS_INDEX_NEW)        index_state = 'A';
        else if (status & GIT_STATUS_INDEX_MODIFIED)   index_state = 'M';
        else if (status & GIT_STATUS_INDEX_DELETED)    index_state = 'D';
        else if (status & GIT_STATUS_INDEX_RENAMED)    index_state = 'R';
        else if (status & GIT_STATUS_INDEX_TYPECHANGE) index_state = 'T';

        char worktree_state = ' ';
        if      (status & GIT_STATUS_WT_NEW)        worktree_state = '?';
        else if (status & GIT_STATUS_WT_MODIFIED)   worktree_state = 'M';
        else if (status & GIT_STATUS_WT_DELETED)    worktree_state = 'D';
        else if (status & GIT_STATUS_WT_RENAMED)    worktree_state = 'R';
        else if (status & GIT_STATUS_WT_TYPECHANGE) worktree_state = 'T';
        else if (status & GIT_STATUS_WT_UNREADABLE) worktree_state = '!';

        if (index_state == ' ' && worktree_state == '?') {
            return "??";
        }

        return std::string() + index_state + worktree_state;
    }

    static int StatusForeachCallback(const char* path, unsigned int status_flags, void* payload) {
        if (!payload || !path) return 0;
        auto* data = static_cast<StatusForeachPayload*>(payload);
        data->self->ProcessStatus(*data->result,
                                  status_flags,
                                  path,
                                  *data->repo_root_generic,
                                  *data->dir_string,
                                  data->dir_is_repo_root);
        return 0;
    }

    static bool IsWithin(std::string_view root, std::string_view candidate) {
        if (root.empty()) return false;
        if (candidate.size() < root.size()) return false;
        if (candidate.substr(0, root.size()) != root) return false;
        if (candidate.size() == root.size()) return true;
        return candidate[root.size()] == '/';
    }

    Repository* EnsureRepository(const fs::path& dir) {
        fs::path base_dir = DetermineBaseDir(dir);
        fs::path dir_abs = Canonicalize(base_dir);
        const std::string dir_string = dir_abs.generic_string();

        if (cached_repo_ && IsWithin(cached_repo_->root_generic, dir_string)) {
            return cached_repo_.get();
        }

        fs::path repo_root;
        RepositoryHandle handle = OpenRepository(base_dir, repo_root);
        if (!handle) {
            cached_repo_.reset();
            return nullptr;
        }

        auto repo = std::make_unique<Repository>();
        repo->root = repo_root;
        repo->root_generic = repo_root.generic_string();
        repo->handle = std::move(handle);
        cached_repo_ = std::move(repo);
        return cached_repo_.get();
    }

    RepositoryHandle OpenRepository(const fs::path& dir, fs::path& repo_root) {
        git_repository* raw_repo = nullptr;
        fs::path search = DetermineBaseDir(dir);
        int rc = git_repository_open_ext(&raw_repo,
                                         search.string().c_str(),
                                         GIT_REPOSITORY_OPEN_CROSS_FS | GIT_REPOSITORY_OPEN_FROM_ENV,
                                         nullptr);
        if (rc != 0) {
            return {};
        }

        RepositoryHandle handle(raw_repo);
        const char* workdir = git_repository_workdir(raw_repo);
        std::error_code ec;
        fs::path root = workdir ? fs::path(workdir) : fs::current_path(ec);
        if (ec) {
            ec.clear();
            root = fs::path();
        }
        if (root.empty()) {
            root = DetermineBaseDir(dir);
        }
        repo_root = Canonicalize(root);
        if (repo_root.empty()) repo_root = root;
        return handle;
    }

    std::unique_ptr<Repository> cached_repo_;
};

} // namespace

#else

namespace {

class NoopStatusImpl : public GitStatusImpl {
public:
    GitStatusResult GetStatus(const fs::path& /*dir*/, bool /*recursive*/) override { return {}; }
};

} // namespace

#endif

GitStatus::GitStatus()
#if NLS_USE_LIBGIT2
    : impl_(std::make_unique<LibGit2StatusImpl>())
#else
    : impl_(std::make_unique<NoopStatusImpl>())
#endif
{}

GitStatus::~GitStatus() = default;

GitStatusResult GitStatus::GetStatus(const fs::path& dir, bool recursive) {
    auto& perf_manager = perf::Manager::Instance();
    if (!perf_manager.enabled()) {
        return impl_->GetStatus(dir, recursive);
    }

    perf::Timer timer("git_status_impl");
    return impl_->GetStatus(dir, recursive);
}

} // namespace nls
