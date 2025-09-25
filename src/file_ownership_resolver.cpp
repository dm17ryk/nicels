#include "file_ownership_resolver.h"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <Aclapi.h>
#include <sddl.h>
#include <windows.h>
#include <winioctl.h>
#endif

namespace nls {

#ifndef _WIN32
bool FileOwnershipResolver::MultiplyWithOverflow(uintmax_t a, uintmax_t b, uintmax_t& result) const {
    if (a == 0 || b == 0) {
        result = 0;
        return true;
    }
    if (a > std::numeric_limits<uintmax_t>::max() / b) {
        return false;
    }
    result = a * b;
    return true;
}
#endif


void FileOwnershipResolver::Populate(FileInfo& file_info, bool dereference) const {
#ifndef _WIN32
    file_info.owner.clear();
    file_info.group.clear();
    file_info.has_owner_id = false;
    file_info.has_group_id = false;
    file_info.owner_numeric.clear();
    file_info.group_numeric.clear();
    file_info.has_owner_numeric = false;
    file_info.has_group_numeric = false;
    file_info.has_link_size = false;
    file_info.allocated_size = 0;
    file_info.has_allocated_size = false;

    auto assign_from_stat = [&](const struct stat& st) {
        file_info.nlink = st.st_nlink;
        file_info.inode = static_cast<uintmax_t>(st.st_ino);
        file_info.owner_id = static_cast<uintmax_t>(st.st_uid);
        file_info.group_id = static_cast<uintmax_t>(st.st_gid);
        file_info.has_owner_id = true;
        file_info.has_group_id = true;
        file_info.owner_numeric = std::to_string(static_cast<uintmax_t>(st.st_uid));
        file_info.group_numeric = std::to_string(static_cast<uintmax_t>(st.st_gid));
        file_info.has_owner_numeric = true;
        file_info.has_group_numeric = true;
        if (auto* pw = ::getpwuid(st.st_uid)) {
            file_info.owner = pw->pw_name;
        } else {
            file_info.owner = std::to_string(static_cast<uintmax_t>(st.st_uid));
        }
        if (auto* gr = ::getgrgid(st.st_gid)) {
            file_info.group = gr->gr_name;
        } else {
            file_info.group = std::to_string(static_cast<uintmax_t>(st.st_gid));
        }
        if (st.st_blocks >= 0) {
            uintmax_t blocks = static_cast<uintmax_t>(st.st_blocks);
            uintmax_t allocated = 0;
            if (!MultiplyWithOverflow(blocks, static_cast<uintmax_t>(512), allocated)) {
                allocated = std::numeric_limits<uintmax_t>::max();
            }
            file_info.allocated_size = allocated;
            file_info.has_allocated_size = true;
        }
    };

    struct stat link_stat {};
    if (::lstat(file_info.path.c_str(), &link_stat) == 0) {
        assign_from_stat(link_stat);
        file_info.link_size = static_cast<uintmax_t>(link_stat.st_size);
        file_info.has_link_size = true;
    }

    if (dereference) {
        struct stat target_stat {};
        if (::stat(file_info.path.c_str(), &target_stat) == 0) {
            assign_from_stat(target_stat);
        }
    }
#else
    file_info.nlink = 1;
    file_info.owner.clear();
    file_info.group.clear();
    file_info.inode = 0;
    file_info.has_owner_id = false;
    file_info.has_group_id = false;
    file_info.owner_numeric.clear();
    file_info.group_numeric.clear();
    file_info.has_owner_numeric = false;
    file_info.has_group_numeric = false;
    file_info.has_link_size = false;
    file_info.allocated_size = 0;
    file_info.has_allocated_size = false;

    bool want_target_attributes = dereference && !file_info.is_broken_symlink;
    SymlinkResolver resolver;
    std::filesystem::path query_path = file_info.path;
    if (want_target_attributes) {
        if (auto resolved = resolver.ResolveTarget(file_info)) {
            query_path = std::move(*resolved);
        }
    }

    std::wstring native_path = query_path.native();

    ::SetLastError(ERROR_SUCCESS);
    DWORD compressed_high = 0;
    DWORD compressed_low = ::GetCompressedFileSizeW(native_path.c_str(), &compressed_high);
    DWORD compressed_err = ::GetLastError();
    if (compressed_low != INVALID_FILE_SIZE || compressed_err == ERROR_SUCCESS) {
        ULARGE_INTEGER comp{};
        comp.HighPart = compressed_high;
        comp.LowPart = compressed_low;
        file_info.allocated_size = static_cast<uintmax_t>(comp.QuadPart);
        file_info.has_allocated_size = true;
    }

    DWORD attributes = ::GetFileAttributesW(native_path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        file_info.has_link_size = true;
        HANDLE link_handle = ::CreateFileW(native_path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (link_handle != INVALID_HANDLE_VALUE) {
            std::array<std::byte, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> buffer{};
            DWORD bytes_returned = 0;
            if (::DeviceIoControl(link_handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(),
                    static_cast<DWORD>(buffer.size()), &bytes_returned, nullptr) != 0) {
                file_info.link_size = bytes_returned;
            }
            ::CloseHandle(link_handle);
        }
    }

    DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (file_info.is_symlink && !want_target_attributes) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    HANDLE handle = ::CreateFileW(native_path.c_str(), FILE_READ_ATTRIBUTES, share_mode, nullptr,
        OPEN_EXISTING, flags, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION file_info_data{};
        if (::GetFileInformationByHandle(handle, &file_info_data)) {
            file_info.nlink = file_info_data.nNumberOfLinks;
            ULARGE_INTEGER index{};
            index.HighPart = file_info_data.nFileIndexHigh;
            index.LowPart = file_info_data.nFileIndexLow;
            file_info.inode = static_cast<uintmax_t>(index.QuadPart);
            if (file_info.is_symlink) {
                ULARGE_INTEGER sz{};
                sz.HighPart = file_info_data.nFileSizeHigh;
                sz.LowPart = file_info_data.nFileSizeLow;
                uintmax_t handle_size = static_cast<uintmax_t>(sz.QuadPart);
                if (!want_target_attributes) {
                    file_info.link_size = handle_size;
                    file_info.has_link_size = true;
                } else if ((file_info_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    file_info.size = handle_size;
                }
            }
        }
        ::CloseHandle(handle);
    }

    auto wide_to_utf8 = [](const std::wstring& wide) -> std::string {
        if (wide.empty()) {
            return {};
        }
        int required = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1) {
            return {};
        }
        std::string utf8(static_cast<size_t>(required - 1), '\0');
        int converted = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), required, nullptr, nullptr);
        if (converted <= 0) {
            return {};
        }
        return utf8;
    };

