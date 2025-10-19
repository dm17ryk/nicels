#pragma once

#include <filesystem>
#include <memory>

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
    const Config& options() const { return *config_; }
    FileScanner& scanner() { return *scanner_; }
    Renderer& renderer() { return *renderer_; }

    int runDatabaseCommand(Config::DbAction action);

    CommandLineParser parser_{};
    Config* config_{nullptr};
    FileOwnershipResolver ownership_resolver_{};
    SymlinkResolver symlink_resolver_{};
    GitStatus git_status_{};
    std::unique_ptr<FileScanner> scanner_{};
    std::unique_ptr<Renderer> renderer_{};
};

}  // namespace nls
