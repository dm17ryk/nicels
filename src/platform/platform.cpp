#include "nicels/platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace nicels::platform {

bool stdout_supports_color() {
#ifdef _WIN32
    HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    // Ensure UTF-8 so Nerd Font icons render in Windows Terminal/PowerShell
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    DWORD mode = 0;
    if (!::GetConsoleMode(handle, &mode)) {
        return false;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    ::SetConsoleMode(handle, mode);
    return true;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

int terminal_width() {
#ifdef _WIN32
    HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return 80;
    }
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!::GetConsoleScreenBufferInfo(handle, &info)) {
        return 80;
    }
    return static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
#else
    struct winsize ws {
    };
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return static_cast<int>(ws.ws_col);
    }
    return 80;
#endif
}

} // namespace nicels::platform
