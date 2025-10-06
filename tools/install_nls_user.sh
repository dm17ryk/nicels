#!/usr/bin/env bash
#
# install_nls_user.sh - Install nicels (nls) for the current user without root.
#
# This script copies a pre-built nls binary and its YAML configuration files
# into user-writable locations (defaulting to ~/.local/bin and ~/.nicels/yaml).
# Existing configuration directories are backed up before the defaults are
# installed to avoid overwriting user customisations.
set -euo pipefail

print_usage() {
  cat <<'USAGE'
Usage: install_nls_user.sh --binary <path-to-nls> [--configs <yaml-dir>] [--prefix <install-root>]
                           [--config-dest <path>] [--dry-run]

Options:
  --binary <path>       Path to the compiled nls executable to install (required).
  --configs <path>      Directory containing YAML configuration files. Defaults to
                        the directory named "yaml" next to the binary.
  --prefix <path>       Base directory for the installation. Defaults to "$HOME/.local".
  --config-dest <path>  Destination for configuration files. Defaults to "$HOME/.nicels/yaml".
  --dry-run             Show the actions that would be performed without making changes.
  -h, --help            Show this help text and exit.

Examples:
  # Install a locally built binary and YAML directory
  ./tools/install_nls_user.sh --binary build/release/nls --configs yaml

  # Install using defaults when the binary and yaml directory are siblings
  ./tools/install_nls_user.sh --binary ./dist/nls
USAGE
}

# Default locations
INSTALL_PREFIX="${HOME}/.local"
CONFIG_DEST="${HOME}/.nicels/yaml"
BINARY_SOURCE=""
CONFIG_SOURCE=""
DRY_RUN=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary)
      BINARY_SOURCE="$2"
      shift 2
      ;;
    --configs)
      CONFIG_SOURCE="$2"
      shift 2
      ;;
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --config-dest)
      CONFIG_DEST="$2"
      shift 2
      ;;
    --dry-run)
      DRY_RUN=true
      shift
      ;;
    -h|--help)
      print_usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      print_usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "$BINARY_SOURCE" ]]; then
  echo "Error: --binary is required." >&2
  print_usage >&2
  exit 1
fi

if [[ ! -f "$BINARY_SOURCE" ]]; then
  echo "Error: binary '$BINARY_SOURCE' does not exist." >&2
  exit 1
fi

if [[ -z "$CONFIG_SOURCE" ]]; then
  CONFIG_SOURCE="$(cd "$(dirname "$BINARY_SOURCE")" && pwd)/yaml"
fi

if [[ ! -d "$CONFIG_SOURCE" ]]; then
  echo "Error: configuration directory '$CONFIG_SOURCE' does not exist." >&2
  exit 1
fi

BIN_DEST="${INSTALL_PREFIX}/bin"

if [[ "$CONFIG_DEST" != /* ]]; then
  echo "Error: --config-dest must be an absolute path." >&2
  exit 1
fi

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
backup_path="${CONFIG_DEST}.bak-${TIMESTAMP}"

do_run() {
  if [[ "$DRY_RUN" == true ]]; then
    printf 'DRY-RUN:' >&2
    for arg in "$@"; do
      printf ' %q' "$arg" >&2
    done
    printf '\n' >&2
  else
    "$@"
  fi
}

install_binary() {
  local dest_file="${BIN_DEST}/nls"
  echo "Installing nls to ${dest_file}"
  do_run install -D -m 0755 "${BINARY_SOURCE}" "${dest_file}"
}

backup_existing_config() {
  if [[ -d "$CONFIG_DEST" ]]; then
    echo "Existing configuration detected at ${CONFIG_DEST}" >&2
    echo "Backing up to ${backup_path}" >&2
    do_run mv "${CONFIG_DEST}" "${backup_path}"
  fi
}

install_configs() {
  echo "Installing configuration files to ${CONFIG_DEST}"
  do_run mkdir -p "${CONFIG_DEST}"
  if command -v rsync >/dev/null 2>&1; then
    do_run rsync -a --delete "${CONFIG_SOURCE}/" "${CONFIG_DEST}/"
  else
    do_run cp -a "${CONFIG_SOURCE}/." "${CONFIG_DEST}/"
  fi
}

warn_about_path() {
  case ":${PATH}:" in
    *:"${BIN_DEST}":*)
      return
      ;;
    *)
      echo
      echo "Note: ${BIN_DEST} is not currently on your PATH." >&2
      echo "Add the following line to your shell profile to use nls immediately:" >&2
      printf '  export PATH="%s:$PATH"\n' "${BIN_DEST}" >&2
      ;;
  esac
}

install_binary
backup_existing_config
install_configs

if [[ "$DRY_RUN" == true ]]; then
  echo
  echo "Dry-run complete. No changes were made."
else
  echo
  echo "nicels installed for user '${USER}'."
  echo "Binary: ${BIN_DEST}/nls"
  echo "Configuration: ${CONFIG_DEST}"
  if [[ -d "$backup_path" ]]; then
    echo "Previous configuration backup: ${backup_path}"
  fi
  warn_about_path
  echo
  echo "Run 'nls --version' to verify the installation."
fi
