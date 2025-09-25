#pragma once

#include "options.h"

namespace nls {

class CommandLineParser {
public:
    Options Parse(int argc, char** argv) const;
};

} // namespace nls
