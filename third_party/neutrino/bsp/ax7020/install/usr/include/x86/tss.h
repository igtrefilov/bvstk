/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/tss.h
 */

#ifndef __X86_TSS_H__
#define __X86_TSS_H__

#include <stdint.h>

typedef struct x86_tss_entry {
	uint32_t	back_link,
				esp0,
				ss0,
				esp1,
				ss1,
				esp2,
				ss2,
				pdbr,
				eip,
				eflags,
				eax,
				ecx,
				edx,
				ebx,
				*esp,
				ebp,
				esi,
				edi,
				es,
				cs,
				ss,
				ds,
				fs,
				gs,
				ldt;
	uint16_t	debug_flag; /* T flag in bottom bit */
	uint16_t	iomap_base;
	uint32_t	iomap_data[1];
} X86_TSS;

extern void tss_init(const uint8_t core);

#endif /* __X86_TSS_H__ */
