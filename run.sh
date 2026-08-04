#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "${1:-}:${2:-}" in
  freertos:jtag)
    exec "$REPO_ROOT/scripts/vitis/run_jtag.sh"
    ;;
  neutrino:jtag)
    "$REPO_ROOT/scripts/neutrino/run_jtag.sh"
    exec "$REPO_ROOT/scripts/neutrino/verify_ssh.sh"
    ;;
  *)
    echo "Usage: $0 {freertos|neutrino} jtag" >&2
    exit 2
    ;;
esac
