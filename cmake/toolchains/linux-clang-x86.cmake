# SPDX-License-Identifier: MIT
# Toolchain file for cross-compiling nicels to Linux x86 (32-bit) using clang.
#
# Usage:
#   cmake --preset linux-x86-cross \
#         -DCMAKE_SYSROOT=/path/to/target/sysroot \
#         -DCMAKE_FIND_ROOT_PATH=/path/to/target/sysroot
#
# The preset will automatically reference this file.

set(CMAKE_SYSTEM_NAME Linux CACHE STRING "")
set(CMAKE_SYSTEM_PROCESSOR x86 CACHE STRING "")

set(CMAKE_C_COMPILER clang CACHE STRING "")
set(CMAKE_C_COMPILER_TARGET i686-linux-gnu CACHE STRING "")
set(CMAKE_CXX_COMPILER clang++ CACHE STRING "")
set(CMAKE_CXX_COMPILER_TARGET i686-linux-gnu CACHE STRING "")

set(CMAKE_AR llvm-ar CACHE STRING "")
set(CMAKE_RANLIB llvm-ranlib CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
