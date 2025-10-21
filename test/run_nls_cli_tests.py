#!/usr/bin/env python3
"""Exercise the nls CLI against the generated test fixtures."""

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import sqlite3
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Iterable, Optional


REPO_ROOT = Path(__file__).resolve().parent.parent


@dataclass
class TestCase:
    name: str
    args: list[str]
    env: Optional[Dict[str, str]] = None
    cwd: Optional[Path] = None
    verify: Optional[Callable[[Path, Path], Optional[str]]] = None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the nls CLI test suite across the fixture directory tree.",
    )
    parser.add_argument(
        "--binary",
        default="build/nls",
        help="Path to the nls executable under test (default: build/nls).",
    )
    parser.add_argument(
        "--fixtures",
        default=str(REPO_ROOT / "test"),
        help="Directory where the fixture tree should be materialised (default: %(default)s)",
    )
    parser.add_argument(
        "--platform",
        choices=("auto", "linux", "windows", "all"),
        default="auto",
        help="Fixture set to unpack before running the checks (default: auto)",
    )
    return parser.parse_args()


def ensure_binary(path: Path) -> Path:
    def _is_executable(candidate: Path) -> bool:
        return candidate.is_file() and os.access(candidate, os.X_OK)

    if _is_executable(path):
        return path

    fallback_candidates: list[Path] = []
    if path.suffix == "" and os.name == "nt":
        fallback_candidates.append(path.with_suffix(".exe"))

    search_roots: list[Path] = []
    if path.parent != path:
        search_roots.append(path.parent)
    search_roots.append(REPO_ROOT / "build")

    names: list[str] = ["nls"]
    if os.name == "nt":
        names.insert(0, "nls.exe")

    for candidate in fallback_candidates:
        if _is_executable(candidate):
            return candidate

    for root in search_roots:
        if not root.exists():
            continue
        for name in names:
            try:
                for candidate in root.rglob(name):
                    if _is_executable(candidate):
                        return candidate
            except PermissionError:
                continue

    raise FileNotFoundError(f"nls binary not found at {path}")


def generate_fixtures(destination: Path, platform_choice: str) -> None:
    script = REPO_ROOT / "tools" / "generate_test_structure.py"
    cmd = [
        sys.executable,
        str(script),
        str(destination),
        "--platform",
        platform_choice,
        "--force",
    ]
    subprocess.run(cmd, check=True, cwd=REPO_ROOT)


def _default_env() -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("LC_ALL", "C.UTF-8")
    env.setdefault("LANG", "C.UTF-8")
    return env


def resolve_platform_choice(selection: str) -> str:
    if selection != "auto":
        return selection
    system = platform.system().lower()
    if "windows" in system:
        return "windows"
    return "linux"


