# Migration Notes

This release restructures **nicels** from a single translation unit with hand-rolled argument
parsing into a modular, modern C++23 application. The highlights:

- Introduced a layered architecture (`App`, `Cli`, `FilesystemScanner`, `Renderer`, `Theme`,
  `GitStatus`, `Platform`, and utility modules) with explicit dependency injection.
- Replaced ad-hoc option parsing with [CLI11](https://github.com/CLIUtils/CLI11) (vendored via
  `third-party/cli11`). The CLI surface is preserved and extended with Markdown export to keep
  documentation in sync.
- Implemented cross-platform ownership, inode, and terminal handling in `platform.cpp` while
  keeping global state limited to the `Logger` and `Config` singletons.
- Migrated the build to CMake with multi-config friendly presets, out-of-source object storage
  (`obj/<config>`) and binary drops in `bin/<config>`. Packaging is driven by CPack for `.deb`,
  `.rpm`, and `.msixbundle` artifacts.
- Added structured rendering pipeline featuring column, list, long-format, and tree views with
  icon and theme strategies.
- Added smoke-test scripts under `test/lin` and `test/win` that exercise the most common modes of
  operation without requiring external dependencies.

For developers moving from the previous Makefile build:

1. Initialise the CLI11 submodule (`git submodule update --init --recursive`).
2. Configure with one of the provided presets, e.g. `cmake --preset linux-release` or
   `cmake --preset windows-ucrt`.
3. Build via `cmake --build --preset linux-release` (or the corresponding Windows preset). The
   executable is produced under `bin/<config>` and automatically copied to the repository root.
4. Run the smoke suites from `test/lin/smoke.sh` or `test/win/smoke.ps1`.

The legacy source tree (`src/*.cpp` from the bootstrap) has been fully replaced. For reference,
all formatting helpers (size, time, permissions) now live under `include/nicels` and are tested
indirectly through the smoke scripts.
