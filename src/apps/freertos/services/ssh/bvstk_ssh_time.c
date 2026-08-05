/* FreeRTOS time and entropy hooks required by wolfSSL/wolfSSH. */
#include <sys/time.h>

#ifdef BVSTK_SSH_ENABLE

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xtime_l.h"

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/random.h>

static uint64_t s_seed_state;

/*
 * wolfSSL's POSIX seed path uses /dev/urandom, which is not present in this
 * FreeRTOS image.  Mix the Zynq global timer, scheduler timing and volatile
 * addresses into a local DRBG seed.  A production board should replace this
 * callback with a hardware TRNG source if one is available in the design.
 */
int bvstk_ssh_seed(OS_Seed *os, byte *output, word32 size)
{
    (void)os;
    if (!output) return -1;

    XTime now;
    XTime_GetTime(&now);
    uint64_t state = (uint64_t)now ^
                     ((uint64_t)xTaskGetTickCount() << 17) ^
                     (uint64_t)(uintptr_t)&state ^
                     (uint64_t)(uintptr_t)output ^
                     s_seed_state;
    if (state == 0) state = 0x9e3779b97f4a7c15ULL;

    for (word32 i = 0; i < size; ++i) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        output[i] = (byte)((state * 0x2545F4914F6CDD1DULL) >> 56);
    }
    s_seed_state = state ^ (uint64_t)size;
    return 0;
}

/*
 * The Xilinx FreeRTOS BSP does not provide newlib's _gettimeofday syscall.
 * wolfSSL only needs a monotonic time source for its embedded support code;
 * the wall-clock epoch is not used by the SSH console.
 */
int _gettimeofday(struct timeval *tv, void *timezone)
{
    (void)timezone;
    if (!tv) return -1;

    uint64_t ticks = (uint64_t)xTaskGetTickCount();
    uint64_t usec = (ticks * 1000000ULL) / (uint64_t)configTICK_RATE_HZ;
    tv->tv_sec = (time_t)(usec / 1000000ULL);
    tv->tv_usec = (suseconds_t)(usec % 1000000ULL);
    return 0;
}

#endif
