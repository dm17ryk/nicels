#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
APP="${ROOT_DIR}/../nicels"
LIN_DIR="${ROOT_DIR}/lin"
EXPECTED="${ROOT_DIR}/expected/linux_smoke.txt"
ACTUAL=$(mktemp)

cleanup() {
  local exit_code=$1
  if [[ ${exit_code} -eq 0 ]]; then
    rm -f "${ACTUAL}"
  else
    echo "Linux smoke test failed; kept ${ACTUAL} for inspection." >&2
  fi
}
trap 'cleanup $?' EXIT

"${APP}" --no-icons --no-color --group-directories-first "${LIN_DIR}" > "${ACTUAL}"
"${APP}" --tree --tree-depth=2 "${LIN_DIR}" >> "${ACTUAL}"

if [[ ! -f "${EXPECTED}" ]]; then
  echo "Expected output not found: ${EXPECTED}" >&2
  exit 1
fi

if command -v diff >/dev/null 2>&1; then
  diff -u "${EXPECTED}" "${ACTUAL}"
else
  cmp "${EXPECTED}" "${ACTUAL}"
fi

echo "Linux smoke test passed."
