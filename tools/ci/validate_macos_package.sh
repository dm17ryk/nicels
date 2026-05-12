#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <package-or-archive> [--run-regression]" >&2
  exit 2
fi

artifact_path="$1"
run_regression=0
if [ "${2:-}" = "--run-regression" ]; then
  run_regression=1
fi

if [ ! -f "$artifact_path" ]; then
  echo "Artifact not found: $artifact_path" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

case "$artifact_path" in
  *.pkg)
    sudo installer -pkg "$artifact_path" -target /
    binary_path="$(command -v nls || true)"
    ;;
  *.tar.gz|*.tgz)
    tar -xzf "$artifact_path" -C "$tmp_dir"
    binary_path="$(find "$tmp_dir" -type f -path '*/bin/nls' -perm -111 | head -n 1)"
    ;;
  *)
    echo "Unsupported macOS artifact type: $artifact_path" >&2
    exit 1
    ;;
esac

if [ -z "${binary_path:-}" ] || [ ! -x "$binary_path" ]; then
  echo "nls binary was not found in $artifact_path" >&2
  exit 1
fi

"$binary_path" --version
"$binary_path" --help >/dev/null

if [ "$run_regression" -eq 1 ]; then
  python3 test/run_nls_cli_tests.py --binary "$binary_path" --fixtures test --platform linux
fi
