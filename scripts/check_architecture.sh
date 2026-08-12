#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

fail=0

check_forbidden() {
  local label="$1"
  local pattern="$2"
  shift 2
  local matches

  matches="$(rg -n "$pattern" "$@" --glob '*.[ch]' || true)"
  if [[ -n "$matches" ]]; then
    echo "Architecture violation in $label:" >&2
    echo "$matches" >&2
    fail=1
  fi
}

check_forbidden \
  "shared/hardware code (OS or BSP dependency)" \
  '#[[:space:]]*include[[:space:]]*[<"](FreeRTOS|task|queue|semphr|ff|xparameters|xstatus|xil_|lwip/|sys/neutrino|sys/mman)' \
  src/shared src/hardware

check_forbidden \
  "portable drivers/services/protocols (OS, BSP, or vendor dependency)" \
  '#[[:space:]]*include[[:space:]]*[<"](FreeRTOS|task|queue|semphr|ff|xparameters|xstatus|xil_|lwip/|sys/neutrino|sys/mman)' \
  src/drivers src/services src/protocols

check_forbidden \
  "Neutrino target (FreeRTOS/Xilinx dependency)" \
  '#[[:space:]]*include[[:space:]]*[<"](FreeRTOS|task|queue|semphr|ff|xparameters|xstatus|xil_|lwip/)' \
  src/apps/neutrino src/ports/neutrino-zynq7000

check_forbidden \
  "lower layers (application dependency)" \
  '#[[:space:]]*include[[:space:]]*"apps/' \
  src/shared src/hardware src/drivers src/services src/protocols src/ports

check_forbidden \
  "shared/hardware code (target-port dependency)" \
  '#[[:space:]]*include[[:space:]]*"ports/' \
  src/shared src/hardware src/drivers src/services src/protocols

check_forbidden \
  "FreeRTOS target (Neutrino source dependency)" \
  '#[[:space:]]*include[[:space:]]*"(apps/neutrino|ports/neutrino-zynq7000)/' \
  src/apps/freertos src/ports/freertos-xilinx

check_forbidden \
  "Neutrino target (FreeRTOS source dependency)" \
  '#[[:space:]]*include[[:space:]]*"(apps/freertos|ports/freertos-xilinx)/' \
  src/apps/neutrino src/ports/neutrino-zynq7000

relative_includes="$(rg -n '#[[:space:]]*include[[:space:]]*"\.\./' src --glob '*.[ch]' || true)"
if [[ -n "$relative_includes" ]]; then
  echo "Relative parent includes are not allowed; include from src/ root:" >&2
  echo "$relative_includes" >&2
  fail=1
fi

legacy_guards="$(rg -n '__QNXNTO__' src --glob '*.[ch]' || true)"
if [[ -n "$legacy_guards" ]]; then
  echo "Target selection must happen in build manifests, not __QNXNTO__ guards:" >&2
  echo "$legacy_guards" >&2
  fail=1
fi

if (( fail != 0 )); then
  exit 1
fi

"$REPO_ROOT/tests/host/run.sh"
echo "Architecture checks passed"
