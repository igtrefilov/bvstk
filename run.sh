#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "${1:-}:${2:-}" in
  freertos:jtag)
    shift 2
    exec "$REPO_ROOT/scripts/vitis/run_jtag.sh" "$@"
    ;;
  freertos:qspi)
    shift 2
    exec "$REPO_ROOT/scripts/vitis/run_qspi.sh" "$@"
    ;;
  neutrino:qspi)
    shift 2
    exec "$REPO_ROOT/scripts/vitis/run_qspi.sh" --target neutrino "$@"
    ;;
  neutrino:sd)
    shift 2
    exec python3 "$REPO_ROOT/scripts/neutrino/install_sd.py" "$@"
    ;;
  neutrino:jtag)
    shift 2
    for argument in "$@"; do
      if [[ "$argument" == -h || "$argument" == --help ]]; then
        exec "$REPO_ROOT/scripts/neutrino/run_jtag.sh" "$@"
      fi
    done
    "$REPO_ROOT/scripts/neutrino/run_jtag.sh" "$@"
    exec "$REPO_ROOT/scripts/neutrino/verify_ssh.sh"
    ;;
  *)
    echo "Usage: $0 {freertos|neutrino} jtag [--hw-server HOST[:PORT]] | freertos qspi [--dry-run] | neutrino qspi [--dry-run] | neutrino sd [--dry-run] [--resources]" >&2
    exit 2
    ;;
esac
