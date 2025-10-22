#pragma once

namespace nls {

class Platform {
public:
    enum class SystemTheme { Unknown, Dark, Light };

    static bool enableVirtualTerminal();
    static bool isOutputTerminal();
    static int terminalWidth();
    static SystemTheme detectSystemTheme();
};

}  // namespace nls
