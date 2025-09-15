\
#include "git_status.h"
#include <system_error>
#include <unordered_map>

namespace fs = std::filesystem;

namespace nls {

#ifdef USE_LIBGIT2
// Minimal libgit2-based status (non-recursive example). Expand as needed.
#include <git2.h>

static bool repo_open_for_path(const fs::path& p, git_repository** out) {
    git_libgit2_init();
    int flags = GIT_REPOSITORY_OPEN_NO_SEARCH; // try exact
    fs::path dir = fs::is_directory(p) ? p : p.parent_path();
    int rc = git_repository_open_ext(out, dir.string().c_str(), 0, nullptr);
    if (rc != 0) return false;
    return true;
}

GitStatusMap get_git_status_for_dir(const fs::path& dir) {
    GitStatusMap map;
    git_repository* repo = nullptr;
    if (!repo_open_for_path(dir, &repo)) return map;
    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
    git_status_list* status = nullptr;
    if (git_status_list_new(&status, repo, &opts) != 0) {
        git_repository_free(repo);
        return map;
    }
    size_t count = git_status_list_entrycount(status);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* e = git_status_byindex(status, i);
        if (!e) continue;
        std::string code = "  ";
        unsigned s = e->status;
        if (s & GIT_STATUS_WT_NEW)      code = "??";
        else if (s & GIT_STATUS_WT_MODIFIED) code = " M";
        else if (s & GIT_STATUS_WT_DELETED)  code = " D";
        else if (s & GIT_STATUS_INDEX_NEW)   code = "A ";
        else if (s & GIT_STATUS_INDEX_MODIFIED) code = "M ";
        else if (s & GIT_STATUS_INDEX_DELETED)  code = "D ";
        std::string path;
        if (e->head_to_index && e->head_to_index->new_file.path) path = e->head_to_index->new_file.path;
        else if (e->index_to_workdir && e->index_to_workdir->new_file.path) path = e->index_to_workdir->new_file.path;
        if (!path.empty()) {
            map[path] = code;
        }
    }
    git_status_list_free(status);
    git_repository_free(repo);
    return map;
}

#else

GitStatusMap get_git_status_for_dir(const fs::path& /*dir*/) {
    // Stub: no git info. Return empty -> treat as clean.
    return {};
}

#endif

} // namespace nls
