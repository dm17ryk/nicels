#include "nicels/git_status.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace nicels {

GitStatusCache::GitStatusCache() = default;

void GitStatusCache::configure(bool enabled) {
    enabled_ = enabled;
}

bool GitStatusCache::enabled() const noexcept {
    return enabled_;
}

std::optional<GitStatusSymbol> GitStatusCache::status_for(const std::filesystem::path&) const {
    if (!enabled_) {
        return std::nullopt;
    }
    return std::nullopt;
}

char to_indicator(GitStatusSymbol symbol) {
    switch (symbol) {
    case GitStatusSymbol::Clean:
        return ' ';
    case GitStatusSymbol::Modified:
        return 'M';
    case GitStatusSymbol::Added:
        return 'A';
    case GitStatusSymbol::Deleted:
        return 'D';
    case GitStatusSymbol::Renamed:
        return 'R';
    case GitStatusSymbol::Conflict:
        return 'C';
    case GitStatusSymbol::Ignored:
        return 'I';
    case GitStatusSymbol::Untracked:
        return '?';
    case GitStatusSymbol::Unknown:
    default:
        return '!';
    }
}

} // namespace nicels
