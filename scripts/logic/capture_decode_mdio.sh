#!/usr/bin/env bash
set -euo pipefail
exec /home/ilya/.venvs/logic2-automation/bin/python \
  /home/ilya/Zynq/bvstk/scripts/logic/capture_decode_mdio.py "$@"
