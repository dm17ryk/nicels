# nicels

`nicels` is a cross-platform, modern C++23 reimagining of the colorful `ls` experience. It renders icons, ANSI themes, and Git
status in a single pass without shelling out for each file. The new architecture embraces dependency injection, RAII, and
modular components so behavior stays deterministic and fast on Linux and Windows.

## Installation

### Linux (glibc)

```bash
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/bin/Release/nicels --help
```

### Windows (MSYS2 UCRT + clang)

```powershell
cmake --preset windows-ucrt-debug
cmake --build --preset windows-ucrt-release --target nicels
# Executable is staged in build/windows-ucrt/bin/Release and copied to repo root
```

## Building from source

The project targets C++23 and requires CMake ≥ 3.24 with a compiler that supports `<format>`, `<chrono>` calendar utilities, and
`std::filesystem`.

1. Update submodules: `git submodule update --init --recursive`
2. Configure with a preset (`linux-debug`, `linux-release`, `windows-ucrt-debug`)
3. Build: `cmake --build --preset <preset> --parallel`
4. The compiled binary is copied to the repository root (`nicels` / `nicels.exe`) and placed in `bin/<config>`.

## Running & testing

Run the executable against any directory:

```bash
./nicels --group-directories-first --gs
```

Smoke tests are provided per platform:

- Linux: `test/lin/smoke.sh`
- Windows: `test/win/smoke.ps1`

The scripts exercise colorless and tree outputs against the sample fixtures under `test/` and compare them with golden baselines
stored in `test/expected`. On success the temporary capture is removed; on failure it is kept for inspection. Refresh the
goldens by rerunning the commands shown below and copying the output into the corresponding file in `test/expected/`.

## Command line interface

The CLI is defined through `Cli` (CLI11) and can emit Markdown for documentation:

```bash
./nicels --dump-markdown
```

<!-- CLI_OPTIONS_BEGIN -->
| Option | Description | Default |
| ------ | ----------- | ------- |
| `-l,--long` | Use long listing format |  |
| `-1,--one-per-line` | List one entry per line |  |
| `-x` | List entries by lines instead of columns |  |
| `-C` | List entries in columns |  |
| `-m` | List entries separated by commas |  |
| `--header` | Show header row for long listings |  |
| `--tree` | Render directories as a tree |  |
| `--tree-depth` | Limit depth when using --tree |  |
| `--zero` | Terminate lines with NUL |  |
| `-a,--all` | Include dot files |  |
| `-A,--almost-all` | Include dot files except . and .. |  |
| `-d,--dirs` | List directories only |  |
| `-f,--files` | List files only |  |
| `-B,--ignore-backups` | Ignore entries ending with ~ |  |
| `-t` | Sort by modification time |  |
| `-S` | Sort by size |  |
| `-X` | Sort by file extension |  |
| `-U` | Do not sort |  |
| `-r,--reverse` | Reverse order |  |
| `--group-directories-first,--sd,--sort-dirs` | Group directories before files |  |
| `--sf,--sort-files` | Place files before directories |  |
| `--df,--dots-first` | Place dot entries first |  |
| `--no-icons,--without-icons` | Disable icons |  |
| `--no-color` | Disable ANSI colors |  |
| `--light` | Use light color theme |  |
| `--dark` | Use dark color theme |  |
| `-q,--hide-control-chars` | Hide control characters in filenames |  |
| `--show-control-chars` | Show control characters |  |
| `-i,--inode` | Display inode numbers |  |
| `-o` | Do not show group |  |
| `-g` | Do not show owner |  |
| `-G,--no-group` | Do not show group |  |
| `-n,--numeric-uid-gid` | Show numeric IDs |  |
| `--bytes,--non-human-readable` | Show sizes in bytes |  |
| `-s,--size` | Show allocated blocks |  |
| `--gs,--git-status` | Include git status column |  |
| `--hyperlink` | Emit OSC 8 hyperlinks |  |
| `-L,--dereference` | Follow symlinks |  |
| `--width` | Override detected terminal width |  |
| `--tabsize` | Set tab size when aligning columns |  |
| `--time-style` | Specify time display style (full-iso, long-iso, iso, locale) |  |
| `--hide` | Hide entries matching glob pattern |  |
| `--ignore` | Ignore entries matching glob pattern |  |
| `--block-size` | Scale block sizes using suffix (K,M,G,Ki,Mi,...) |  |
| `paths` | Directories or files to display |  |
| `--dump-markdown` | Print CLI options as markdown and exit |  |

<!-- CLI_OPTIONS_END -->

## Performance notes

- Filesystem traversal batches `std::filesystem::directory_iterator` results and caches metadata to avoid repeated stats.
- Git status runs a single `git status --porcelain -z` per repository and caches the parsed output.
- Rendering avoids iostream sync by leaning on `std::format`, reusable buffers, and simple ANSI width detection.
- Use `--no-gs` to disable Git integration and `--no-icons`/`--no-color` for pure text output when targeting remote consoles.

## ASCII screenshots

### Linux

```
nicels --group-directories-first test/lin
```

```
folder1                      file.txt
file -> folder/file          file.xml
file.c                       folder3 -> folder1/folder3/
file.cpp                     smoke.sh
file.out
```

### Windows

```
nicels.exe --group-directories-first test\win
```

```
folder1       folder4       file.exe      file.txt
folder2       file.cpp      file.exe.lnk  smoke.ps1
```

## Packaging

After configuring a build, produce native packages with CPack:

```bash
cmake --build --preset linux-release --target package
cpack -G DEB -B dist --config build/linux-release/CPackConfig.cmake
cpack -G RPM -B dist --config build/linux-release/CPackConfig.cmake
```

On Windows UCRT, run:

```powershell
cmake --build --preset windows-ucrt-release --target package
cpack -G MSIX -B dist --config build/windows-ucrt/CPackConfig.cmake
```

## License

MIT
