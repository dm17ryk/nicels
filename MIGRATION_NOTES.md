# Migration Notes

This refactor rebuilds **nicels** from the original Makefile-based bootstrap into a modern, modular C++23 application.

## Highlights

- **Architecture**: Introduced `App`, `Config`, `Cli`, `FileSystemScanner`, `Renderer`, `Theme`, `GitStatusCache`, `Logger`, and `Perf` modules. The runtime flow is `App` → `Cli` → `FileSystemScanner` → `Renderer` with optional Git augmentation.
- **Modern C++**: All functionality now lives in namespaced classes, no global state beyond the `Config` and `Logger` singletons. Uses `<filesystem>`, `<format>`, `<chrono>`, and RAII utilities throughout.
- **Build system**: Replaced the Makefile with CMake (C++23, multi-config layouts, CLI11 submodule includes) and added presets for Linux and Windows (MSYS2 UCRT). Post-build step mirrors GNU `ls` by copying the binary to the repo root.
- **Packaging**: Enabled CPack generators for `.deb`, `.rpm`, and `.msixbundle` artifacts with baseline metadata.
- **Documentation**: CLI help is driven by `Cli::usage_markdown()`. Run `./nicels --dump-markdown` to regenerate the README options table.
- **Testing hooks**: Added `./test/lin` and `./test/win` placeholders for smoke scripts (to be filled with platform-specific runs).

## Follow-up Opportunities

- Integrate libgit2 as an optional backend for `GitStatusCache` to avoid invoking `git`.
- Expand icon/color themes by loading YAML resources similar to the legacy project.
- Improve width calculation using `std::ranges` and Unicode-aware `wcwidth` logic.
- Extend tree rendering to print directory headers when mixing files and directories in a single invocation.
