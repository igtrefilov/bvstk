#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_TMP="$(mktemp -d)"
trap 'rm -rf "$TEST_TMP"' EXIT

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_shared.c" \
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_slave.c" \
  "$REPO_ROOT/src/drivers/pl/sd/bvstk_sd_controller.c" \
  "$REPO_ROOT/src/drivers/pl/smi/bvstk_smi_core.c" \
  "$REPO_ROOT/src/drivers/pl/spi/bvstk_spi_core.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_control.c" \
  "$REPO_ROOT/src/shared/base/bvstk_parse.c" \
  "$REPO_ROOT/src/shared/base/bvstk_status.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c" \
  "$REPO_ROOT/src/services/smi/bvstk_smi_service.c" \
  "$REPO_ROOT/src/services/control/bvstk_control_api.c" \
  -o "$TEST_TMP/test_shared"

"$TEST_TMP/test_shared"
