#!/usr/bin/env python3
"""Utility to manage the nls VERSION file.

This helper keeps the version format consistent and codifies the bumping
behaviour used by CI pipelines:

* Major/minor numbers are set manually.
* Maintenance number is incremented after each merged PR and resets the build.
* Build number is incremented for release builds when code has changed.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Optional

RE_VERSION = re.compile(r"^(\d+)\.(\d+)(?:\.(\d+)(?:\.(\d+))?)?$")

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION_FILE = REPO_ROOT / "VERSION"


@dataclass
class Version:
    major: int
    minor: int
    maintenance: int = 0
    build: int = 0
    has_maintenance: bool = False
    has_build: bool = False

    @classmethod
    def parse(cls, text: str) -> "Version":
        match = RE_VERSION.match(text.strip())
        if not match:
            raise ValueError("VERSION must match major.minor[.maintenance[.build]]")
        major = int(match.group(1))
        minor = int(match.group(2))
        maintenance = int(match.group(3)) if match.group(3) is not None else 0
        build = int(match.group(4)) if match.group(4) is not None else 0
        has_maintenance = match.group(3) is not None
        has_build = match.group(4) is not None
        return cls(
            major=major,
            minor=minor,
            maintenance=maintenance,
            build=build,
            has_maintenance=has_maintenance,
            has_build=has_build,
        )

    def __str__(self) -> str:  # pragma: no cover - convenience helper
        parts = [str(self.major), str(self.minor)]
        if self.has_maintenance or self.maintenance:
            parts.append(str(self.maintenance))
            if self.has_build or self.build:
                parts.append(str(self.build))
        return ".".join(parts)

    def ensure_optional_components(self) -> None:
        if not self.has_maintenance:
            self.maintenance = 0
            self.has_maintenance = True
        if not self.has_build:
            self.build = 0
            self.has_build = True


def read_version() -> Version:
    if not VERSION_FILE.exists():
        raise SystemExit(f"error: VERSION file not found at {VERSION_FILE}")
    first_line = VERSION_FILE.read_text(encoding="utf-8").splitlines()
    if not first_line:
        raise SystemExit("error: VERSION file is empty")
    try:
        return Version.parse(first_line[0])
    except ValueError as exc:  # pragma: no cover - diagnostic path
        raise SystemExit(f"error: {exc}") from exc


def write_version(version: Version) -> None:
    version.ensure_optional_components()
    VERSION_FILE.write_text(f"{version.major}.{version.minor}.{version.maintenance}.{version.build}\n", encoding="utf-8")


def git_diff_has_changes(reference: str) -> bool:
    try:
        result = subprocess.run(
            ["git", "diff", "--quiet", reference, "--"],
            cwd=REPO_ROOT,
            check=False,
        )
    except FileNotFoundError as exc:  # pragma: no cover - git missing
        raise SystemExit("error: git executable not found") from exc
    return result.returncode == 1


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Manage the nls VERSION file")
    parser.add_argument("--set-major", type=int, help="Set the major version component")
    parser.add_argument("--set-minor", type=int, help="Set the minor version component")
    parser.add_argument("--bump-maintenance", action="store_true", help="Increment the maintenance component and reset build")
    parser.add_argument("--bump-build", action="store_true", help="Increment the build component")
    parser.add_argument(
        "--compare-ref",
        metavar="REF",
        help="Only bump the build component when code differs from the specified git reference",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print the resulting version without modifying the file")
    parser.add_argument("--quiet", action="store_true", help="Suppress informational output")
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    version = read_version()

    if args.set_major is not None:
        if args.set_major < 0:
            raise SystemExit("error: major version must be non-negative")
        version.major = args.set_major
    if args.set_minor is not None:
        if args.set_minor < 0:
            raise SystemExit("error: minor version must be non-negative")
        version.minor = args.set_minor

    if args.set_major is not None or args.set_minor is not None:
        version.maintenance = 0
        version.build = 0
        version.has_maintenance = True
        version.has_build = True

    if args.bump_maintenance:
        version.maintenance += 1
        version.build = 0
        version.has_maintenance = True
        version.has_build = True

    if args.bump_build:
        if args.compare_ref:
            if not git_diff_has_changes(args.compare_ref):
                if not args.quiet:
                    print("No code changes detected; build component unchanged.")
                if args.dry_run:
                    return 0
                # No version change requested beyond potential maintenance bump.
                if not args.bump_maintenance and args.set_major is None and args.set_minor is None:
                    return 0
        version.build += 1
        version.has_build = True
        version.has_maintenance = True

    version.ensure_optional_components()
    new_version = str(version)

    if not args.quiet:
        print(new_version)

    if not args.dry_run:
        write_version(version)

    return 0


if __name__ == "__main__":
    sys.exit(main())
