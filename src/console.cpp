#include "console.h"

#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#endif

#include <cstdio>

namespace nls {

bool enable_virtual_terminal() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // Ensure UTF-8 so Nerd Font icons render in Windows Terminal/PowerShell
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    if (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) return true;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
#else
    return true;
#endif
}

int terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return (csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }
    return 80;
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

} // namespace nls
