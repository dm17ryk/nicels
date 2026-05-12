#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <package-path> [--run-regression]" >&2
  exit 2
fi

package_path="$1"
run_regression=0
if [ "${2:-}" = "--run-regression" ]; then
  run_regression=1
fi

if [ ! -f "$package_path" ]; then
  echo "Package not found: $package_path" >&2
  exit 1
fi

package_dir="$(cd "$(dirname "$package_path")" && pwd -P)"
package_path="${package_dir}/$(basename "$package_path")"

if command -v sudo >/dev/null 2>&1; then
  sudo_cmd=(sudo)
else
  sudo_cmd=()
fi

case "$package_path" in
  *.deb)
    export DEBIAN_FRONTEND=noninteractive
    "${sudo_cmd[@]}" apt-get update
    "${sudo_cmd[@]}" apt-get install -y "$package_path"
    ;;
  *.rpm)
    if command -v dnf5 >/dev/null 2>&1; then
      "${sudo_cmd[@]}" dnf5 install -y "$package_path"
    elif command -v dnf >/dev/null 2>&1; then
      "${sudo_cmd[@]}" dnf install -y "$package_path"
    elif command -v microdnf >/dev/null 2>&1; then
      "${sudo_cmd[@]}" microdnf install -y "$package_path"
    else
      echo "No RPM-capable package manager found" >&2
      exit 1
    fi
    ;;
  *)
    echo "Unsupported Linux package type: $package_path" >&2
    exit 1
    ;;
esac

binary_path="$(command -v nls || true)"
if [ -z "$binary_path" ]; then
  echo "Installed nls binary was not found on PATH" >&2
  exit 1
fi

"$binary_path" --version
"$binary_path" --help >/dev/null

if [ "$run_regression" -eq 1 ]; then
  python3 test/run_nls_cli_tests.py --binary "$binary_path" --fixtures test --platform linux
fi
