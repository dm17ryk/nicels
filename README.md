# nls — NextLS (C++ port of ColorLS)

A fast, cross‑platform (`Linux` + `Windows`) C++ utility that emulates the Ruby **colorls** UX:
icons, colors, long listing, and (optionally) Git status — but without shelling out to `git`.

This is a **bootstrap**. It already works as a colorful, iconified `ls` with `-l`, `-1`, `-a`, `-A`,
sorting (`-t`, `-S`), grouping (`--group-directories-first`), and a **stub** for `--gs` (Git status).
You can build it with **make** on Linux and Windows (MSYS2 / Git Bash / MinGW).

> Roadmap: integrate libgit2 for real `--gs`; extend icon and color maps; add YAML config loader; tree view.

## Build

### Linux
```bash
make
./nls
```

### Windows (MSYS2 / Git Bash / MinGW)
```bash
make
./nls.exe
```

> If you want `--gs` to use libgit2, install libgit2 and build with:
> ```bash
> make USE_LIBGIT2=1
> ```
> On Windows, libgit2 may additionally require: `-lws2_32 -lcrypt32 -lwinhttp -lbcrypt -lssh2` depending on how the library was built.

## Usage

```bash
nls [options] [path ...]
```

Common options (subset, more coming):
- `-l` : long listing (permissions, owner/group, size, time)
- `-1` : one entry per line
- `-a` : include dotfiles
- `-A` : almost all (exclude `.` and `..`)
- `-t` : sort by mtime
- `-S` : sort by size
- `-r` : reverse order
- `--gs` : (stub) show git status prefix; compile with libgit2 for real status
- `--group-directories-first` : list directories before files
- `--no-icons` : disable icons
- `--no-color` : disable ANSI colors

## Notes

- Windows: the app enables ANSI support in the console at startup. Use Windows Terminal or a modern console.
- Icons require a Nerd Font (same as colorls). Set your terminal font accordingly or run with `--no-icons`.
- Column layout currently uses a simple printable-width heuristic; we’ll add wcwidth-based width later.
- Owner/group on Windows are placeholders in this bootstrap. POSIX owner/group shown on Linux.

## License

MIT (same spirit as colorls).
