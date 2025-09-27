#pragma once

#include "nicels/config.hpp"
#include "nicels/entry.hpp"
#include "nicels/git_status.hpp"

#include <filesystem>
#include <vector>

namespace nicels {

class FileSystemScanner {
public:
    explicit FileSystemScanner(const Config::Data& config);

    [[nodiscard]] std::vector<FileEntry> scan(const std::filesystem::path& root) const;

private:
    const Config::Data& config_;

    [[nodiscard]] bool include_entry(const std::filesystem::directory_entry& entry) const;
    [[nodiscard]] bool matches_patterns(std::string_view name, const std::vector<std::string>& patterns) const;
};

} // namespace nicels
