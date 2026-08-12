/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __RK3588_CRU_H__
#define __RK3588_CRU_H__

#include "hw/vendor/rockchip/rk_cru.h"
#include <sys/bitfield.h>

#define RK3588_CRU_BASE                0xfd7c0000
#define RK3568_CRU_SIZE                0x10000

#define RK3588_CRU_CPLL_CON0           0x01a0u
#define RK3588_CRU_CPLL_CON1           0x01a4u
#define RK3588_CRU_CPLL_CON2           0x01a8u
#define RK3588_CRU_CPLL_CON3           0x01acu
#define RK3588_CRU_CPLL_CON4           0x01b0u
#define RK3588_CRU_CPLL_CON5           0x01b4u
#define RK3588_CRU_CPLL_CON6           0x01b8u

#define RK3588_CRU_GPLL_CON0           0x01c0u
#define RK3588_CRU_GPLL_CON1           0x01c4u
#define RK3588_CRU_GPLL_CON2           0x01c8u
#define RK3588_CRU_GPLL_CON3           0x01ccu
#define RK3588_CRU_GPLL_CON4           0x01d0u
#define RK3588_CRU_GPLL_CON5           0x01d4u
#define RK3588_CRU_GPLL_CON6           0x01d8u

#define RK3588_CRU_MODE_CON00          0x0280u

#define RK3588_CRU_CLK_GPLL_MODE_MASK  __BITS32(3, 2)
#define RK3588_CRU_CLK_CPLL_MODE_MASK  __BITS32(9, 8)

#define RK3588_CRU_PLLCON0_M_MASK      __MASK32(10)

#define RK3588_CRU_PLLCON1_P_MASK      __MASK32(6)
#define RK3588_CRU_PLLCON1_S_MASK      __BITS32(8, 6)

#endif /* __RK3588_CRU_H__ */
