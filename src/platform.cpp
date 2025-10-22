#include "platform.h"

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX 1
#    endif
#    include <windows.h>
#    include <winreg.h>
#else
#    include <fcntl.h>
#    include <sys/ioctl.h>
#    include <sys/select.h>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <termios.h>
#    include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#ifndef _WIN32
#    include <cstdio>
#endif

namespace nls {

namespace {

Platform::SystemTheme parseThemeString(std::string_view theme)
{
    if (theme.empty()) {
        return Platform::SystemTheme::Unknown;
    }
    std::string lowered(theme);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (lowered == "dark" || lowered == "dark-mode" || lowered == "darkmode") {
        return Platform::SystemTheme::Dark;
    }
    if (lowered == "light" || lowered == "light-mode" || lowered == "lightmode") {
        return Platform::SystemTheme::Light;
    }
    return Platform::SystemTheme::Unknown;
}

Platform::SystemTheme detectFromGtkTheme()
{
    if (const char* value = std::getenv("GTK_THEME")) {
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (lowered.find("dark") != std::string::npos) {
            return Platform::SystemTheme::Dark;
        }
        if (lowered.find("light") != std::string::npos) {
            return Platform::SystemTheme::Light;
        }
    }
    if (const char* value = std::getenv("QT_QPA_PLATFORMTHEME")) {
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (lowered.find("dark") != std::string::npos) {
            return Platform::SystemTheme::Dark;
        }
        if (lowered.find("light") != std::string::npos) {
            return Platform::SystemTheme::Light;
        }
    }
    if (const char* value = std::getenv("QT_STYLE_OVERRIDE")) {
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (lowered.find("dark") != std::string::npos) {
            return Platform::SystemTheme::Dark;
        }
        if (lowered.find("light") != std::string::npos) {
            return Platform::SystemTheme::Light;
        }
    }
    return Platform::SystemTheme::Unknown;
}

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

double computeLuma(const Rgb& rgb)
{
    constexpr double inv = 1.0 / 255.0;
    double r = static_cast<double>(rgb.r) * inv;
    double g = static_cast<double>(rgb.g) * inv;
    double b = static_cast<double>(rgb.b) * inv;
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

Platform::SystemTheme themeFromRgb(const Rgb& rgb)
{
    return computeLuma(rgb) < 0.5 ? Platform::SystemTheme::Dark : Platform::SystemTheme::Light;
}

Platform::SystemTheme detectFromColorFgbg()
{
    const char* env = std::getenv("COLORFGBG");
    if (!env || !*env) {
        return Platform::SystemTheme::Unknown;
    }

    std::string value(env);
    std::size_t pos = value.find_last_of(";:");
    std::string token = pos == std::string::npos ? value : value.substr(pos + 1);
    try {
        int bg_value = std::stoi(token);
        static constexpr std::array<Rgb, 16> palette = {
            Rgb{0, 0, 0},       Rgb{205, 0, 0},    Rgb{0, 205, 0},   Rgb{205, 205, 0},
            Rgb{0, 0, 238},     Rgb{205, 0, 205},  Rgb{0, 205, 205}, Rgb{229, 229, 229},
            Rgb{127, 127, 127}, Rgb{255, 0, 0},    Rgb{0, 255, 0},   Rgb{255, 255, 0},
            Rgb{92, 92, 255},   Rgb{255, 0, 255},  Rgb{0, 255, 255}, Rgb{255, 255, 255}};
        if (bg_value >= 0 && bg_value < static_cast<int>(palette.size())) {
            return themeFromRgb(palette[static_cast<std::size_t>(bg_value)]);
        }
    } catch (...) {
        return Platform::SystemTheme::Unknown;
    }

    return Platform::SystemTheme::Unknown;
}

#ifdef _WIN32
Platform::SystemTheme detectFromWindowsPersonalization()
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    LONG status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD,
        nullptr,
        &value,
        &size);

    if (status == ERROR_SUCCESS) {
        return value != 0 ? Platform::SystemTheme::Light : Platform::SystemTheme::Dark;
    }

    return Platform::SystemTheme::Unknown;
}
#endif

#ifndef _WIN32

std::optional<Rgb> queryOscBackground()
{
    int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
        return std::nullopt;
    }

    termios original{};
    if (tcgetattr(fd, &original) != 0) {
        ::close(fd);
        return std::nullopt;
    }

