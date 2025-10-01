#pragma once

namespace nls {

class Platform {
public:
    static bool enableVirtualTerminal();
    static bool isOutputTerminal();
    static int terminalWidth();
};

}  // namespace nls
