#include <system_error>

#include "git_status.h"

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

#if NLS_USE_LIBGIT2

// Open a repository by searching upward from 'p'
static bool repo_open_for_path(const fs::path& p, git_repository** out) {
    git_libgit2_init();
    fs::path dir = fs::is_directory(p) ? p : p.parent_path();
    int rc = git_repository_open_ext(out, dir.string().c_str(),
                                     GIT_REPOSITORY_OPEN_CROSS_FS | GIT_REPOSITORY_OPEN_FROM_ENV,
                                     nullptr);
    return rc == 0;
}

// Map libgit2 bitfield to two-char porcelain-style code (X=index, Y=worktree)
static std::string to_porcelain_code(unsigned s) {
    // Special cases first
    if (s & GIT_STATUS_CONFLICTED) {
        return "UU";
    }
    if (s & GIT_STATUS_IGNORED) {
        return "!!";
    }

    // X (index) column
    char X = ' ';
    if      (s & GIT_STATUS_INDEX_NEW)        X = 'A';
    else if (s & GIT_STATUS_INDEX_MODIFIED)   X = 'M';
    else if (s & GIT_STATUS_INDEX_DELETED)    X = 'D';
    else if (s & GIT_STATUS_INDEX_RENAMED)    X = 'R';
    else if (s & GIT_STATUS_INDEX_TYPECHANGE) X = 'T';

    // Y (work tree) column
    char Y = ' ';
    if      (s & GIT_STATUS_WT_NEW)           Y = '?';
    else if (s & GIT_STATUS_WT_MODIFIED)      Y = 'M';
    else if (s & GIT_STATUS_WT_DELETED)       Y = 'D';
    else if (s & GIT_STATUS_WT_RENAMED)       Y = 'R';
    else if (s & GIT_STATUS_WT_TYPECHANGE)    Y = 'T';
    else if (s & GIT_STATUS_WT_UNREADABLE)    Y = '!';

    // Untracked files are rendered "??" (not " ?")
    if (X == ' ' && Y == '?') return "??";

    // Normal two-character code
    return std::string() + X + Y;
}

GitStatusResult get_git_status_for_dir(const fs::path& dir) {
    GitStatusResult result;

    git_repository* repo = nullptr;
    if (!repo_open_for_path(dir, &repo)) return result;
    result.repository_found = true;

    std::error_code ec;
    fs::path dir_abs = fs::is_directory(dir) ? dir : dir.parent_path();
    dir_abs = fs::weakly_canonical(dir_abs, ec);
    if (ec) dir_abs = fs::absolute(dir, ec);

    const char* wd = git_repository_workdir(repo);
    fs::path repo_root = wd ? fs::path(wd) : fs::current_path();
    repo_root = fs::weakly_canonical(repo_root, ec);

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags =
        GIT_STATUS_OPT_INCLUDE_UNTRACKED |
        GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
        GIT_STATUS_OPT_INCLUDE_IGNORED |
        GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
        GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

    git_status_list* status = nullptr;
    if (git_status_list_new(&status, repo, &opts) != 0) {
        git_repository_free(repo);
        return result;
    }

    const size_t count = git_status_list_entrycount(status);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* e = git_status_byindex(status, i);
        if (!e) continue;

        const unsigned s = e->status;
        const std::string code = to_porcelain_code(s);

        if (code.empty()) continue;

        // libgit2 paths are relative to repo root; prefer whichever delta is present
        std::string rel_from_repo;
        if (e->head_to_index && e->head_to_index->new_file.path)
            rel_from_repo = e->head_to_index->new_file.path;
        else if (e->index_to_workdir && e->index_to_workdir->new_file.path)
            rel_from_repo = e->index_to_workdir->new_file.path;

        if (rel_from_repo.empty()) continue;

        // Absolute path for the changed entry
        fs::path abs = repo_root / rel_from_repo;
        abs = fs::weakly_canonical(abs, ec);
        if (ec) continue;

        // Only keep entries under the listed directory; map keys relative to 'dir'
        const auto abs_str = abs.generic_string();
        const auto dir_str = dir_abs.generic_string();
        if (abs_str.size() >= dir_str.size() &&
            abs_str.compare(0, dir_str.size(), dir_str) == 0) {
            fs::path rel_to_dir = fs::relative(abs, dir_abs, ec);
            if (!ec) {
                std::string rel = rel_to_dir.generic_string();
                if (rel.empty() || rel == ".") {
                    result.default_modes.insert(code);
                    continue;
                }
                if (!rel.empty() && rel.back() == '/') rel.pop_back();
                auto slash = rel.find('/');
                std::string key = (slash == std::string::npos) ? rel : rel.substr(0, slash);
                if (!key.empty()) {
                    result.entries[key].insert(code);
                }
            }
        }
    }

    git_status_list_free(status);
    git_repository_free(repo);
    return result;
}

#else

GitStatusResult get_git_status_for_dir(const fs::path& /*dir*/) {
    // Stub: no git info.
    return {};
}

#endif

} // namespace nls
