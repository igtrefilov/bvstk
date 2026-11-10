#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_TMP="$1"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffunction-sections -fdata-sections \
  -I"$SCRIPT_DIR/i2c_slave_stubs" -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_i2c_slave_reply.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_slave.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  -Wl,--gc-sections -o "$TEST_TMP/test_i2c_slave_reply"
"$TEST_TMP/test_i2c_slave_reply" "$REPO_ROOT/configs"
