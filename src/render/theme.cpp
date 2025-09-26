#include "nicels/theme.h"

#include <algorithm>

namespace nicels {

namespace {
std::string to_utf8(const char8_t* text) {
    return std::string{reinterpret_cast<const char*>(text)};
}
} // namespace

Theme::Theme(const Config::Options& options)
    : options_{options} {
    load_default_icons();
    load_default_colors();
}

void Theme::load_default_icons() {
    icon_map_["folder"] = to_utf8(u8"\uf07b");
    icon_map_["hidden"] = to_utf8(u8"\uf19fc");
    icon_map_["symlink"] = to_utf8(u8"\ue5ff");
    icon_map_["file"] = to_utf8(u8"\uf15b");
    icon_map_["exe"] = to_utf8(u8"\uf144");
    icon_map_["txt"] = to_utf8(u8"\uf15c");
    icon_map_["md"] = to_utf8(u8"\uf48a");
    icon_map_["json"] = to_utf8(u8"\ue60b");
    icon_map_["png"] = to_utf8(u8"\uf1c5");
    icon_map_["jpg"] = to_utf8(u8"\uf1c5");
    icon_map_["jpeg"] = to_utf8(u8"\uf1c5");
    icon_map_["gif"] = to_utf8(u8"\uf1c5");
    icon_map_["cpp"] = to_utf8(u8"\ue61d");
    icon_map_["hpp"] = to_utf8(u8"\uf0fd");
    icon_map_["h"] = to_utf8(u8"\uf0fd");
    icon_map_["py"] = to_utf8(u8"\ue235");
    icon_map_["rb"] = to_utf8(u8"\ue21e");
    icon_map_["js"] = to_utf8(u8"\ue74e");
    icon_map_["ts"] = to_utf8(u8"\ue628");
    icon_map_["zip"] = to_utf8(u8"\uf1c6");
    icon_map_["gz"] = to_utf8(u8"\uf1c6");
    icon_map_["7z"] = to_utf8(u8"\uf1c6");
    icon_map_["pdf"] = to_utf8(u8"\uf1c1");
}

void Theme::load_default_colors() {
    color_map_["directory"] = "\033[38;5;33m";
    color_map_["symlink"] = "\033[38;5;45m";
    color_map_["executable"] = "\033[38;5;166m";
    color_map_["fifo"] = "\033[38;5;220m";
    color_map_["socket"] = "\033[38;5;46m";
    color_map_["block"] = "\033[38;5;214m";
    color_map_["character"] = "\033[38;5;109m";
    color_map_["regular"] = "";
    color_map_["broken"] = "\033[38;5;160m";
    color_map_["git_modified"] = "\033[38;5;178m";
    color_map_["git_untracked"] = "\033[38;5;39m";
    color_map_["git_staged"] = "\033[38;5;34m";
}

std::string Theme::icon_for(const FileEntry& entry) const {
    if (!options_.icons_enabled) {
        return {};
    }

    std::string key = entry.icon_key;
    auto it = icon_map_.find(key);
    if (it != icon_map_.end()) {
        return it->second;
    }

    if (entry.type == FileEntry::Type::Directory) {
        return icon_map_.at("folder");
    }
    if (entry.type == FileEntry::Type::Symlink) {
        auto it2 = icon_map_.find("symlink");
        return it2 != icon_map_.end() ? it2->second : std::string{};
    }

    return icon_map_.at("file");
}

std::string Theme::colorize(const FileEntry& entry, std::string_view text) const {
    if (!options_.color_enabled || text.empty()) {
        return std::string{text};
    }

    std::string key;
    if (entry.is_broken_symlink) {
        key = "broken";
    } else if (entry.type == FileEntry::Type::Directory) {
        key = "directory";
    } else if (entry.type == FileEntry::Type::Symlink) {
        key = "symlink";
    } else if (entry.type == FileEntry::Type::Fifo) {
        key = "fifo";
    } else if (entry.type == FileEntry::Type::Socket) {
        key = "socket";
    } else if (entry.type == FileEntry::Type::Block) {
        key = "block";
    } else if (entry.type == FileEntry::Type::Character) {
        key = "character";
    } else if (entry.executable) {
        key = "executable";
    } else {
        key = "regular";
    }

    auto it = color_map_.find(key);
    if (it == color_map_.end() || it->second.empty()) {
        return std::string{text};
    }
    return it->second + std::string{text} + reset();
}

std::string Theme::git_status_color(std::string_view status) const {
    if (!options_.color_enabled) {
        return {};
    }
    if (status.empty()) {
        return {};
    }
    char index = status[0];
    char worktree = status.size() > 1 ? status[1] : ' ';
    if (index == '?' || worktree == '?') {
        auto it = color_map_.find("git_untracked");
        return it != color_map_.end() ? it->second : std::string{};
    }
    if (index == 'M' || worktree == 'M') {
        auto it = color_map_.find("git_modified");
        return it != color_map_.end() ? it->second : std::string{};
    }
    if (index != ' ' && index != '?') {
        auto it = color_map_.find("git_staged");
        return it != color_map_.end() ? it->second : std::string{};
    }
    return {};
}

std::string Theme::reset() const {
    return options_.color_enabled ? std::string{"\033[0m"} : std::string{};
}

} // namespace nicels
