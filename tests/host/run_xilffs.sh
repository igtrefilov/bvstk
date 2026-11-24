#!/usr/bin/env bash
# Run from the repository: tests/host/run_xilffs.sh PATH/TO/xilffs_v4_6/src
# Runs production storage against two in-memory FAT volumes on the host.
set -eu -o pipefail
vendor=${1:?Pass a xilffs_v4_6/src directory containing ff.c and include/ff.h}
vendor=$(cd "$vendor" && pwd)
root=$(cd "$(dirname "$0")/../.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
# Use the layout that Vitis generates so the normal BSP patcher runs unchanged.
src="$out/plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp/ps7_cortexa9_0/libsrc/xilffs_v4_6/src"
inc="$src/include"
mkdir -p "$inc"
cp "$vendor"/{ff.c,ffsystem.c} "$src/"
cp "$vendor"/include/{diskio.h,ff.h,ffconf.h,integer.h} "$inc/"
python3 "$root/tools/codegen/patch_ffconf_lfn.py" "$out"
# FatFs uses 32-bit long on Cortex-A9. Keep its on-disk typedefs on LP64 hosts.
sed -i -E 's/^(typedef (unsigned )?)long([[:space:]]+D?LONG|[[:space:]]+DWORD);/\1int\3;/' "$inc/integer.h"
cat > "$inc/xparameters.h" <<'HEADER'
#define FILE_SYSTEM_INTERFACE_RAM
#define FILE_SYSTEM_USE_MKFS
#define FILE_SYSTEM_NUM_LOGIC_VOL 2
HEADER
echo '#include <stdint.h>' > "$inc/xil_types.h"
cat > "$inc/FreeRTOS.h" <<'HEADER'
#ifndef XILFFS_HOST_FREERTOS_H
#define XILFFS_HOST_FREERTOS_H
#include <pthread.h>
#include <stdlib.h>
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef unsigned TickType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define pvPortMalloc malloc
#define vPortFree free
#endif
HEADER
cat > "$inc/semphr.h" <<'HEADER'
#include "FreeRTOS.h"
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
int xSemaphoreTakeRecursive(SemaphoreHandle_t, unsigned);
int xSemaphoreGiveRecursive(SemaphoreHandle_t);
#define xSemaphoreTake xSemaphoreTakeRecursive
#define xSemaphoreGive xSemaphoreGiveRecursive
HEADER
cat > "$inc/task.h" <<'HEADER'
#include "FreeRTOS.h"
void vTaskSuspendAll(void);
int xTaskResumeAll(void);
HEADER
cd "$root"
cc -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -ffunction-sections -fdata-sections \
    -I"$inc" -Itests/host/fs_stubs -Isrc -pthread tests/host/test_xilffs_locks.c \
    "$src/ff.c" "$src/ffsystem.c" "$vendor/ffunicode.c" \
    src/apps/freertos/storage/fs/{fs_dcp2_backend,fs_shared}.c \
    src/services/fs/{bvstk_fs_reader,bvstk_fs_storage}.c -Wl,--gc-sections -o "$out/test"
"$out/test"
