#!/usr/bin/env bash
set -euo pipefail
HTTP_TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
HTTP_REPO_ROOT=$(cd "$HTTP_TEST_DIR/../.." && pwd)
HTTP_TEST_OUTPUT=${1:?Pass an existing temporary output directory}
cd "$HTTP_REPO_ROOT"
http_flags=(-std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic)
if [[ ${SANITIZE:-0} == 1 ]]; then
  http_flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
fi
cc "${http_flags[@]}" -Isrc tests/host/test_http_input.c \
  src/shared/protocols/http/bvstk_http_input.c -o "$HTTP_TEST_OUTPUT/test_http_input"
"$HTTP_TEST_OUTPUT/test_http_input"
cc "${http_flags[@]}" -Itests/host/http_stubs -Isrc \
  tests/host/test_http_transport.c -o "$HTTP_TEST_OUTPUT/test_http_transport"
"$HTTP_TEST_OUTPUT/test_http_transport"
cc "${http_flags[@]}" -Itests/host/fs_stubs -Isrc \
  tests/host/test_http_ui.c \
  src/apps/freertos/services/http/http_ui.c \
  src/apps/freertos/services/http/http_ui_bus.c \
  src/apps/freertos/services/http/http_ui_files.c \
  src/shared/protocols/http/bvstk_http_input.c \
  src/shared/config/bvstk_i2c_config_codec.c \
  src/shared/config/bvstk_mdio_config_codec.c \
  src/apps/freertos/console/bus_shell_common.c \
  src/apps/freertos/console/i2c_shell.c \
  src/apps/freertos/console/mdio_shell.c \
  src/drivers/pl/i2c/bvstk_i2c_master.c \
  src/drivers/pl/mdio/bvstk_mdio_core.c \
  src/services/i2c/bvstk_i2c_devices.c \
  src/services/i2c/bvstk_i2c_cache.c \
  src/services/i2c/bvstk_i2c_policy.c \
  src/services/i2c/bvstk_i2c_master_service.c \
  src/services/mdio/bvstk_mdio_service.c \
  src/shared/cli/bvstk_i2c_completion.c \
  src/shared/cli/bvstk_mdio_completion.c \
  src/shared/base/bvstk_parse.c src/shared/base/bvstk_status.c \
  -o "$HTTP_TEST_OUTPUT/test_http_ui"
"$HTTP_TEST_OUTPUT/test_http_ui" "$HTTP_TEST_OUTPUT/http-fs"
