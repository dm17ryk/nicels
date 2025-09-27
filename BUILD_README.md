# Building nicels with CMake

This document describes the modern CMake workflow for the project.  The build
system is designed around clang/clang++, libgit2 bundled as a git submodule and
a header-only copy of CLI11.  Multi-configuration builds, out-of-source binary
trees and cross-compilation scenarios are supported via CMake presets.

## Prerequisites

1. **Clone the repository with submodules**
   ```sh
   git clone https://github.com/<your-org>/nicels.git
   cd nicels
   git submodule update --init --recursive
   ```
2. **Install required tools**
   * CMake ≥ 3.28 (the project is ready for CMake 4.x when available)
   * Ninja (for the `Ninja Multi-Config` generator used by the presets)
   * clang & clang++
   * A recent Python (optional, only for auxiliary scripts)

> **Tip:** On Windows/MSYS2 install `clang`, `ninja`, and `cmake` packages and
> run the commands from a MSYS or CLANG64 shell.

## Configure

Configure once using the appropriate preset for your host platform:

* **Linux**
  ```sh
  cmake --preset linux-clang
  ```
* **Windows / MSYS2**
  ```sh
  cmake --preset msys-clang
  ```

Each preset creates an out-of-source build tree under `build/<preset>/` and
prepares both `Debug` and `Release` configurations.

## Build

Select the configuration you want at build time.  For example:

```sh
cmake --build --preset linux-clang-debug
cmake --build --preset linux-clang-release
```

The resulting executable is written to `build/<preset>/<config>/nls`.

## Install (optional)

You can stage an install tree beneath the build directory:

```sh
cmake --build --preset linux-clang-release --target install
```

The files land in `build/<preset>/install/`.

## Running (optional)

From within the build tree you can run the tool directly:

```sh
build/linux-clang/Debug/nls --help
```

## Tests

The project does not yet have automated tests, but the `linux-clang-test`
preset keeps the workflow ready:

```sh
ctest --preset linux-clang-test
```

## Cross compilation

An ARM64 cross-compilation preset is provided as an example.  It assumes an
LLVM-based toolchain targeting `aarch64-linux-gnu` and optionally a sysroot.

```sh
cmake --preset linux-arm64-cross -DCMAKE_SYSROOT=/path/to/sysroot
cmake --build --preset linux-arm64-cross-release
```

Adjust the `CMAKE_SYSROOT` and other cache variables for your toolchain.
Additional presets/toolchain files can be added following the example in
`cmake/toolchains/linux-clang-aarch64.cmake`.

## Customisation

Useful cache toggles exposed by the top-level `CMakeLists.txt`:

* `-DNLS_ENABLE_LIBGIT2=OFF` to build without libgit2 (CLI functionality only)
* `-DNLS_ENABLE_IPO=OFF` to disable link-time optimisation
* `-DNLS_WARNINGS_AS_ERRORS=ON` to promote warnings to errors
* `-DLIBGIT2_ENABLE_SSH=libssh2` to force the libssh2 backend when the dependency is available

The bundled dependencies are configured via `find_package()` wrappers located in
`cmake/modules`.  They default to the vendored submodules to ensure hermetic
builds, but respect standard CMake cache variables if you need to override
behaviour.
