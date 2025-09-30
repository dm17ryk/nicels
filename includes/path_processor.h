#pragma once

#include <filesystem>
#include <vector>

#include "config.h"
#include "fs_scanner.h"
#include "git_status.h"
#include "renderer.h"

namespace nls {

class PathProcessor {
public:
    PathProcessor(const Config& config,
                  FileScanner& scanner,
                  Renderer& renderer,
                  GitStatus& git_status) noexcept;

    [[nodiscard]] VisitResult process(const std::filesystem::path& path);

private:
    [[nodiscard]] VisitResult listPath(const std::filesystem::path& path);
    [[nodiscard]] std::vector<TreeItem> buildTreeItems(const std::filesystem::path& dir,
                                                       std::size_t depth,
                                                       std::vector<Entry>& flat,
                                                       VisitResult& status);
    void applyGitStatus(std::vector<Entry>& items, const std::filesystem::path& dir);
    void sortEntries(std::vector<Entry>& entries) const;

    [[nodiscard]] const Config& options() const noexcept { return config_; }
    [[nodiscard]] FileScanner& scanner() noexcept { return scanner_; }
    [[nodiscard]] Renderer& renderer() noexcept { return renderer_; }
    [[nodiscard]] GitStatus& gitStatus() noexcept { return git_status_; }

    const Config& config_;
    FileScanner& scanner_;
    Renderer& renderer_;
    GitStatus& git_status_;
};

}  // namespace nls

