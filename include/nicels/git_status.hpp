#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace nicels {

enum class GitStatusSymbol {
    Clean,
    Modified,
    Added,
    Deleted,
    Renamed,
    Conflict,
    Ignored,
    Untracked,
    Unknown
};

class GitStatusCache {
public:
    GitStatusCache();

    void configure(bool enabled);
    [[nodiscard]] bool enabled() const noexcept;

    std::optional<GitStatusSymbol> status_for(const std::filesystem::path& file_path) const;

private:
    bool enabled_{false};
};

char to_indicator(GitStatusSymbol symbol);

} // namespace nicels
