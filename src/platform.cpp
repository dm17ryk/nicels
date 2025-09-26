#include "nicels/platform.hpp"

#include "nicels/logger.hpp"

#include <system_error>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#    include <io.h>
#    include <Aclapi.h>
#    include <sddl.h>
#else
#    include <pwd.h>
#    include <grp.h>
#    include <sys/ioctl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace nicels {

bool stdout_is_terminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

int detect_terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return info.srWindow.Right - info.srWindow.Left + 1;
    }
    return 80;
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80;
#endif
}

namespace {
#ifdef _WIN32
std::string sid_to_name(PSID sid, bool numeric) {
    if (!sid) {
        return {};
    }
    if (numeric) {
        LPSTR sid_string = nullptr;
        if (ConvertSidToStringSidA(sid, &sid_string)) {
            std::string result{sid_string};
            LocalFree(sid_string);
            return result;
        }
        return {};
    }

    char name[256];
    char domain[256];
    DWORD name_size = sizeof(name);
    DWORD domain_size = sizeof(domain);
    SID_NAME_USE use;
    if (LookupAccountSidA(nullptr, sid, name, &name_size, domain, &domain_size, &use)) {
        if (domain_size > 0) {
            return std::string{domain} + "\\" + name;
        }
        return std::string{name};
    }
    return sid_to_name(sid, true);
}
#endif
} // namespace

std::string owner_name(const std::filesystem::path& path, bool numeric, bool follow_symlink) {
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PSID owner = nullptr;
    if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &descriptor) == ERROR_SUCCESS) {
        std::string result = sid_to_name(owner, numeric);
        if (descriptor) {
            LocalFree(descriptor);
        }
        return result;
    }
    if (descriptor) {
        LocalFree(descriptor);
    }
    return {};
#else
    struct stat st;
    const auto stat_fn = follow_symlink ? ::stat : ::lstat;
    if (stat_fn(path.c_str(), &st) != 0) {
        return {};
    }
    if (numeric) {
        return std::to_string(st.st_uid);
    }
    if (auto* pw = ::getpwuid(st.st_uid)) {
        return pw->pw_name ? std::string{pw->pw_name} : std::to_string(st.st_uid);
    }
    return std::to_string(st.st_uid);
#endif
}

std::string group_name(const std::filesystem::path& path, bool numeric, bool follow_symlink) {
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PSID group = nullptr;
    if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, GROUP_SECURITY_INFORMATION, nullptr, &group, nullptr, nullptr, &descriptor) == ERROR_SUCCESS) {
        std::string result = sid_to_name(group, numeric);
        if (descriptor) {
            LocalFree(descriptor);
        }
        return result;
    }
    if (descriptor) {
        LocalFree(descriptor);
    }
    return {};
#else
    struct stat st;
    const auto stat_fn = follow_symlink ? ::stat : ::lstat;
    if (stat_fn(path.c_str(), &st) != 0) {
        return {};
    }
    if (numeric) {
        return std::to_string(st.st_gid);
    }
    if (auto* gr = ::getgrgid(st.st_gid)) {
        return gr->gr_name ? std::string{gr->gr_name} : std::to_string(st.st_gid);
    }
    return std::to_string(st.st_gid);
#endif
}

std::optional<std::uintmax_t> inode_number(const std::filesystem::directory_entry& entry) {
#ifdef _WIN32
    (void)entry;
    return std::nullopt;
#else
    struct stat st;
    if (::lstat(entry.path().c_str(), &st) != 0) {
        return std::nullopt;
    }
    return static_cast<std::uintmax_t>(st.st_ino);
#endif
}

bool has_executable_bit(const std::filesystem::directory_entry& entry) {
#ifdef _WIN32
    const auto attrs = GetFileAttributesW(entry.path().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (entry.is_directory()) {
        return false;
    }
    const auto extension = entry.path().extension().wstring();
    return extension == L".exe" || extension == L".bat" || extension == L".cmd" || extension == L".ps1";
#else
    std::error_code ec;
    const auto perms = entry.status(ec).permissions();
    if (ec) {
        return false;
    }
    using std::filesystem::perms;
    return (perms::owner_exec | perms::group_exec | perms::others_exec) & perms;
#endif
}

bool is_hidden(const std::filesystem::directory_entry& entry) {
#ifdef _WIN32
    const auto attrs = GetFileAttributesW(entry.path().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    const auto filename = entry.path().filename().string();
    return !filename.empty() && filename.front() == '.';
#endif
}

bool supports_color(ColorPolicy policy) {
    switch (policy) {
    case ColorPolicy::Always: return true;
    case ColorPolicy::Never: return false;
    case ColorPolicy::Auto: default: return stdout_is_terminal();
    }
}

std::string resolve_symlink(const std::filesystem::directory_entry& entry, bool follow) {
    if (!entry.is_symlink()) {
        return {};
    }
    std::error_code ec;
    auto target = std::filesystem::read_symlink(entry.path(), ec);
    if (ec) {
        return {};
    }
    if (follow) {
        auto resolved = std::filesystem::weakly_canonical(entry.path(), ec);
        if (!ec) {
            return resolved.string();
        }
    }
    return target.string();
}

} // namespace nicels
