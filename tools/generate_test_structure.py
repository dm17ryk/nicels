#!/usr/bin/env python3
"""Materialise the test directory fixtures for nls CLI tests.

The repository keeps the Linux and Windows directory assets in compressed
archives under ``test/assets`` so that we do not have to version-control
trees containing special permissions, FIFOs, or nested Git repositories.
This script unpacks those archives into a destination directory, recreating
the exact layouts used by the behavioural tests.
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
import tarfile
from pathlib import Path


ASSETS_DIR = Path(__file__).resolve().parent.parent / "test" / "assets"
LIN_ARCHIVE = ASSETS_DIR / "lin_testdata.tar.gz"
WIN_ARCHIVE = ASSETS_DIR / "win_testdata.tar.gz"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the nls test directory structures from archived fixtures.",
    )
    parser.add_argument(
        "destination",
        nargs="?",
        default="build/testdata",
        help="Directory that will receive the generated fixtures (default: build/testdata).",
    )
    parser.add_argument(
        "--platform",
        choices=("linux", "windows", "all"),
        default="all",
        help="Select which fixture set to unpack (default: all).",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing fixture subdirectories at the destination.",
    )
    return parser.parse_args()


def _rmtree(path: Path) -> None:
    def _onerror(func, value, exc):
        if not os.access(value, os.W_OK):
            os.chmod(value, 0o700)
            func(value)
        else:  # pragma: no cover - only used on stubborn paths
            raise

    shutil.rmtree(path, onerror=_onerror)


def _platform_entries(selection: str) -> list[tuple[str, Path]]:
    if selection == "linux":
        return [("linux", LIN_ARCHIVE)]
    if selection == "windows":
        return [("windows", WIN_ARCHIVE)]
    return [
        ("linux", LIN_ARCHIVE),
        ("windows", WIN_ARCHIVE),
    ]


def _safe_extract(archive: Path, destination: Path) -> None:
    if not archive.exists():
        raise FileNotFoundError(f"fixture archive {archive} is missing")

    with tarfile.open(archive, mode="r:gz") as bundle:
        members = bundle.getmembers()
        dest_root = destination.resolve()
        for member in members:
            member_path = dest_root / member.name
            try:
                member_path.resolve().relative_to(dest_root)
            except ValueError as exc:
                raise RuntimeError(f"archive member {member.name!r} escapes destination {dest_root}") from exc
        bundle.extractall(dest_root)


def _post_process(destination: Path, platform: str) -> None:
    if platform == "linux":
        no_access = destination / "lin" / "permissions" / "no_access"
        if no_access.exists():
            os.chmod(no_access, 0)


def main() -> int:
    args = parse_args()
    destination = Path(args.destination).expanduser()

    selected = _platform_entries(args.platform)

    if destination.exists():
        if not destination.is_dir():
            print(f"error: destination {destination} exists and is not a directory", file=sys.stderr)
            return 1
        for platform, _ in selected:
            subdir = destination / ("lin" if platform == "linux" else "win")
            if subdir.exists():
                if args.force:
                    _rmtree(subdir)
                else:
                    print(
                        f"error: fixture directory {subdir} already exists (use --force to overwrite)",
                        file=sys.stderr,
                    )
                    return 1
    else:
        destination.mkdir(parents=True, exist_ok=True)

    destination.mkdir(parents=True, exist_ok=True)

    if not selected:
        print("nothing to do: no platforms selected", file=sys.stderr)
        return 1

    for platform, archive in selected:
        print(f"Unpacking {platform} fixtures from {archive} ...")
        _safe_extract(archive, destination)
        _post_process(destination, platform)

    return 0


if __name__ == "__main__":
    sys.exit(main())
