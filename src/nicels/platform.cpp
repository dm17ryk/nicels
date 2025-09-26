#include "nicels/platform.hpp"

#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace nicels::platform {

bool stdout_is_tty() {
#ifdef _WIN32
    return ::_isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

std::uint32_t terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return static_cast<std::uint32_t>(info.dwSize.X);
    }
    return 80;
#else
    struct winsize ws {
    };
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        return ws.ws_col;
    }
    return 80;
#endif
}

void enable_virtual_terminal_processing() {
#ifdef _WIN32
    HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD mode = 0;
    if (!::GetConsoleMode(handle, &mode)) {
        return;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    ::SetConsoleMode(handle, mode);
#endif
}

std::string system_locale() {
    if (const char* lang = std::getenv("LC_ALL")) {
        return lang;
    }
    if (const char* lang = std::getenv("LANG")) {
        return lang;
    }
    return "";
}

} // namespace nicels::platform
