/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_pgtbl.h - x86 Page Tables
 */

#ifndef __X86_PGTBL_H__
#define __X86_PGTBL_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

/*
 * Page directory entry bits
 */
#define X86_PDE_PRESENT			0x00000001
#define X86_PDE_WRITE			0x00000002
#define X86_PDE_USER			0x00000004
#define X86_PDE_WT				0x00000008
#define X86_PDE_CD				0x00000010
#define X86_PDE_ACCESSED		0x00000020
#define X86_PDE_PS				0x00000080
#define X86_PDE_GLOBAL			0x00000100
#define X86_PDE_USER1			0x00000200
#define X86_PDE_USER2			0x00000400
#define X86_PDE_USER3			0x00000800
#define X86_PDE_NX				((uint64_t)0x80000000 << 32)
#define X86_4M_PAGESIZE			0x00400000

/*
 * Page table entry bits
 */
#define X86_PTE_PRESENT			_ONEBIT64L(0)
#define X86_PTE_WRITE			_ONEBIT64L(1)
#define X86_PTE_USER			_ONEBIT64L(2)
#define X86_PTE_WT				_ONEBIT64L(3)
#define X86_PTE_CD				_ONEBIT64L(4)
#define X86_PTE_ACCESSED		_ONEBIT64L(5)
#define X86_PTE_DIRTY			_ONEBIT64L(6)
#define X86_PTE_PAT				_ONEBIT64L(7)
#define X86_PTE_GLOBAL			_ONEBIT64L(8)
#define X86_PTE_USER1			_ONEBIT64L(9)
#define X86_PTE_USER2			_ONEBIT64L(10)
#define X86_PTE_USER3			_ONEBIT64L(11)
#define X86_PTE_NX				_ONEBIT64L(63)

#endif /* __X86_PGTBL_H__ */