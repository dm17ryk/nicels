# nicels

## Project overview and goals
nicels is a modern, cross-platform reimagining of `ls` that emphasises rich,
customisable output, first-class Git awareness, and predictable builds. The
executable is implemented in ISO C++23, uses [CLI11](https://github.com/CLIUtils/CLI11)
for its command-line interface, and links against libgit2 when Git status
information is requested. Icons, colour themes, file metadata, and Git porce-
lain state can all be surfaced in a single listing, while CMake presets keep
builds reproducible across Linux and Windows targets.【F:CMakeLists.txt†L39-L122】【F:src/git_status.cpp†L1-L116】

## Installation
### Linux
1. Install toolchain dependencies:
   ```sh
   sudo apt update
   sudo apt install git cmake ninja-build clang clang++ pkg-config libssl-dev
   ```
2. Clone the repository and fetch vendored dependencies:
   ```sh
   git clone https://github.com/dm17ryk/nicels.git
   cd nicels
   git submodule update --init --recursive
   ```

### Windows (MSYS2 UCRT)
1. Start a **UCRT64** shell and install the required packages:
   ```sh
   pacman -S --needed \
       git \
       mingw-w64-ucrt-x86_64-{git,cmake,clang,ninja,python,llvm,llvm-libs,lld,toolchain}
   ```
2. Clone the sources and initialise submodules:
   ```sh
   git clone https://github.com/dm17ryk/nicels.git
   cd nicels
   git submodule update --init --recursive
   ```

## Build, run, and test
The project ships CMake presets for single- and multi-config generators.
Configure once, then build whichever configuration you need.【F:BUILD_README.md†L18-L66】

### Configure
```sh
# Linux
cmake --preset linux-clang

# Windows / MSYS2 UCRT
cmake --preset msys-clang
```

### Build
```sh
# Linux
cmake --build --preset linux-clang-release
cmake --build --preset linux-clang-debug

# Windows / MSYS2 UCRT
cmake --build --preset msys-clang-release
cmake --build --preset msys-clang-debug
```

The `nls` executable appears under `build/<preset>/<config>/`. Run it directly
from the build tree or after `cmake --install` to stage an install tree under
`build/<preset>/install/`.【F:BUILD_README.md†L68-L118】

## Versioning

nls follows a four-component version scheme: `major.minor[.maintenance[.build]]`.
The canonical version is stored in the repository's `VERSION` file and surfaced
via `nls --version` at runtime.【F:VERSION†L1-L1】【F:src/command_line_parser.cpp†L413-L443】

* **Major/minor** numbers are updated manually whenever a breaking or notable
  change warrants it.
* **Maintenance** increments automatically after each pull request merge. The
  helper script `tools/update_version.py --bump-maintenance` applies the bump
  and resets the build counter.【F:tools/update_version.py†L1-L169】
* **Build** increments on release builds that ship new code. Use
  `tools/update_version.py --bump-build` and provide `--compare-ref <ref>` to
  guard against unintended bumps when no changes occurred compared to the
  previous release reference.【F:tools/update_version.py†L1-L169】

Both bump operations print the resulting version and rewrite `VERSION` unless
`--dry-run` is specified. The script enforces non-negative values and ensures
the file always contains four components, starting at `1.0.0.0`.

### Run
```sh
./build/linux-clang/Release/nls --help
./build/msys-clang/Release/nls.exe -laA
```

### Tests
CTest presets are provided even though no tests are defined yet:
```sh
ctest --preset linux-clang-test
ctest --preset msys-clang-test
```
You will see the informational message from CMake until tests are added.【F:CMakeLists.txt†L159-L170】

## Configuration files and themes
`nls` loads its colour themes and icon maps from YAML files. The search order is:

1. A directory specified via the `NLS_DATA_DIR` environment variable (highest priority).
2. `./yaml/` relative to the current working directory.
3. `yaml/` next to the executable and its parent directory.
4. System-wide defaults:
   - Linux/macOS: `/etc/dm17ryk/nicels/yaml`
   - Windows: the directory that contains the installed executable
5. Per-user overrides:
   - Linux/macOS: `~/.nicels/yaml`
   - Windows: `%APPDATA%\.nicels\yaml` (falling back to `%USERPROFILE%` if needed)

When both a global and a user YAML file exist for the same resource, `nls`
loads the global configuration first and then applies the user entries on top,
allowing per-user customisation without modifying the installation. If
`NLS_DATA_DIR` is set, files from that directory take precedence and suppress
user overrides for matching resources. Missing YAML files are silently ignored;
compiled-in defaults remain in effect.【F:src/resources.cpp†L9-L116】【F:src/theme.cpp†L360-L505】

Palette definitions (`colors.yaml`, `dark_theme.yaml`, and `light_theme.yaml`)
support per-user overrides for the existing keys so you can tweak individual
colours without copying the full files; new keys are ignored. Icon maps
(`files.yaml`, `folders.yaml`, and their alias files) accept additional entries
in user overrides, letting you introduce new file and folder icons or aliases
alongside the bundled defaults.

To create additional colour schemes, add a `<name>_theme.yaml` file to any of
the configuration directories above. Launch `nls` with `--theme=<name>` to load
it; the program reports an error and falls back to the default theme when the
named file is missing.

## CLI usage
### Quick start
`nls` mirrors the GNU `ls` workflow. Listing the current directory with Git
status and icons enabled looks like:
```sh
nls -laA --gs --header --report=long
```
Use `--color=auto` (default) to keep ANSI colours only when stdout is a
terminal, and `--no-icons` when running inside tools that strip Unicode.

### Keeping this section in sync
The CLI reference below is generated from the live binary. After changing
options in `src/command_line_parser.cpp`, rebuild `nls` and run:
```sh
python ./tools/update_cli_reference.py ./build/msys-clang/Release/nls.exe
```
Paste the output into the README to keep the documentation in lock-step with
what the executable reports.

#### Positionals

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `paths` | `PATH ...` | `—` | paths to list |

#### General

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-h, --help` | `—` | `—` | Print this help message and exit |
| `--version` | `—` | `—` | Display program version information and exit |

#### Layout options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-l, --long` | `—` | `—` | use a long listing format |
| `-1, --one-per-line` | `—` | `—` | list one file per line |
| `-x` | `—` | `—` | list entries by lines instead of by columns |
| `-C` | `—` | `—` | list entries by columns instead of by lines |
| `--format` | `WORD` | `vertical` | use format: across (-x), horizontal (-x), long (-l), single-column (-1), vertical (-C) or comma (-m) (default: vertical) |
| `--header` | `—` | `—` | print directory header and column names in long listing |
| `-m` | `—` | `—` | fill width with a comma separated list of entries |
| `-T, --tabsize` | `COLS` | `—` | assume tab stops at each COLS instead of 8 |
| `-w, --width` | `COLS` | `—` | set output width to COLS. 0 means no limit |
| `--tree{0}` | `—` | `=DEPTH` | show tree view of directories, optionally limited to DEPTH (0 for unlimited) |
| `--report{long}` | `—` | `=WORD` | show summary report: short, long (default: long) |
| `--zero` | `—` | `—` | end each output line with NUL, not newline |

#### Filtering options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-a, --all` | `—` | `—` | do not ignore entries starting with . |
| `-A, --almost-all` | `—` | `—` | do not list . and .. |
| `-d, --dirs` | `—` | `—` | show only directories |
| `-f, --files` | `—` | `—` | show only files |
| `-B, --ignore-backups` | `—` | `—` | do not list implied entries ending with ~ |
| `--hide` | `PATTERN ...` | `—` | do not list implied entries matching shell PATTERN (overridden by -a or -A) |
| `-I, --ignore` | `PATTERN ...` | `—` | do not list implied entries matching shell PATTERN |

#### Sorting options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-t` | `—` | `—` | sort by modification time, newest first |
| `-S` | `—` | `—` | sort by file size, largest first |
| `-X` | `—` | `—` | sort by file extension |
| `-U` | `—` | `—` | do not sort; list entries in directory order |
| `-r, --reverse` | `—` | `—` | reverse order while sorting |
| `--sort` | `WORD` | `name` | sort by WORD instead of name: none, size, time, extension (default: name) |
| `--sd, --sort-dirs, --group-directories-first` | `—` | `—` | sort directories before files |
| `--sf, --sort-files` | `—` | `—` | sort files first |
| `--df, --dots-first` | `—` | `—` | sort dot-files and dot-folders first |

#### Appearance options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-b, --escape` | `—` | `—` | print C-style escapes for nongraphic characters |
| `-N, --literal` | `—` | `—` | print entry names without quoting |
| `-Q, --quote-name` | `—` | `—` | enclose entry names in double quotes |
| `--quoting-style` | `WORD` | `literal` | use quoting style WORD for entry names: literal, locale, shell, shell-always, shell-escape, shell-escape-always, c, escape (default: literal) |
| `-p` | `—` | `—` | append / indicator to directories |
| `--indicator-style` | `STYLE` | `slash` | append indicator with style STYLE to entry names: none, slash (-p) (default: slash) |
| `--no-icons, --without-icons` | `—` | `—` | disable icons in output |
| `--no-color` | `—` | `—` | disable ANSI colors |
| `--color` | `WHEN` | `auto` | colorize the output: auto, always, never (default: auto) |
| `--theme` | `NAME` | `—` | use theme NAME (loads NAME_theme.yaml) |
| `--light` | `—` | `—` | use light color scheme |
| `--dark` | `—` | `—` | use dark color scheme |
| `-q, --hide-control-chars` | `—` | `—` | print ? instead of nongraphic characters |
| `--show-control-chars` | `—` | `—` | show nongraphic characters as-is |
| `--time-style` | `FORMAT` | `local` | use time display format: default, locale, local, long-iso, full-iso, iso, iso8601, FORMAT (default: local) |
| `--full-time` | `—` | `—` | like -l --time-style=full-iso |
| `--hyperlink` | `—` | `—` | emit hyperlinks for entries |

#### Information options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `-i, --inode` | `—` | `—` | show inode number |
| `-o` | `—` | `—` | use a long listing format without group information |
| `-g` | `—` | `—` | use a long listing format without owner information |
| `-G, --no-group` | `—` | `—` | show no group information in a long listing |
| `-n, --numeric-uid-gid` | `—` | `—` | like -l, but list numeric user and group IDs |
| `--bytes, --non-human-readable` | `—` | `—` | show file sizes in bytes |
| `-s, --size` | `—` | `—` | print the allocated size of each file, in blocks |
| `--block-size` | `SIZE` | `—` | with -l, scale sizes by SIZE when printing them |
| `-L, --dereference` | `—` | `—` | when showing file information for a symbolic link, show information for the file the link references |
| `--gs, --git-status` | `—` | `—` | show git status for each file |

#### Debug options

| Option(s) | Argument | Default | Description |
| --- | --- | --- | --- |
| `--perf-debug` | `—` | `—` | enable performance diagnostics |

**Footnotes and related behaviour**
- `SIZE` accepts optional binary (K, M, …) or decimal (KB, MB, …) suffixes.
- `TIME_STYLE` values mirror `date(1)` and honour the `TIME_STYLE` environment variable.
- `WHEN` defaults to `always`; set `LS_COLORS` or `dircolors(1)` to refine colour palettes.
- Exit status: `0` (success), `1` (minor issues), `2` (serious trouble).
- Related commands: `date(1)` and `dircolors(1)`.

## Performance notes and Git status tuning
- `--gs/--git-status` delegates to libgit2. Disable it for large trees or build
  without libgit2 by configuring with `-DNLS_ENABLE_LIBGIT2=OFF`.
- Long listings render faster with `--no-icons`, `--color=never`, or a narrower
  `--report short` summary when scripting.
- On Windows presets, the link options statically link libgcc/libstdc++/winpthread
  so the produced `nls.exe` is self-contained.【F:CMakeLists.txt†L109-L149】
- Theme YAML files under `yaml/` hold light/dark palettes. Copy the entries you
  want to adjust into your per-user directory to change colours, and add new
  icon or alias rows to extend icon coverage.【F:yaml/light_theme.yaml†L1-L200】

## ASCII screenshots
### Linux (ANSI colours with inode, owner/group, Git status)
```
Directory: ␛[38;2;65;105;225m/workspace/nicels␛[0m

␛[38;2;211;211;211m  Inode␛[0m ␛[38;2;211;211;211mMode      ␛[0m ␛[38;2;211;211;211mLinks␛[0m ␛[38;2;211;211;211mOwner␛[0m ␛[38;2;211;211;211mGroup␛[0m ␛[38;2;211;211;211m   Size␛[0m ␛[38;2;211;211;211mLastWriteTime           ␛[0m ␛[38;2;211;211;211mGit␛[0m ␛[38;2;211;211;211mName␛[0m
------- ---------- ----- ----- ----- ------- ------------------------ --- ----
␛[38;2;255;228;181m1441818␛[0m ␛[38;2;30;144;255md␛[0m␛[38;2;50;205;50mr␛[0m␛[38;2;189;183;107mw␛[0m␛[38;2;255;0;0mx␛[0m␛[38;2;50;205;50mr␛[0m-␛[38;2;255;0;0mx␛[0m␛[38;2;50;205;50mr␛[0m-␛[38;2;255;0;0mx␛[0m ␛[38;2;255;228;181m    5␛[0m ␛[38;2;255;228;181mroot ␛[0m ␛[38;2;189;183;107mroot ␛[0m ␛[38;2;255;218;185m    0 B␛[0m ␛[38;2;0;255;0mSat Sep 27 23:19:06 2025␛[0m     ␛[38;2;30;144;255mbuild/␛[0m
␛[38;2;255;228;181m1442005␛[0m -␛[38;2;50;205;50mr␛[0m␛[38;2;189;183;107mw␛[0m-␛[38;2;50;205;50mr␛[0m--␛[38;2;50;205;50mr␛[0m-- ␛[38;2;255;228;181m    1␛[0m ␛[38;2;255;228;181mroot ␛[0m ␛[38;2;189;183;107mroot ␛[0m ␛[38;2;255;218;185m3.5 KiB␛[0m ␛[38;2;0;255;0mSat Sep 27 23:17:05 2025␛[0m     ␛[38;2;255;255;0mBUILD_README.md␛[0m
```
![Linux long listing](images/Linux-alAL_2025-10-02.png)

### Windows (MSYS2 UCRT example)
```
Directory: ␛[38;2;65;105;225mC:\\Users\\Dev\\source\\nicels␛[0m

␛[38;2;211;211;211m  Inode␛[0m ␛[38;2;211;211;211mMode      ␛[0m ␛[38;2;211;211;211mLinks␛[0m ␛[38;2;211;211;211mOwner         ␛[0m ␛[38;2;211;211;211mGroup         ␛[0m ␛[38;2;211;211;211m   Size␛[0m ␛[38;2;211;211;211mLastWriteTime           ␛[0m ␛[38;2;211;211;211mGit␛[0m ␛[38;2;211;211;211mName␛[0m
------- ---------- ----- -------------- -------------- ------- ------------------------ --- -----------------
␛[38;2;255;228;181m000000␛[0m ␛[38;2;30;144;255md␛[0m␛[38;2;50;205;50mr␛[0m␛[38;2;189;183;107mw␛[0m␛[38;2;255;0;0mx␛[0m␛[38;2;50;205;50mr␛[0m-␛[38;2;255;0;0mx␛[0m␛[38;2;50;205;50mr␛[0m-␛[38;2;255;0;0mx␛[0m ␛[38;2;255;228;181m    5␛[0m ␛[38;2;255;228;181mDEV\\Dev      ␛[0m ␛[38;2;189;183;107mDEV\\None     ␛[0m ␛[38;2;255;218;185m    0 B␛[0m ␛[38;2;0;255;0mSat Sep 27 23:19:06 2025␛[0m     ␛[38;2;30;144;255msrc\\␛[0m
␛[38;2;255;228;181m000000␛[0m -␛[38;2;50;205;50mr␛[0m␛[38;2;189;183;107mw␛[0m-␛[38;2;50;205;50mr␛[0m--␛[38;2;50;205;50mr␛[0m-- ␛[38;2;255;228;181m    1␛[0m ␛[38;2;255;228;181mDEV\\Dev      ␛[0m ␛[38;2;189;183;107mDEV\\None     ␛[0m ␛[38;2;255;218;185m6.8 KiB␛[0m ␛[38;2;0;255;0mSat Sep 27 23:17:05 2025␛[0m     ␛[38;2;255;255;0mCMakeLists.txt␛[0m
```
![Windows long listing](images/Windows-alAL_2025-10-02.png)

## Packaging and artifacts
Binary packaging is not configured yet. Use `cmake --install` to stage files for
archiving or distribution; the install target copies headers to
`include/nls/` and the executable to `bin/`.【F:CMakeLists.txt†L151-L156】
