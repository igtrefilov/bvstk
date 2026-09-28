#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "${1:-}:${2:-}" in
  freertos:jtag)
    exec "$REPO_ROOT/scripts/vitis/run_jtag.sh"
    ;;
  freertos:qspi)
    shift 2
    exec "$REPO_ROOT/scripts/vitis/run_qspi.sh" "$@"
    ;;
  neutrino:jtag)
    "$REPO_ROOT/scripts/neutrino/run_jtag.sh"
    exec "$REPO_ROOT/scripts/neutrino/verify_ssh.sh"
    ;;
  *)
    echo "Usage: $0 {freertos|neutrino} jtag | freertos qspi [--dry-run]" >&2
    exit 2
    ;;
esac
