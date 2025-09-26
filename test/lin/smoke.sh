#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BIN="${REPO_ROOT}/nicels"

if [[ ! -x "${BIN}" ]]; then
  echo "nicels executable not found at ${BIN}. Build the project first." >&2
  exit 1
fi

cd "${SCRIPT_DIR}"

echo "# nicels long listing"
"${BIN}" -l --no-color --no-icons .

echo "\n# nicels tree"
"${BIN}" --tree --no-icons --no-color --tree-depth=2 .
