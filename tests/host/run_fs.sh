#!/usr/bin/env bash
set -euo pipefail
FS_TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
FS_REPO_ROOT=$(cd "$FS_TEST_DIR/../.." && pwd)
FS_TEST_OUTPUT=${1:?Pass an existing temporary output directory}
cd "$FS_REPO_ROOT"
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -pthread)
if [[ ${SANITIZE:-0} == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
fi
cc "${flags[@]}" -Isrc tests/host/test_fs_posix.c \
  src/services/fs/bvstk_fs_reader.c src/services/fs/bvstk_fs_storage.c \
  src/ports/neutrino-zynq7000/storage/bvstk_fs_posix.c \
  -o "$FS_TEST_OUTPUT/test_fs_posix"
"$FS_TEST_OUTPUT/test_fs_posix" "$FS_TEST_OUTPUT/posix-storage"
cc "${flags[@]}" -ffunction-sections -fdata-sections -Itests/host/fs_stubs -Isrc \
  tests/host/test_fs_fatfs.c src/apps/freertos/storage/fs/fs_dcp2_backend.c \
  src/apps/freertos/storage/fs/fs_shared.c src/services/fs/bvstk_fs_reader.c \
  src/services/fs/bvstk_fs_storage.c -Wl,--gc-sections \
  -o "$FS_TEST_OUTPUT/test_fs_fatfs"
"$FS_TEST_OUTPUT/test_fs_fatfs" "$FS_TEST_OUTPUT/fatfs-storage"
cc "${flags[@]}" -Wno-overlength-strings -Isrc -Isrc/apps/freertos/config \
  tests/host/test_profile_storage.c \
  src/apps/neutrino/config/bvstk_profile_store.c \
  src/apps/neutrino/config/bvstk_i2c_config_store.c \
  src/apps/neutrino/config/bvstk_mdio_config_store.c \
  src/shared/config/bvstk_device_profile.c \
  src/shared/config/bvstk_i2c_config_codec.c src/shared/config/bvstk_mdio_config_codec.c \
  src/services/fs/bvstk_fs_reader.c src/services/fs/bvstk_fs_storage.c \
  src/ports/neutrino-zynq7000/storage/bvstk_fs_posix.c \
  -o "$FS_TEST_OUTPUT/test_profile_storage"
"$FS_TEST_OUTPUT/test_profile_storage" "$FS_TEST_OUTPUT/profile-storage" "$FS_REPO_ROOT"
