#include "nicels/git_status.hpp"

#include <filesystem>
#include <memory>
#include <system_error>
#include <unordered_map>

#if __has_include(<git2.h>)
#  include <git2.h>
#  define NICELS_HAS_LIBGIT2 1
#else
#  define NICELS_HAS_LIBGIT2 0
#endif

namespace nicels {

namespace {
#if NICELS_HAS_LIBGIT2
struct GitLibrary {
    GitLibrary() { git_libgit2_init(); }
    ~GitLibrary() { git_libgit2_shutdown(); }
};

GitLibrary& git_library() {
    static GitLibrary library;
    return library;
}

bool has_staged(unsigned status) {
    return (status & (GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_INDEX_DELETED |
        GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_TYPECHANGE)) != 0;
}

bool has_unstaged(unsigned status) {
    return (status & (GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_DELETED |
        GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_TYPECHANGE | GIT_STATUS_WT_UNREADABLE)) != 0;
}

bool is_untracked(unsigned status) { return (status & GIT_STATUS_WT_NEW) != 0; }

#endif
} // namespace

struct GitRepositoryStatus::Impl {
    std::filesystem::path repo_root;
    std::filesystem::path query_root;
    std::unordered_map<std::string, GitFileStatus> status_map;
    bool loaded { false };
};

GitRepositoryStatus::GitRepositoryStatus() : impl_(std::make_unique<Impl>()) {
#if NICELS_HAS_LIBGIT2
    git_library();
#endif
}

GitRepositoryStatus::~GitRepositoryStatus() = default;

bool GitRepositoryStatus::load(const std::filesystem::path& root) {
#if !NICELS_HAS_LIBGIT2
    (void)root;
    impl_->status_map.clear();
    impl_->loaded = false;
    return false;
#else
    impl_->status_map.clear();
    impl_->loaded = false;

    git_repository* repo = nullptr;
    // git_libgit2_init();

    std::error_code ec;
    std::filesystem::path search = std::filesystem::is_directory(root) ? root : root.parent_path();
    if (git_repository_open_ext(&repo, search.string().c_str(), GIT_REPOSITORY_OPEN_CROSS_FS | GIT_REPOSITORY_OPEN_FROM_ENV, nullptr) != 0) {
        return false;
    }

    impl_->loaded = true;

    const char* wd = git_repository_workdir(repo);
    std::filesystem::path repo_root = wd ? std::filesystem::path(wd) : std::filesystem::current_path();
    repo_root = std::filesystem::weakly_canonical(repo_root, ec);
    if (ec) {
        repo_root = std::filesystem::absolute(repo_root, ec);
    }

    auto query_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        query_root = std::filesystem::absolute(root, ec);
    }

    impl_->repo_root = repo_root;
    impl_->query_root = query_root;

    git_status_options options = GIT_STATUS_OPTIONS_INIT;
    options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS | GIT_STATUS_OPT_INCLUDE_IGNORED |
        GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX | GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

    git_status_list* status_list = nullptr;
    if (git_status_list_new(&status_list, repo, &options) != 0) {
        git_repository_free(repo);
        return false;
    }

    const auto count = git_status_list_entrycount(status_list);
    impl_->status_map.reserve(static_cast<size_t>(count));

    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* entry = git_status_byindex(status_list, i);
        if (!entry) {
            continue;
        }

        unsigned status = entry->status;
        if ((status & GIT_STATUS_IGNORED) != 0) {
            continue;
        }

        std::string path_from_repo;
        if (entry->head_to_index && entry->head_to_index->new_file.path) {
            path_from_repo = entry->head_to_index->new_file.path;
        } else if (entry->index_to_workdir && entry->index_to_workdir->new_file.path) {
            path_from_repo = entry->index_to_workdir->new_file.path;
        }

        if (path_from_repo.empty()) {
            continue;
        }

        auto absolute_path = std::filesystem::weakly_canonical(repo_root / path_from_repo, ec);
        if (ec) {
            ec.clear();
            absolute_path = std::filesystem::absolute(repo_root / path_from_repo, ec);
        }
        if (ec) {
            continue;
        }

        auto relative_to_query = std::filesystem::relative(absolute_path, query_root, ec);
        if (ec) {
            ec.clear();
            continue;
        }

        std::string key = relative_to_query.generic_string();
        if (key.empty() || key == ".") {
            continue;
        }

        GitFileStatus file_status;
        file_status.staged = has_staged(status);
        file_status.unstaged = has_unstaged(status);
        file_status.untracked = is_untracked(status);

        impl_->status_map[key] = file_status;
    }

    git_status_list_free(status_list);
    git_repository_free(repo);
    return true;
#endif
}

std::optional<GitFileStatus> GitRepositoryStatus::status_for(const std::filesystem::path& path) const {
#if !NICELS_HAS_LIBGIT2
    (void)path;
    return std::nullopt;
#else
    if (!impl_->loaded) {
        return std::nullopt;
    }

    std::error_code ec;
    auto canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = std::filesystem::absolute(path, ec);
    }
    if (ec) {
        return std::nullopt;
    }

    auto relative = std::filesystem::relative(canonical_path, impl_->query_root, ec);
    if (ec) {
        return std::nullopt;
    }

    auto key = relative.generic_string();
    auto it = impl_->status_map.find(key);
    if (it == impl_->status_map.end()) {
        return std::nullopt;
    }
    return it->second;
#endif
}

} // namespace nicels
