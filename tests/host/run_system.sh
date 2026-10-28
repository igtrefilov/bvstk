#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
flags=(-std=c11 -Wall -Wextra -Werror -pedantic -Isrc)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g); fi
cc "${flags[@]}" tests/host/test_system.c src/services/system/bvstk_system.c \
 src/shared/protocols/http/bvstk_http_input.c src/shared/base/bvstk_status.c -o "$1/test_system"
"$1/test_system"
