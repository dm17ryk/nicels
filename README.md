# nicels

A modern, cross-platform, and colorful replacement for `ls` inspired by GNU `ls` and the Ruby
`colorls` gem. The project now embraces an object-oriented C++23 design with a modular architecture,
fast filesystem traversal, optional Git integration, and a CMake-based toolchain that targets both
Linux (glibc) and Windows (MSYS2 UCRT / clang).

## Quick start

```bash
# initialise submodules (CLI11 is vendored)
git submodule update --init --recursive

# configure and build (Linux example)
cmake --preset linux-release
cmake --build --preset linux-release

# run in the repo root
./nicels --help
```

Windows (MSYS2 UCRT + clang) uses the `windows-ucrt` preset:

```powershell
cmake --preset windows-ucrt
cmake --build --preset windows-release
```

The executable is placed under `bin/<Config>/nicels` and copied to the repository root after every
successful build. Object files live in `obj/<Config>` thanks to the supplied presets.

## Project layout

```
include/nicels/   → public headers for the modular subsystems
src/              → implementation of App, CLI, renderer, filesystem, git, etc.
third-party/      → CLI11 submodule
CMakePresets.json → ready-to-use presets for Linux and Windows builds
bin/<config>/     → build outputs
obj/<config>/     → CMake build directories (via presets)
test/lin, test/win→ platform-specific smoke scenarios
```

## Usage

```bash
nicels [options] [paths ...]
```

Key features:

- Column, single-column, long, and tree renderers with ANSI color and Nerd Font icon theming.
- Git status integration (runs `git status --porcelain` once per repository and caches results).
- Cross-platform owner/group resolution, inode display (POSIX), and terminal width detection.
- Markdown export of the CLI surface (`nicels --dump-cli-markdown`) for documentation automation.
- Optional hyperlink output, block-size formatting, and comprehensive sorting controls.

### CLI reference

The following table is generated from the CLI11 option tree (run `nicels --dump-cli-markdown` to
regenerate):

| Option | Description |
| --- | --- |
| ``-v,--verbose`` | Increase logging verbosity |
| ``--debug`` | Enable debug logging |
| ``-a,--all`` | Include directory entries whose names begin with a dot |
| ``-A,--almost-all`` | Like --all but ignore '.' and '..' |
| ``-d,--directory`` | List directories themselves, not their contents |
| ``-f,--files`` | List files only |
| ``--group-directories-first`` | Group directories before files |
| ``--group-directories-last`` | Group directories after files |
| ``-l`` | Use a long listing format |
| ``-1`` | List one file per line |
| ``-r,--reverse`` | Reverse order while sorting |
| ``-t`` | Sort by modification time |
| ``-S`` | Sort by file size |
| ``-X`` | Sort by file extension |
| ``--tree`` | Show a tree view |
| ``--tree-depth`` | Limit tree depth |
| ``--no-icons`` | Disable icons |
| ``--no-color`` | Disable ANSI colors |
| ``--color`` | Always enable ANSI colors |
| ``--git-status`` | Display git status prefixes when inside a repo |
| ``-I,--ignore-backups`` | Ignore files ending with ~ |
| ``--hyperlink`` | Print clickable hyperlinks when supported |
| ``--header`` | Print directory headers |
| ``--inode`` | Print inode numbers |
| ``--numeric-ids`` | List numeric user and group IDs |
| ``--dereference`` | Follow symbolic links |
| ``--hide-control-chars`` | Replace control characters with '?' |
| ``-0,--zero`` | End each output line with NUL, not newline |
| ``--block-size`` | Show block size instead of bytes |
| ``--width`` | Assume screen width |
| ``--time-style`` | Time display style |
| ``--hide`` | Glob of files to hide |
| ``--ignore`` | Glob of files to ignore |
| ``--dump-cli-markdown`` | Print CLI markdown documentation and exit |
| ``paths`` | Paths to list |

## ASCII screenshots

### Linux (long format + Git status, colors disabled for readability)

```
$ ./nicels -l --git-status --no-color test/lin
-rw-r--r--  1 runner runner   532B Jul 11 12:34 file
-rw-r--r--  1 runner runner   1.2K Jul 11 12:34 file.c
-rw-r--r--  1 runner runner   1.5K Jul 11 12:34 file.cpp
drwxr-xr-x  2 runner runner   4.0K Jul 11 12:34 folder1
└── 📁 folder1
    ├── 📄 nested.txt
    └── 📄 nested.md
```

### Windows (MSYS2 UCRT, tree view)

```
PS> .\nicels --tree --no-color --no-icons test\win
└── folder1
    ├── file.txt
    ├── file.cpp
    └── file.exe
```

(Colors and icons are shown when the terminal supports them; disable via `--no-color` / `--no-icons`.)

## Tests and validation

Two lightweight smoke harnesses demonstrate the common scenarios without depending on the host
filesystem:

- `test/lin/smoke.sh` – Bash script that exercises long and tree listings against sample fixtures.
- `test/win/smoke.ps1` – PowerShell script that mirrors the Linux run under Windows/MSYS2.

Run them after building to sanity-check the binaries:

```bash
./test/lin/smoke.sh
```

```powershell
pwsh ./test/win/smoke.ps1
```

## Packaging

CPack recipes are bundled with the build. Once configured, run:

```bash
cmake --build --preset linux-release --target package
```

This produces `.deb` and `.rpm` packages under the build directory and a `.msixbundle` on Windows
when using the `windows-ucrt` preset. Adjust `CPACK_MSIX_PACKAGE_PUBLISHER` in `CMakeLists.txt` to
match your certificate information before shipping a Windows package.

## Performance notes

- Filesystem scanning uses `std::filesystem::directory_iterator` with error codes to avoid
  exceptions in hot paths.
- Git status queries execute once per repository root and reuse cached porcelain output; disable via
  `--git-status` if you do not need it.
- Rendering avoids iostream synchronization with C I/O and provides simple column packing. Use the
  `--width` option to override automatic terminal width detection when scripting.

## Regenerating this document

Run `./nicels --dump-cli-markdown` and update the table above to keep the README aligned with the
executable. ASCII screenshots were captured from the fixtures under `test/lin` and `test/win`.
