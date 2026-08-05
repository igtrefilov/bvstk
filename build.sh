#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  echo "Usage: $0 {check|freertos|neutrino|neutrino-image|all}" >&2
}

case "${1:-}" in
  check)
    exec "$REPO_ROOT/scripts/check_architecture.sh"
    ;;
  freertos)
    exec "$REPO_ROOT/scripts/vitis/build.sh"
    ;;
  neutrino)
    exec "$REPO_ROOT/scripts/neutrino/build.sh"
    ;;
  neutrino-image)
    exec "$REPO_ROOT/scripts/neutrino/build_image.sh"
    ;;
  all)
    "$REPO_ROOT/scripts/vitis/build.sh"
    "$REPO_ROOT/scripts/neutrino/build_image.sh"
    ;;
  *)
    usage
    exit 2
    ;;
esac
