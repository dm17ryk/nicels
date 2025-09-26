# nicels

A modern, cross-platform reimagining of the classic `colorls`-style experience written in idiomatic C++23. The tool provides colorful, icon-rich directory listings, long-format metadata, optional tree views, and packaging recipes for Linux and Windows.

## Project overview and goals

- Pure C++23 implementation with zero global state (only minimal singletons for configuration/logging).
- Modular architecture (`App`, `Cli`, `Config`, `FileSystemScanner`, `Renderer`, `Theme`, `GitStatusCache`, `Logger`, `Platform`, `Formatter`, `Perf`).
- CLI compatibility with core GNU `ls` flags plus quality-of-life options from `colorls`.
- Multi-config CMake build with presets for Linux (Debug/Release) and Windows (MSYS2 UCRT clang++).
- Packaging via CPack into `.deb`, `.rpm`, and `.msix` bundles.

## Installation

### Linux

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
./build/linux-release/bin/Release/nicels --help
```

### Windows (MSYS2 UCRT + clang++)

```bash
cmake --preset windows-ucrt-release
cmake --build --preset windows-ucrt-release --parallel
./build/windows-release/bin/Release/nicels.exe --help
```

Packaging examples:

```bash
# Debian / Ubuntu
cpack -G DEB --config build/linux-release/CPackConfig.cmake
sudo dpkg -i build/linux-release/nicels-*.deb

# Fedora / RHEL
cpack -G RPM --config build/linux-release/CPackConfig.cmake
sudo rpm -Uvh build/linux-release/nicels-*.rpm

# Windows (run from MSYS2 PowerShell)
cpack -G MSIX --config build/windows-release/CPackConfig.cmake
```

## Build, run, and test

- Build Debug: `cmake --preset linux-debug && cmake --build --preset linux-debug`
- Build Release: `cmake --preset linux-release && cmake --build --preset linux-release`
- Run smoke tests: see [`test/lin`](test/lin) and [`test/win`](test/win) for sample invocations.
- Generate README CLI section: `./nicels --help-markdown` (after building) and paste into the CLI section below.

## CLI usage

```
nicels [options] [paths...]
```

| Option | Description |
|--------|-------------|
| -V, --version | Print version information and exit |
| --help-markdown | Print CLI options in Markdown table format |
| -l, --long | Use a long listing format |
| -1 | List one entry per line |
| -a, --all | Include directory entries whose names begin with a dot (.) |
| -A, --almost-all | Include almost all entries, excluding '.' and '..' |
| -d, --dirs | List directories themselves, not their contents |
| -f, --files | List only files (omit directories) |
| -t, --time | Sort by modification time, newest first |
| -S, --size | Sort by file size, largest first |
| -r, --reverse | Reverse the order while sorting |
| --group-directories-first | Group directories before files |
| --no-icons | Disable icons in output |
| --no-color | Disable ANSI colors |
| --classify | Append indicator (one of */=>@|) to entries |
| --no-control-char-filter | Show control characters rather than replacing them |
| --tree | Display directories as a tree |
| --tree-depth | Limit tree depth (requires --tree) |
| --git-status | Control git status retrieval (auto, always, never) |
| --report | Emit a summary report (values: short, long) |
| --time-style | Control timestamp formatting (default, iso, long-iso, full-iso) |
| --size-style | Control size formatting (binary, si) |
| --hyperlink | Emit hyperlinks for supported terminals |
| --locale | Override locale (LANG style) |
| -v, --log-level | Set log verbosity |

## ASCII screenshots

### Linux (dark theme)

```
📁 src          drwxr-xr-x user group  4.0K 2024-05-12 12:34
📄 README.md    -rw-r--r-- user group  3.2K 2024-05-12 12:34
📄 CMakeLists   -rw-r--r-- user group  1.8K 2024-05-12 12:34
```

### Windows (MSYS2 UCRT)

```
📁 src          drwxr-xr-x Domain\User 4.0K 2024-05-12 12:34
📄 README.md    -rw-r--r-- Domain\User 3.2K 2024-05-12 12:34
📄 CMakeLists   -rw-r--r-- Domain\User 1.8K 2024-05-12 12:34
```

## Performance notes and Git status tuning

- `GitStatusCache` is designed for a future libgit2-backed implementation; currently it can be toggled with `--git-status`.
- Use `--size-style=si` or `--size-style=binary` to control human-readable formatting.
- `--tree-depth` limits recursion to keep directory traversal fast.
- Build in Release (`cmake --preset linux-release`) for production workloads.

## Packaging and artifacts

- CPack generators: `.deb`, `.rpm`, `.msix`.
- Install the MSIX by double-clicking on Windows 11 with developer mode enabled.
- Linux packages declare dependencies on `git`, `libstdc++6`, and `libgcc-s1`.

## Tests and samples

Sample usage scripts live in:

- [`test/lin`](test/lin)
- [`test/win`](test/win)

Each directory contains example invocations that exercise long listing, tree mode, color toggles, and summary reports.
