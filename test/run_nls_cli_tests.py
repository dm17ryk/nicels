#!/usr/bin/env python3
"""Exercise the nls CLI against the generated test fixtures."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, Optional


REPO_ROOT = Path(__file__).resolve().parent.parent


@dataclass
class TestCase:
    name: str
    args: list[str]
    env: Optional[Dict[str, str]] = None
    cwd: Optional[Path] = None


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
        help="Existing fixtures directory to reuse. "
        "If omitted, fixtures are generated in a temporary directory.",
    )
    parser.add_argument(
        "--keep-fixtures",
        action="store_true",
        help="Do not delete generated fixtures after the run (useful for debugging).",
    )
    return parser.parse_args()


def ensure_binary(path: Path) -> Path:
    if not path.exists():
        raise FileNotFoundError(f"nls binary not found at {path}")
    if not os.access(path, os.X_OK):
        raise PermissionError(f"nls binary at {path} is not executable")
    return path


def generate_fixtures(destination: Path) -> None:
    script = REPO_ROOT / "tools" / "generate_test_structure.py"
    cmd = [sys.executable, str(script), str(destination), "--platform", "linux", "--force"]
    subprocess.run(cmd, check=True, cwd=REPO_ROOT)


def _default_env() -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("LC_ALL", "C.UTF-8")
    env.setdefault("LANG", "C.UTF-8")
    return env


def build_cases(fixture_dir: Path) -> list[TestCase]:
    lin_root = fixture_dir / "lin"
    if not lin_root.is_dir():
        raise RuntimeError(f"linux fixture root {lin_root} is missing")

    cases: list[TestCase] = []
    env = _default_env()
    cwd = REPO_ROOT

    def add(name: str, *args: str, case_env: Optional[Dict[str, str]] = None, case_cwd: Optional[Path] = None) -> None:
        args_list = list(args)
        if case_env is None:
            merged_env = env
        else:
            merged_env = env.copy()
            merged_env.update(case_env)
        cases.append(TestCase(name=name, args=args_list, env=merged_env, cwd=case_cwd or cwd))

    # General behaviour.
    add("help", "--help")
    add("version", "--version")

    copy_config_root = fixture_dir / "config-home"
    home_dir = copy_config_root / "home"
    xdg_dir = copy_config_root / "xdg"
    home_dir.mkdir(parents=True, exist_ok=True)
    xdg_dir.mkdir(parents=True, exist_ok=True)
    add(
        "copy-config",
        "--copy-config",
        case_env={
            "HOME": str(home_dir),
            "XDG_CONFIG_HOME": str(xdg_dir),
        },
    )

    add("default-listing", str(lin_root))

    # Layout options.
    add("long-format", "-l", str(lin_root))
    add("one-per-line", "-1", str(lin_root))
    add("list-by-lines", "-x", str(lin_root))
    add("list-by-columns", "-C", str(lin_root))
    for format_opt in ("vertical", "across", "horizontal", "long", "single-column", "comma"):
        add(f"format-{format_opt}", f"--format={format_opt}", str(lin_root))
    add("header", "--header", str(lin_root))
    add("comma-separated", "-m", str(lin_root))
    add("tabsize", "-T", "4", str(lin_root))
    add("width", "-w", "120", str(lin_root))
    add("tree", "--tree", str(lin_root))
    add("tree-depth", "--tree=2", str(lin_root))
    for report_opt in ("short", "long"):
        add(f"report-{report_opt}", f"--report={report_opt}", str(lin_root))
    add("zero-terminated", "--zero", str(lin_root))

    # Filtering options.
    add("all", "-a", str(lin_root))
    add("almost-all", "-A", str(lin_root))
    add("dirs-only", "-d", str(lin_root))
    add("files-only", "-f", str(lin_root))
    add("ignore-backups", "-B", str(lin_root))
    add("hide-pattern", "--hide", "*.log", str(lin_root))
    add("ignore-pattern", "-I", "*.bin", str(lin_root))

    # Sorting options.
    add("sort-mod-time", "-t", str(lin_root))
    add("sort-size", "-S", str(lin_root))
    add("sort-extension", "-X", str(lin_root))
    add("unsorted", "-U", str(lin_root))
    add("reverse", "-r", str(lin_root))
    for sort_opt in ("size", "time", "extension", "none"):
        add(f"sort-by-{sort_opt}", "--sort", sort_opt, str(lin_root))
    add("group-directories-first", "--group-directories-first", str(lin_root))
    add("sort-files-first", "--sort-files", str(lin_root))
    add("dots-first", "--dots-first", str(lin_root))

    # Appearance options.
    add("escape", "-b", str(lin_root))
    add("literal", "-N", str(lin_root))
    add("quote-name", "-Q", str(lin_root))
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
        add(f"quoting-style-{style}", "--quoting-style", style, str(lin_root))
    add("append-indicator", "-p", str(lin_root))
    add("indicator-none", "--indicator-style", "none", str(lin_root))
    add("indicator-slash", "--indicator-style", "slash", str(lin_root))
    add("no-icons", "--no-icons", str(lin_root))
    add("without-icons-alias", "--without-icons", str(lin_root))
    add("no-color", "--no-color", str(lin_root))
    for when in ("always", "auto", "never"):
        add(f"color-{when}", f"--color={when}", str(lin_root))
    add("theme-dark", "--theme", "Dark", str(lin_root))
    add("light-theme", "--light", str(lin_root))
    add("dark-theme", "--dark", str(lin_root))
    add("hide-control", "-q", str(lin_root))
    add("show-control", "--show-control-chars", str(lin_root))
    for time_style in ("default", "locale", "local", "long-iso", "full-iso", "iso", "iso8601"):
        add(f"time-style-{time_style}", "--time-style", time_style, str(lin_root))
    add("full-time", "--full-time", str(lin_root))
    add("hyperlink", "--hyperlink", str(lin_root))

    # Information options.
    add("inode", "-i", str(lin_root))
    add("no-owner", "-o", str(lin_root))
    add("no-group-long", "-g", str(lin_root))
    add("no-group-option", "-G", str(lin_root))
    add("numeric-ids", "-n", str(lin_root))
    add("bytes", "--bytes", str(lin_root))
    add("size", "--size", str(lin_root))
    add("block-size", "--block-size", "1K", str(lin_root))
    add("dereference", "-L", str(lin_root))
    add("git-status", "--git-status", str(lin_root))

    # Debug / diagnostics.
    add("perf-debug", "--perf-debug", str(lin_root))

    # Subcommands.
    add("db-help", "db", "--help")

    # Representative option combinations.
    add("combo-long-all-git", "-l", "-a", "--git-status", "--header", str(lin_root))
    add("combo-sorting", "-S", "-t", "--reverse", str(lin_root))
    add("combo-format-tree", "--format", "long", "--tree=1", str(lin_root))
    add("combo-themed", "-l", "--color=always", "--theme", "Dark", str(lin_root))
    add("combo-filtering", "-A", "--hide", "*.log", "--ignore", "*.bin", str(lin_root))
    add("combo-quoting", "-Q", "--quoting-style", "shell-escape", str(lin_root))
    add("combo-numeric", "-n", "--bytes", "--block-size", "1M", str(lin_root))
    add("combo-appearance", "--no-icons", "--no-color", "--hyperlink", str(lin_root))
    add("combo-information", "-i", "-o", "-g", "-G", "--size", str(lin_root))

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

    if failures:
        raise RuntimeError("\n".join(failures))


def main() -> int:
    args = parse_args()
    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = (REPO_ROOT / binary).resolve()
    ensure_binary(binary)

    if args.fixtures:
        fixture_root = Path(args.fixtures)
        if not fixture_root.is_dir():
            raise FileNotFoundError(f"fixture directory {fixture_root} does not exist")
        temp_context = _StaticPathContext(fixture_root.resolve())
    elif args.keep_fixtures:
        persistent_dir = Path(tempfile.mkdtemp(prefix="nls-fixtures-")).resolve()
        temp_context = _StaticPathContext(persistent_dir)
    else:
        tmp_dir = tempfile.TemporaryDirectory(prefix="nls-fixtures-")
        temp_context = tmp_dir

    try:
        with temp_context as folder:
            fixture_dir = Path(folder)
            if not args.fixtures:
                generate_fixtures(fixture_dir)
            log_dir = fixture_dir / "logs"
            log_dir.mkdir(parents=True, exist_ok=True)
            cases = build_cases(fixture_dir)
            run_cases(binary, cases, log_dir)
            print(f"\nAll {len(cases)} CLI checks passed. Logs stored in {log_dir}.")
    except Exception as exc:  # pragma: no cover
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


class _StaticPathContext:  # pragma: no cover - compatibility utility
    def __init__(self, path: Path):
        self._path = path

    def __enter__(self) -> str:
        return str(self._path)

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        return False


if __name__ == "__main__":
    sys.exit(main())
