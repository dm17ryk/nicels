#include "git_status.h"

#include <memory>
#include <system_error>
#include <utility>

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

class GitStatusImpl {
public:
    virtual ~GitStatusImpl() = default;
    virtual GitStatusResult GetStatus(const fs::path& dir) = 0;
};

#if NLS_USE_LIBGIT2
namespace {

struct RepositoryDeleter {
    void operator()(git_repository* repo) const noexcept { git_repository_free(repo); }
};

struct StatusListDeleter {
    void operator()(git_status_list* list) const noexcept { git_status_list_free(list); }
};

using RepositoryHandle = std::unique_ptr<git_repository, RepositoryDeleter>;
using StatusListHandle = std::unique_ptr<git_status_list, StatusListDeleter>;

fs::path Canonicalize(const fs::path& input) {
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(input, ec);
    if (ec) canonical = fs::absolute(input, ec);
    if (ec) return {};
    return canonical;
}

fs::path DetermineBaseDir(const fs::path& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec) && !ec) {
        return path;
    }
    return path.parent_path();
}

std::string ToPorcelainCode(unsigned status) {
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

bool IsWithin(const std::string& root, const std::string& candidate) {
    if (root.empty()) return false;
    if (candidate.size() < root.size()) return false;
    if (candidate.compare(0, root.size(), root) != 0) return false;
    if (candidate.size() == root.size()) return true;
    return candidate[root.size()] == '/';
}

class LibGit2StatusImpl : public GitStatusImpl {
public:
    LibGit2StatusImpl() { git_libgit2_init(); }

    ~LibGit2StatusImpl() override {
        cached_repo_.reset();
        git_libgit2_shutdown();
    }

    GitStatusResult GetStatus(const fs::path& dir) override {
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
        options.flags =
            GIT_STATUS_OPT_INCLUDE_UNTRACKED |
            GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
            GIT_STATUS_OPT_INCLUDE_IGNORED |
            GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
            GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

        git_status_list* raw_list = nullptr;
        if (git_status_list_new(&raw_list, repository->handle.get(), &options) != 0) {
            return result;
        }

        StatusListHandle list(raw_list);
        const size_t count = git_status_list_entrycount(list.get());
        std::error_code ec;
        for (size_t i = 0; i < count; ++i) {
            const git_status_entry* entry = git_status_byindex(list.get(), i);
            if (!entry) continue;

            const unsigned status = entry->status;
            const std::string code = ToPorcelainCode(status);
            if (code.empty()) continue;

            std::string relative_from_repo;
            if (entry->head_to_index && entry->head_to_index->new_file.path) {
                relative_from_repo = entry->head_to_index->new_file.path;
            } else if (entry->index_to_workdir && entry->index_to_workdir->new_file.path) {
                relative_from_repo = entry->index_to_workdir->new_file.path;
            }

            if (relative_from_repo.empty()) continue;

            fs::path absolute = repository->root / relative_from_repo;
            absolute = fs::weakly_canonical(absolute, ec);
            if (ec) {
                ec.clear();
                absolute = fs::absolute(absolute, ec);
            }
            if (ec) {
                ec.clear();
                continue;
            }

            const std::string abs_string = absolute.generic_string();
            const std::string dir_string = dir_abs.generic_string();
            if (!IsWithin(dir_string, abs_string)) {
                continue;
            }

            fs::path relative_to_dir = fs::relative(absolute, dir_abs, ec);
            if (ec) {
                ec.clear();
                continue;
            }

            std::string relative = relative_to_dir.generic_string();
            if (relative.empty() || relative == ".") {
                result.default_modes.insert(code);
                continue;
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

        return result;
    }

private:
    struct Repository {
        RepositoryHandle handle;
        fs::path root;
        std::string root_generic;
    };

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
    GitStatusResult GetStatus(const fs::path& /*dir*/) override { return {}; }
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

GitStatusResult GitStatus::GetStatus(const fs::path& dir) {
    return impl_->GetStatus(dir);
}

} // namespace nls
