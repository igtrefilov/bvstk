/*
 *
 * (c) 2023-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __RK3568_CRU_H__
#define __RK3568_CRU_H__

#include <sys/bitfield.h>

#include "hw/vendor/rockchip/rk_cru.h"

#define RK3568_CRU_BASE                  0xFDD20000u

#define RK3568_CRU_APLL_CON0             0x0000u
#define RK3568_CRU_APLL_CON1             0x0004u
#define RK3568_CRU_APLL_CON2             0x0008u

#define RK3568_CRU_GPLL_CON0             0x0040u
#define RK3568_CRU_GPLL_CON1             0x0044u
#define RK3568_CRU_GPLL_CON2             0x0048u
#define RK3568_CRU_GPLL_CON3             0x004cu
#define RK3568_CRU_GPLL_CON4             0x0050u

#define RK3568_CRU_CPLL_CON0             0x0060u
#define RK3568_CRU_CPLL_CON1             0x0064u
#define RK3568_CRU_CPLL_CON2             0x0068u
#define RK3568_CRU_CPLL_CON3             0x006cu
#define RK3568_CRU_CPLL_CON4             0x0070u

#define RK3568_CRU_MODE_CON00            0x00C0u

#define RK3568_CRU_CLK_GPLL_MODE_MASK    __BITS32(7, 6)
#define RK3568_CRU_CLK_CPLL_MODE_MASK    __BITS32(5, 4)

#define RK3568_CRU_PLLCON0_POSTDIV1_MASK __BITS32(14, 12)
#define RK3568_CRU_PLLCON0_FBDIV_MASK    __MASK32(12)

#define RK3568_CRU_PLLCON1_DSMPD_MASK    __BIT32(12)
#define RK3568_CRU_PLLCON1_PLL_LOCK_MASK __BIT32(10)
#define RK3568_CRU_PLLCON1_POSTDIV2_MASK __BITS32(8, 6)
#define RK3568_CRU_PLLCON1_REFDIV_MASK   __MASK32(6)

#define RK3568_CRU_PLLCON2_FRACDIV_MASK  __MASK32(24)

#define RK3568_CRU_GLB_SRST_FST          0x00d4u
#define RK3568_CRU_GLB_SRST_FST_CFG_VAL  0xfdb9u

#define RK3568_CRU_CLKSEL_CON72          0x0220
#define RK3568_CLK_SPI0_SEL_100M         0x20002
#define RK3568_CLK_SPI1_SEL_100M         0x80008

#define MHz                              1000000u
#define RK3568_CRU_FREF                  24ul * MHz

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <aarch64/inout.h>

#define CRU_GET_FIELD(base, REG, FIELD)                           \
    (__SHIFTOUT((cru_read_##REG(base)), RK3568_CRU_##FIELD##_MASK))

/*
 * Accessor to read the GRU_APPL_CON0
 */
static inline const uint32_t
cru_read_apll_con0(const uintptr_t base)
{
    return in32(base + RK3568_CRU_APLL_CON0);
}

/*
 * Accessor to read the GRU_APPL_CON1
 */
static inline const uint32_t
cru_read_apll_con1(const uintptr_t base)
{
    return in32(base + RK3568_CRU_APLL_CON1);
}

/*
 * Accessor to read the GRU_APPL_CON2
 */
static inline const uint32_t
cru_read_apll_con2(const uintptr_t base)
{
    return in32(base + RK3568_CRU_APLL_CON2);
}

extern uint32_t rk3568_get_pll_rate(const uintptr_t base);
#endif /* __ASSEMBLER__ */

#endif /* __RK3568_CRU_H__ */
