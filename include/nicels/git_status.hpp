#pragma once

#include "nicels/entry.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nicels {

class GitRepositoryStatus {
public:
    GitRepositoryStatus();
    ~GitRepositoryStatus();

    [[nodiscard]] bool load(const std::filesystem::path& root);
    [[nodiscard]] std::optional<GitFileStatus> status_for(const std::filesystem::path& path) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nicels
