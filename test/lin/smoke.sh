#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build/linux-debug}"
cmake --preset linux-debug >/dev/null
cmake --build --preset linux-debug --parallel >/dev/null
"$BUILD_DIR/bin/Debug/nicels" --long --report=short "$@"
"$BUILD_DIR/bin/Debug/nicels" --tree --tree-depth=2
