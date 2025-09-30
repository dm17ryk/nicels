#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "command_line_parser.h"
#include "config.h"
#include "file_ownership_resolver.h"
#include "fs_scanner.h"
#include "git_status.h"
#include "renderer.h"
#include "symlink_resolver.h"

namespace nls {

class App {
public:
    int run(int argc, char** argv);

private:
    VisitResult processPath(const std::filesystem::path& path);
    VisitResult listPath(const std::filesystem::path& path);
    std::vector<TreeItem> buildTreeItems(const std::filesystem::path& dir,
                                         std::size_t depth,
                                         std::vector<Entry>& flat,
                                         VisitResult& status);
    void applyGitStatus(std::vector<Entry>& items, const std::filesystem::path& dir);
    void sortEntries(std::vector<Entry>& entries);

    const Config& options() const { return *config_; }
    FileScanner& scanner() { return *scanner_; }
    Renderer& renderer() { return *renderer_; }

    CommandLineParser parser_{};
    Config* config_{nullptr};
    FileOwnershipResolver ownership_resolver_{};
    SymlinkResolver symlink_resolver_{};
    GitStatus git_status_{};
    std::unique_ptr<FileScanner> scanner_{};
    std::unique_ptr<Renderer> renderer_{};
};

}  // namespace nls

