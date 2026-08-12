/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_cr0.h - x86 CR0 Control Register
 */

#ifndef __X86_CR0_H__
#define __X86_CR0_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

#include _NTO_HDR_(x86/priv.h)

/*
 * Machine status word flags (TODO: replace it with CR0, cause MSW is deprecated in new x86)
 */
#define X86_CR0_PE				_ONEBIT32L(0) // Protected Mode Enable
#define X86_MSW_MP_BIT			_ONEBIT32L(1) // Monitor co-processor
#define X86_MSW_EM_BIT			_ONEBIT32L(2) // x87 FPU Emulation
#define X86_MSW_TS_BIT			_ONEBIT32L(3) // Task switched
#define X86_MSW_ET_BIT			_ONEBIT32L(4) // Extension type
#define X86_MSW_NE_BIT			_ONEBIT32L(5) // Numeric error
#define X86_MSW_WP_BIT			_ONEBIT32L(16) // Write protect
#define X86_MSW_AM_BIT			_ONEBIT32L(18) // Alignment mask
#define X86_MSW_NW_BIT			_ONEBIT32L(29) // Not-write through
#define X86_MSW_CD_BIT			_ONEBIT32L(30) // Cache disable
#define X86_CR0_PG				_ONEBIT32L(31) // Paging

static inline void
x86_cr0_pg_unset(void) {
	ldcr0(rdcr0() & ~X86_CR0_PG);
}

#endif /* __X86_CR0_H__ */