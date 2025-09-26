#pragma once

#include <string>

#include "nicels/config.h"
#include "nicels/fs.h"
#include "nicels/theme.h"

namespace nicels::formatter {

std::string permissions(const FileEntry& entry);
std::string link_count(const FileEntry& entry);
std::string owner(const FileEntry& entry, const Config::Options& options);
std::string group(const FileEntry& entry, const Config::Options& options);
std::string size(const FileEntry& entry, const Config::Options& options);
std::string modified_time(const FileEntry& entry, const Config::Options& options);
std::string inode(const FileEntry& entry);
std::string git_status(const FileEntry& entry, const Theme& theme);

} // namespace nicels::formatter
