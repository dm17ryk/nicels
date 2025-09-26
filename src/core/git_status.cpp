#include "nicels/git_status.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nicels/logger.h"

namespace nicels {
namespace {
#ifndef _WIN32
using popen_handle = std::unique_ptr<FILE, decltype(&pclose)>;
popen_handle make_pipe(const std::string& command) {
    return popen_handle(::popen(command.c_str(), "r"), pclose);
}
#else
using popen_handle = std::unique_ptr<FILE, decltype(&_pclose)>;
popen_handle make_pipe(const std::string& command) {
    return popen_handle(_popen(command.c_str(), "r"), _pclose);
}
#endif

std::string shell_quote(const std::filesystem::path& path) {
#ifndef _WIN32
    std::string result = "'";
    auto native = path.string();
    for (char ch : native) {
        if (ch == '\'') {
            result += "'\\''";
        } else {
            result += ch;
        }
    }
    result += "'";
    return result;
#else
    std::string result = "\"";
    auto native = path.string();
    for (char ch : native) {
        if (ch == '"') {
            result += "\\\"";
        } else {
            result += ch;
        }
    }
    result += "\"";
    return result;
#endif
}

std::string read_command(const std::string& command) {
    auto pipe = make_pipe(command);
    if (!pipe || !pipe.get()) {
        return {};
    }

    std::array<char, 4096> buffer{};
    std::string output;
    while (true) {
        std::size_t bytes = std::fread(buffer.data(), 1, buffer.size(), pipe.get());
        if (bytes == 0) {
            break;
        }
        output.append(buffer.data(), bytes);
    }
    return output;
}

std::unordered_map<std::string, std::string> parse_porcelain(std::string_view data) {
    std::unordered_map<std::string, std::string> map;
    std::size_t pos = 0;
    while (pos < data.size()) {
        std::size_t end = data.find('\0', pos);
        if (end == std::string_view::npos) {
            break;
        }
        if (end == pos) {
            break;
        }
        std::string_view entry = data.substr(pos, end - pos);
        if (entry.size() >= 3) {
            std::string status(entry.substr(0, 2));
            std::string path(entry.substr(3));
            auto rename_pos = path.find(" -> ");
            if (rename_pos != std::string::npos) {
                path = path.substr(rename_pos + 4);
            }
            map.emplace(std::move(path), std::move(status));
        }
        pos = end + 1;
    }
    return map;
}

} // namespace

GitStatusCache::GitStatusCache(bool enabled)
    : enabled_{enabled} {}

std::string GitStatusCache::status_for(const std::filesystem::path& path) {
    if (!enabled_) {
        return {};
    }

    auto repo_root = find_repository_root(path);
    if (!repo_root) {
        return {};
    }

    Repository& repo = load_repository(*repo_root);
    if (!repo.loaded) {
        refresh_repository(repo);
    }

    std::error_code ec;
    auto relative = std::filesystem::relative(path, repo.root, ec);
    if (ec) {
        return {};
    }

    auto key = relative.generic_string();
    auto it = repo.entries.find(key);
    if (it != repo.entries.end()) {
        return it->second;
    }

    return {};
}

std::optional<std::filesystem::path> GitStatusCache::find_repository_root(std::filesystem::path path) const {
    std::error_code ec;
    path = std::filesystem::absolute(path, ec);
    if (ec) {
        return std::nullopt;
    }

    if (std::filesystem::is_directory(path, ec)) {
        // ok
    } else {
        path = path.parent_path();
    }

    while (!path.empty()) {
        auto candidate = path / ".git";
        if (std::filesystem::exists(candidate, ec)) {
            return path;
        }
        auto parent = path.parent_path();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return std::nullopt;
}

GitStatusCache::Repository& GitStatusCache::load_repository(const std::filesystem::path& root) {
    auto it = cache_.find(root);
    if (it == cache_.end()) {
        Repository repo;
        repo.root = root;
        auto [inserted_it, _] = cache_.emplace(root, std::move(repo));
        return inserted_it->second;
    }
    return it->second;
}

void GitStatusCache::refresh_repository(Repository& repo) {
    std::string command = "git -C " + shell_quote(repo.root) + " status --porcelain -z";
    auto output = read_command(command);
    if (output.empty()) {
        repo.entries.clear();
        repo.loaded = true;
        return;
    }

    repo.entries = parse_porcelain(output);
    repo.loaded = true;
}

} // namespace nicels