def build_cases(fixture_dir: Path, root_dir: Path) -> list[TestCase]:
    if not root_dir.is_dir():
        raise RuntimeError(f"fixture root {root_dir} is missing")

    cases: list[TestCase] = []
    env = _default_env()
    cwd = REPO_ROOT

    def add(
        name: str,
        *args: str,
        case_env: Optional[Dict[str, str]] = None,
        case_cwd: Optional[Path] = None,
        verify: Optional[Callable[[Path, Path], Optional[str]]] = None,
    ) -> None:
        args_list = list(args)
        if case_env is None:
            merged_env = env
        else:
            merged_env = env.copy()
            merged_env.update(case_env)
        cases.append(TestCase(name=name, args=args_list, env=merged_env, cwd=case_cwd or cwd, verify=verify))

    db_path = REPO_ROOT / "DB" / "NLS.sqlite3"
    if not db_path.exists():
        raise RuntimeError(f"configuration database not found at {db_path}")

    reset_code = "\x1b[0m"

    def _ansi_from_rgb(value: int) -> str:
        rgb = value & 0xFFFFFF
        r = (rgb >> 16) & 0xFF
        g = (rgb >> 8) & 0xFF
        b = rgb & 0xFF
        return f"\x1b[38;2;{r};{g};{b}m"

    with sqlite3.connect(db_path) as conn:
        cursor = conn.cursor()
        theme_colors = {
            row[0]: _ansi_from_rgb(int(row[1]))
            for row in cursor.execute(
                "SELECT lower(t.element), c.value FROM Theme_colors t "
                "JOIN Colors c ON t.color_id = c.id WHERE t.id = 1"
            )
        }
        file_icons = {
            row[0]: row[1]
            for row in cursor.execute("SELECT lower(name), icon FROM Files WHERE icon IS NOT NULL")
        }
        folder_icons = {
            row[0]: row[1]
            for row in cursor.execute("SELECT lower(name), icon FROM Folders WHERE icon IS NOT NULL")
        }

    fallback_file_icon = file_icons.get("file", "\uf15b")
    fallback_folder_icon = folder_icons.get("folder", "\uf07b")

    def expect_colored_entry(
        text: str,
        *,
        name: str,
        icon_key: str,
        color_key: str,
        is_dir: bool = False,
    ) -> Optional[str]:
        icon_map = folder_icons if is_dir else file_icons
        fallback_icon = fallback_folder_icon if is_dir else fallback_file_icon
        icon = icon_map.get(icon_key.lower(), fallback_icon)
        if not icon:
            return f"expected icon for key '{icon_key}'"
        color = theme_colors.get(color_key.lower())
        if not color:
            return f"expected theme color '{color_key}'"
        target_name = f"{name}/" if is_dir else name
        expected = f"{color}{icon} {target_name}{reset_code}"
        if expected not in text:
            return f"expected '{expected}' in stdout"
        return None

    def verify_linux_basic_colors(out_path: Path, _: Path) -> Optional[str]:
        text = out_path.read_text(encoding="utf-8", errors="replace")
        checks = [
            ("readme.txt", "txt", "recognized_file", False),
            ("random.bin", "bin", "recognized_file", False),
            ("mixed.case", "file", "unrecognized_file", False),
        ]
        for name, icon_key, color_key, is_dir in checks:
            message = expect_colored_entry(
                text,
                name=name,
                icon_key=icon_key,
                color_key=color_key,
                is_dir=is_dir,
            )
            if message:
                return message
        return None

    def verify_linux_directory_colors(out_path: Path, _: Path) -> Optional[str]:
        text = out_path.read_text(encoding="utf-8", errors="replace")
        return expect_colored_entry(
            text,
            name="basic",
            icon_key="folder",
            color_key="dir",
            is_dir=True,
        )

    def verify_git_status_colors(out_path: Path, _: Path) -> Optional[str]:
        text = out_path.read_text(encoding="utf-8", errors="replace")
        mod_color = theme_colors.get("modification")
        untracked_color = theme_colors.get("untracked")
        if not mod_color or not untracked_color:
            return "expected git status colors in theme"
        if f"{mod_color}M{reset_code}" not in text:
            return "expected colored modified status 'M'"
        if f"{untracked_color}?{reset_code}" not in text:
            return "expected colored untracked status '?'"
        for filename in ("tracked.txt", "untracked.txt"):
            message = expect_colored_entry(
                text,
                name=filename,
                icon_key="txt",
                color_key="recognized_file",
                is_dir=False,
            )
            if message:
                return message
        return None

    data_dir_env = {"NLS_DATA_DIR": str(db_path.parent)}
    linux_root = fixture_dir / "lin"
    if not linux_root.is_dir():
        raise RuntimeError(f"expected Linux fixtures at {linux_root}")
    linux_basic = linux_root / "basic"
    git_repo = linux_root / "git_repo"
    if not linux_basic.is_dir():
        raise RuntimeError(f"expected basic fixture directory at {linux_basic}")
    if not git_repo.is_dir():
        raise RuntimeError(f"expected git repo fixture at {git_repo}")

    add(
        "db-colors-icons-basic",
        "--color=always",
        "-1",
        str(linux_basic),
        case_env=data_dir_env,
        verify=verify_linux_basic_colors,
    )
    add(
        "db-directory-colors",
        "--color=always",
        "-1",
        str(linux_root),
        case_env=data_dir_env,
        verify=verify_linux_directory_colors,
    )
    add(
        "db-git-status-colors",
        "--color=always",
        "--git-status",
        "-l",
        str(git_repo),
        case_env=data_dir_env,
        verify=verify_git_status_colors,
    )

    # General behaviour.
    add("help", "--help")
    add("version", "--version")

    add("default-listing", str(root_dir))

    # Layout options.
    add("long-format", "-l", str(root_dir))
    add("one-per-line", "-1", str(root_dir))
    add("list-by-lines", "-x", str(root_dir))
    add("list-by-columns", "-C", str(root_dir))
    for format_opt in ("vertical", "across", "horizontal", "long", "single-column", "comma"):
        add(f"format-{format_opt}", f"--format={format_opt}", str(root_dir))
    add("header", "--header", str(root_dir))
    add("comma-separated", "-m", str(root_dir))
    add("tabsize", "-T", "4", str(root_dir))
    add("width", "-w", "120", str(root_dir))
    add("tree", "--tree", str(root_dir))
    add("tree-depth", "--tree=2", str(root_dir))
    for report_opt in ("short", "long"):
        add(f"report-{report_opt}", f"--report={report_opt}", str(root_dir))
    add("zero-terminated", "--zero", str(root_dir))

    # Filtering options.
    add("all", "-a", str(root_dir))
    add("almost-all", "-A", str(root_dir))
    add("dirs-only", "-d", str(root_dir))
    add("files-only", "-f", str(root_dir))
    add("ignore-backups", "-B", str(root_dir))
    add("hide-pattern", "--hide", "*.log", str(root_dir))
    add("ignore-pattern", "-I", "*.bin", str(root_dir))

    # Sorting options.
    add("sort-mod-time", "-t", str(root_dir))
    add("sort-size", "-S", str(root_dir))
    add("sort-extension", "-X", str(root_dir))
    add("unsorted", "-U", str(root_dir))
    add("reverse", "-r", str(root_dir))
    for sort_opt in ("size", "time", "extension", "none"):
        add(f"sort-by-{sort_opt}", "--sort", sort_opt, str(root_dir))
    add("group-directories-first", "--group-directories-first", str(root_dir))
    add("sort-files-first", "--sort-files", str(root_dir))
    add("dots-first", "--dots-first", str(root_dir))

    # Appearance options.
    add("escape", "-b", str(root_dir))
    add("literal", "-N", str(root_dir))
    add("quote-name", "-Q", str(root_dir))
    quoting_styles: Iterable[str] = (
        "literal",
        "locale",
        "shell",
        "shell-always",
        "shell-escape",
        "shell-escape-always",
        "c",
        "escape",
    )
    for style in quoting_styles:
        add(f"quoting-style-{style}", "--quoting-style", style, str(root_dir))
    add("append-indicator", "-p", str(root_dir))
    add("indicator-none", "--indicator-style", "none", str(root_dir))
    add("indicator-slash", "--indicator-style", "slash", str(root_dir))
    add("no-icons", "--no-icons", str(root_dir))
    add("without-icons-alias", "--without-icons", str(root_dir))
    add("no-color", "--no-color", str(root_dir))
    for when in ("always", "auto", "never"):
        add(f"color-{when}", f"--color={when}", str(root_dir))
    add("theme-dark", "--theme", "Dark", str(root_dir))
    add("light-theme", "--light", str(root_dir))
    add("dark-theme", "--dark", str(root_dir))
    add("hide-control", "-q", str(root_dir))
    add("show-control", "--show-control-chars", str(root_dir))
    for time_style in ("default", "locale", "local", "long-iso", "full-iso", "iso", "iso8601"):
        add(f"time-style-{time_style}", "--time-style", time_style, str(root_dir))
    add("full-time", "--full-time", str(root_dir))
    add("hyperlink", "--hyperlink", str(root_dir))

    # Information options.
    add("inode", "-i", str(root_dir))
    add("no-owner", "-o", str(root_dir))
    add("no-group-long", "-g", str(root_dir))
    add("no-group-option", "-G", str(root_dir))
    add("numeric-ids", "-n", str(root_dir))
    add("bytes", "--bytes", str(root_dir))
    add("size", "--size", str(root_dir))
    add("block-size", "--block-size", "1K", str(root_dir))
    add("dereference", "-L", str(root_dir))
    add("git-status", "--git-status", str(root_dir))

    # Debug / diagnostics.
    add("perf-debug", "--perf-debug", str(root_dir))

    # Subcommands.
    add("db-help", "db", "--help")

    # Representative option combinations.
    add("combo-long-all-git", "-l", "-a", "--git-status", "--header", str(root_dir))
    add("combo-sorting", "-S", "-t", "--reverse", str(root_dir))
    add("combo-format-tree", "--format", "long", "--tree=1", str(root_dir))
    add("combo-themed", "-l", "--color=always", "--theme", "Dark", str(root_dir))
    add("combo-filtering", "-A", "--hide", "*.log", "--ignore", "*.bin", str(root_dir))
    add("combo-quoting", "-Q", "--quoting-style", "shell-escape", str(root_dir))
    add("combo-numeric", "-n", "--bytes", "--block-size", "1M", str(root_dir))
    add("combo-appearance", "--no-icons", "--no-color", "--hyperlink", str(root_dir))
    add("combo-information", "-i", "-o", "-g", "-G", "--size", str(root_dir))

    # Database subcommand exercises.
    db_home = fixture_dir / "db-home"
    if db_home.exists():
        shutil.rmtree(db_home)
    db_home.mkdir(parents=True, exist_ok=True)

    db_env = env.copy()
    db_env.update({
        "HOME": str(db_home),
        "XDG_CONFIG_HOME": str(db_home / ".config"),
    })
    if os.name == "nt":
        appdata = db_home / "AppData"
        userprofile = db_home / "UserProfile"
        db_env.setdefault("APPDATA", str(appdata))
        db_env.setdefault("USERPROFILE", str(userprofile))
        user_db_dir = Path(db_env["APPDATA"]) / "nicels" / "DB"
    else:
        user_db_dir = Path(db_env["HOME"]) / ".nicels" / "DB"

    user_db_path = user_db_dir / "NLS.sqlite3"
    set_ext = ".fixturedb"
    alias_name = "fixture-alias"
    folder_name = "FixtureFolder"
    folder_alias = "fixture-folder-alias"

    def make_verify_db_update(target: str, name: str) -> Callable[[Path, Path], Optional[str]]:
        expected_phrase = f"updated {target} icon entry '{name}'"

        def _verify(out_path: Path, _: Path) -> Optional[str]:
            text = out_path.read_text(encoding="utf-8", errors="replace")
            if expected_phrase not in text:
                return f"expected '{expected_phrase}' in stdout"
            if not user_db_path.exists():
                return f"expected {user_db_path} to be created"
            return None

        return _verify

    def expect_in_stdout(substring: str) -> Callable[[Path, Path], Optional[str]]:
        def _check(out_path: Path, _: Path) -> Optional[str]:
            text = out_path.read_text(encoding="utf-8", errors="replace")
            if substring not in text:
                return f"expected '{substring}' in stdout"
            return None
        return _check

    add("db-show-files", "db", "--show-files", case_env=db_env)
    add(
        "db-set-file",
        "db",
        "--set-file",
        "--name",
        set_ext,
        "--icon",
        "@",
        "--icon_class",
        "test",
        "--icon_utf_16_codes",
        "\\u2605",
        "--icon_hex_code",
        "0x2605",
        "--used_by",
        "cli-tests",
        "--description",
        "Fixture entry",
        case_env=db_env,
        verify=make_verify_db_update("file", set_ext),
    )
    add(
        "db-show-files-after-set",
        "db",
        "--show-files",
        case_env=db_env,
        verify=expect_in_stdout(set_ext),
    )
    add(
        "db-show-folders",
        "db",
        "--show-folders",
        case_env=db_env,
    )
    add(
        "db-set-folder",
        "db",
        "--set-folder",
        "--name",
        folder_name,
        "--icon",
        "&",
        "--icon_class",
        "test-folder",
        "--icon_utf_16_codes",
        "\\u2606",
        "--icon_hex_code",
        "0x2606",
        "--used_by",
        "cli-tests",
        "--description",
        "Folder fixture entry",
        case_env=db_env,
        verify=make_verify_db_update("folder", folder_name),
    )
    add(
        "db-show-folders-after-set",
        "db",
        "--show-folders",
        case_env=db_env,
        verify=expect_in_stdout(folder_name.lower()),
    )
    add(
        "db-set-file-alias",
        "db",
        "--set-file-aliases",
        "--name",
        set_ext,
        "--alias",
        alias_name,
        case_env=db_env,
        verify=expect_in_stdout(f"updated file alias '{alias_name}'"),
    )
    add(
        "db-set-folder-alias",
        "db",
        "--set-folder-aliases",
        "--name",
        folder_name,
        "--alias",
        folder_alias,
        case_env=db_env,
        verify=expect_in_stdout(f"updated folder alias '{folder_alias}'"),
    )
    add(
        "db-show-file-aliases",
        "db",
        "--show-file-aliases",
        case_env=db_env,
        verify=expect_in_stdout(alias_name),
    )
    add(
        "db-show-folder-aliases",
        "db",
        "--show-folder-aliases",
        case_env=db_env,
        verify=expect_in_stdout(folder_alias),
    )

    return cases