    termios modified = original;
    modified.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    modified.c_cc[VMIN] = 0;
    modified.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &modified) != 0) {
        ::close(fd);
        return std::nullopt;
    }

    const char query[] = "\x1b]11;?\x1b\\";
    if (::write(fd, query, sizeof(query) - 1) < 0) {
        tcsetattr(fd, TCSANOW, &original);
        ::close(fd);
        return std::nullopt;
    }
    tcdrain(fd);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (::select(fd + 1, &read_fds, nullptr, nullptr, &timeout) <= 0) {
        tcsetattr(fd, TCSANOW, &original);
        ::close(fd);
        return std::nullopt;
    }

    std::string buffer;
    buffer.reserve(128);
    char ch = 0;
    while (true) {
        ssize_t n = ::read(fd, &ch, 1);
        if (n <= 0) {
            break;
        }
        buffer.push_back(ch);
        if (ch == '\a') {
            break;
        }
        if (buffer.size() >= 2 && buffer[buffer.size() - 2] == '\x1b' && buffer.back() == '\\') {
            break;
        }
        if (buffer.size() > 200) {
            break;
        }
    }

    tcsetattr(fd, TCSANOW, &original);
    ::close(fd);

    std::size_t pos = buffer.find("rgb:");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::string payload = buffer.substr(pos + 4);
    std::size_t terminator = payload.find_first_of("\a\x1b");
    if (terminator != std::string::npos) {
        payload = payload.substr(0, terminator);
    }

    std::array<std::string, 3> parts{};
    std::size_t start = 0;
    int count = 0;
    while (start < payload.size() && count < 3) {
        std::size_t slash = payload.find('/', start);
        std::string token = payload.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!token.empty()) {
            parts[static_cast<std::size_t>(count++)] = token;
        } else {
            ++count;
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    if (count < 3) {
        return std::nullopt;
    }

    auto parse = [](const std::string& token) -> int {
        if (token.size() >= 4) {
            return static_cast<int>(std::strtoul(token.substr(0, 4).c_str(), nullptr, 16) >> 8);
        }
        if (token.size() >= 2) {
            return static_cast<int>(std::strtoul(token.substr(0, 2).c_str(), nullptr, 16));
        }
        return 0;
    };

    Rgb result{
        parse(parts[0]),
        parse(parts[1]),
        parse(parts[2]),
    };
    return result;
}

std::string trim(const std::string& input)
{
    std::size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
        ++begin;
    }
    std::size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(begin, end - begin);
}

std::optional<std::string> runCommand(const char* command)
{
    FILE* pipe = ::popen(command, "r");
    if (!pipe) {
        return std::nullopt;
    }
    std::string output;
    char chunk[256];
    while (std::fgets(chunk, sizeof(chunk), pipe)) {
        output.append(chunk);
    }
    ::pclose(pipe);
    output = trim(output);
    if (output.empty()) {
        return std::nullopt;
    }
    return output;
}

Platform::SystemTheme detectDesktopPreference()
{
    if (auto out = runCommand("gdbus call --session --dest org.freedesktop.portal.Desktop --object-path /org/freedesktop/portal/desktop --method org.freedesktop.portal.Settings.Read org.freedesktop.appearance color-scheme")) {
        for (char ch : *out) {
            if (ch == '1') {
                return Platform::SystemTheme::Dark;
            }
            if (ch == '2') {
                return Platform::SystemTheme::Light;
            }
        }
    }

    if (auto out = runCommand("gsettings get org.gnome.desktop.interface color-scheme")) {
        std::string lowered = *out;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lowered.find("dark") != std::string::npos) {
            return Platform::SystemTheme::Dark;
        }
        if (lowered.find("light") != std::string::npos) {
            return Platform::SystemTheme::Light;
        }
    }

    return Platform::SystemTheme::Unknown;
}

#endif // !_WIN32
} // namespace

bool Platform::enableVirtualTerminal()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }

    if ((dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        return true;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
#else
    return true;
#endif
}

bool Platform::isOutputTerminal()
{
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) != 0) {
        return true;
    }

    DWORD file_type = GetFileType(handle);
    if (file_type == FILE_TYPE_UNKNOWN) {
        return false;
    }

    return file_type == FILE_TYPE_CHAR;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

int Platform::terminalWidth()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return (csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }
    return 80;
#else
    struct winsize w {
    };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

Platform::SystemTheme Platform::detectSystemTheme()
{
    // Allow explicit override
    if (const char* forced = std::getenv("NLS_THEME")) {
        if (auto parsed = parseThemeString(forced); parsed != SystemTheme::Unknown) {
            return parsed;
        }
    }

#ifdef _WIN32
    if (auto color_theme = detectFromColorFgbg(); color_theme != SystemTheme::Unknown) {
        return color_theme;
    }

    if (auto win_theme = detectFromWindowsPersonalization(); win_theme != SystemTheme::Unknown) {
        return win_theme;
    }

    return SystemTheme::Unknown;
#else
    if (auto rgb = queryOscBackground()) {
        return themeFromRgb(*rgb);
    }

    if (auto color_theme = detectFromColorFgbg(); color_theme != SystemTheme::Unknown) {
        return color_theme;
    }

    if (auto desktop = detectDesktopPreference(); desktop != SystemTheme::Unknown) {
        return desktop;
    }

    if (auto gtk_theme = detectFromGtkTheme(); gtk_theme != SystemTheme::Unknown) {
        return gtk_theme;
    }

    return SystemTheme::Unknown;
#endif
}

} // namespace nls
