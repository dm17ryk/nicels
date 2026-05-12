#!/usr/bin/env python3
"""Unit tests for fixture archive extraction compatibility."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPT_PATH = REPO_ROOT / "tools" / "generate_test_structure.py"


def _load_generate_test_structure_module():
    spec = importlib.util.spec_from_file_location("generate_test_structure", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load {SCRIPT_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


generate_test_structure = _load_generate_test_structure_module()


class _FakeMember:
    def __init__(self, name: str) -> None:
        self.name = name


class _FakeTarBundle:
    def __init__(self, *, supports_filter: bool, member_name: str = "lin/basic/readme.txt") -> None:
        self.supports_filter = supports_filter
        self.members = [_FakeMember(member_name)]
        self.extract_calls: list[tuple[Path, object]] = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb) -> bool:
        return False

    def getmembers(self):
        return self.members

    def extractall(self, path, members=None, *, numeric_owner=False, filter=None):
        if filter is not None and not self.supports_filter:
            raise TypeError("extractall() got an unexpected keyword argument 'filter'")
        self.extract_calls.append((Path(path), filter))


class SafeExtractTests(unittest.TestCase):
    def test_extractall_uses_fully_trusted_filter_when_supported(self) -> None:
        bundle = _FakeTarBundle(supports_filter=True)

        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(
            generate_test_structure.tarfile,
            "open",
            return_value=bundle,
        ):
            archive = Path(tmp) / "fixtures.tar.gz"
            archive.touch()
            generate_test_structure._safe_extract(archive, Path(tmp))

        self.assertEqual(len(bundle.extract_calls), 1)
        self.assertEqual(bundle.extract_calls[0][1], "fully_trusted")

    def test_extractall_falls_back_without_filter_on_type_error(self) -> None:
        bundle = _FakeTarBundle(supports_filter=False)

        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(
            generate_test_structure.tarfile,
            "open",
            return_value=bundle,
        ):
            archive = Path(tmp) / "fixtures.tar.gz"
            archive.touch()
            generate_test_structure._safe_extract(archive, Path(tmp))

        self.assertEqual(len(bundle.extract_calls), 1)
        self.assertIsNone(bundle.extract_calls[0][1])

    def test_traversal_is_rejected_before_supported_filter_extract(self) -> None:
        bundle = _FakeTarBundle(supports_filter=True, member_name="../escape.txt")

        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(
            generate_test_structure.tarfile,
            "open",
            return_value=bundle,
        ):
            archive = Path(tmp) / "fixtures.tar.gz"
            archive.touch()
            with self.assertRaisesRegex(RuntimeError, "escapes destination"):
                generate_test_structure._safe_extract(archive, Path(tmp))

        self.assertEqual(bundle.extract_calls, [])

    def test_traversal_is_rejected_before_fallback_extract(self) -> None:
        bundle = _FakeTarBundle(supports_filter=False, member_name="../escape.txt")

        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(
            generate_test_structure.tarfile,
            "open",
            return_value=bundle,
        ):
            archive = Path(tmp) / "fixtures.tar.gz"
            archive.touch()
            with self.assertRaisesRegex(RuntimeError, "escapes destination"):
                generate_test_structure._safe_extract(archive, Path(tmp))

        self.assertEqual(bundle.extract_calls, [])


if __name__ == "__main__":
    unittest.main()
