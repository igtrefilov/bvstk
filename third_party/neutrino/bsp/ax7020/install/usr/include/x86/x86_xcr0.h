/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_xcr0.h - x86 CR0 Extended Control Register
 */

#ifndef __X86_XCR0_H__
#define __X86_XCR0_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

#include _NTO_HDR_(x86/priv.h)

// Extended Control Register (XCR0) helpers
typedef union {
	uint64_t raw;
	struct {
		uint64_t
			x87       :1,
			sse       :1,
			avx       :1,
			bndreg    :1,
			bndcsr    :1,
			optmask   :1,
			zmm_hi256 :1,
			hi16_zmm  :1,
			res0      :1,
			pkru      :1,
			res1      :7,
			xtilecfg  :1,
			xtiledata :1,
			res2      :45;
	} __attribute__((packed)) bits;
} x86_xcr0_t;

static inline void
x86_xcr0_avx_enable(void) {
	x86_xcr0_t xcr0 = { .raw = xgetbv(0) };
	if (0 == xcr0.bits.avx) {
		xcr0.bits.sse = 1; // software cannot enable the XSAVE feature set for AVX state but not for SSE state.
		xcr0.bits.avx = 1;
		xsetbv(0, xcr0.raw);
	}
}

#endif /* __X86_XCR0_H__ */