    auto sid_to_string = [&](PSID sid) -> std::string {
        if (sid == nullptr || !::IsValidSid(sid)) {
            return {};
        }
        LPWSTR sid_w = nullptr;
        if (!::ConvertSidToStringSidW(sid, &sid_w)) {
            return {};
        }
        std::wstring sid_native = sid_w ? sid_w : L"";
        if (sid_w) {
            ::LocalFree(sid_w);
        }
        return wide_to_utf8(sid_native);
    };

    auto sid_to_rid = [&](PSID sid) -> std::optional<uintmax_t> {
        if (sid == nullptr || !::IsValidSid(sid)) {
            return std::nullopt;
        }
        auto* count = ::GetSidSubAuthorityCount(sid);
        if (count == nullptr || *count == 0) {
            return std::nullopt;
        }
        DWORD value = *::GetSidSubAuthority(sid, static_cast<DWORD>(*count - 1));
        return static_cast<uintmax_t>(value);
    };

    auto sid_to_account_name = [&](PSID sid) -> std::string {
        if (sid == nullptr || !::IsValidSid(sid)) {
            return {};
        }

        DWORD name_len = 0;
        DWORD domain_len = 0;
        SID_NAME_USE sid_type;
        if (!::LookupAccountSidW(nullptr, sid, nullptr, &name_len, nullptr, &domain_len, &sid_type)) {
            DWORD err = ::GetLastError();
            if (err != ERROR_INSUFFICIENT_BUFFER || name_len == 0) {
                return {};
            }
        }

        std::wstring name(name_len, L'\0');
        std::wstring domain(domain_len, L'\0');
        if (!::LookupAccountSidW(nullptr, sid, name_len ? name.data() : nullptr, &name_len,
                domain_len ? domain.data() : nullptr, &domain_len, &sid_type)) {
            return {};
        }

        name.resize(name_len);
        domain.resize(domain_len);

        if (name.empty()) {
            return {};
        }
        if (!domain.empty()) {
            return wide_to_utf8(domain + L"\\" + name);
        }
        return wide_to_utf8(name);
    };

    PSECURITY_DESCRIPTOR security_descriptor = nullptr;
    PSID owner_sid = nullptr;
    PSID group_sid = nullptr;
    DWORD result = ::GetNamedSecurityInfoW(native_path.c_str(), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        &owner_sid, &group_sid, nullptr, nullptr, &security_descriptor);

    if (result == ERROR_SUCCESS) {
        file_info.owner = sid_to_account_name(owner_sid);
        file_info.group = sid_to_account_name(group_sid);
        std::string owner_sid_string = sid_to_string(owner_sid);
        if (!owner_sid_string.empty()) {
            file_info.owner_numeric = std::move(owner_sid_string);
            file_info.has_owner_numeric = true;
        }
        std::string group_sid_string = sid_to_string(group_sid);
        if (!group_sid_string.empty()) {
            file_info.group_numeric = std::move(group_sid_string);
            file_info.has_group_numeric = true;
        }
        if (auto owner_rid = sid_to_rid(owner_sid)) {
            file_info.owner_id = *owner_rid;
            file_info.has_owner_id = true;
        }
        if (auto group_rid = sid_to_rid(group_sid)) {
            file_info.group_id = *group_rid;
            file_info.has_group_id = true;
        }
    }

    if (security_descriptor) {
        ::LocalFree(security_descriptor);
    }
#endif
}

} // namespace nls
