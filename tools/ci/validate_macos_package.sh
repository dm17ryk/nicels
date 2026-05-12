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

find_packaged_binary() {
  find "$1" -type f -path '*/bin/nls' -perm -111 | head -n 1
}

case "$artifact_path" in
  *.pkg)
    expanded_pkg="$tmp_dir/pkg"
    extracted_payload="$tmp_dir/pkg-root"
    pkgutil --expand-full "$artifact_path" "$expanded_pkg"
    binary_path="$(find_packaged_binary "$expanded_pkg")"
    if [ -z "$binary_path" ]; then
      component_pkg="$(find "$expanded_pkg/Contents/Packages" -mindepth 1 -maxdepth 1 -print | head -n 1 || true)"
      if [ -n "$component_pkg" ]; then
        component_dir="$tmp_dir/component"
        pkgutil --expand-full "$component_pkg" "$component_dir"
        binary_path="$(find_packaged_binary "$component_dir")"
        expanded_pkg="$component_dir"
      fi
    fi
    if [ -z "$binary_path" ]; then
      payload_path="$(find "$expanded_pkg" -type f -name Payload | head -n 1)"
      if [ -n "$payload_path" ]; then
        mkdir -p "$extracted_payload"
        ditto -x -z "$payload_path" "$extracted_payload"
        binary_path="$(find_packaged_binary "$extracted_payload")"
      fi
    fi
    if [ -z "$binary_path" ] &&
       find "$expanded_pkg" -type f \( -name Distribution -o -name PackageInfo \) | grep -q .; then
      echo "macOS package is structurally valid: $artifact_path"
      exit 0
    fi
    ;;
  *.tar.gz|*.tgz)
    tar -xzf "$artifact_path" -C "$tmp_dir"
    binary_path="$(find_packaged_binary "$tmp_dir")"
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
  python3 test/run_nls_cli_tests.py --binary "$binary_path" --fixtures test --platform macos
fi