def run_cases(binary: Path, cases: list[TestCase], log_dir: Path) -> None:
    failures: list[str] = []
    for index, case in enumerate(cases, start=1):
        cmd = [str(binary), *case.args]
        step_label = f"[{index}/{len(cases)}] {case.name}"
        print(f"{step_label}: {' '.join(case.args)}")
        result = subprocess.run(
            cmd,
            cwd=case.cwd,
            env=case.env,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )

        out_file = log_dir / f"{index:03d}_{case.name}.stdout"
        err_file = log_dir / f"{index:03d}_{case.name}.stderr"
        out_file.write_text(result.stdout, encoding="utf-8", errors="replace")
        err_file.write_text(result.stderr, encoding="utf-8", errors="replace")

        if result.returncode != 0:
            failures.append(
                f"{step_label} failed with exit code {result.returncode} "
                f"(stdout: {out_file}, stderr: {err_file})",
            )
            continue

        if case.verify is not None:
            message = case.verify(out_file, err_file)
            if message:
                failures.append(
                    f"{step_label} verification failed: {message} "
                    f"(stdout: {out_file}, stderr: {err_file})",
                )

    if failures:
        raise RuntimeError("\n".join(failures))


def main() -> int:
    args = parse_args()
    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = (REPO_ROOT / binary).resolve()
    ensure_binary(binary)

    fixtures_root = Path(args.fixtures).expanduser().resolve()
    fixtures_root.mkdir(parents=True, exist_ok=True)

    platform_choice = resolve_platform_choice(args.platform)
    if args.platform == "linux":
        fixture_selection = "linux"
    else:
        fixture_selection = "all"
    generate_fixtures(fixtures_root, fixture_selection)

    log_dir = fixtures_root / "_logs"
    if log_dir.exists():
        shutil.rmtree(log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    if platform_choice == "windows":
        root_dir = fixtures_root / "win"
    else:
        root_dir = fixtures_root / "lin"

    cases = build_cases(fixtures_root, root_dir)
    try:
        run_cases(binary, cases, log_dir)
    except Exception as exc:  # pragma: no cover
        print(f"error: {exc}", file=sys.stderr)
        return 1

    db_home_cleanup = fixtures_root / "db-home"
    if db_home_cleanup.exists():
        shutil.rmtree(db_home_cleanup)

    print(f"\nAll {len(cases)} CLI checks passed. Logs stored in {log_dir}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
