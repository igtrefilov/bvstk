/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_eflags.h - x86 Extended Flags
 */

#ifndef __X86_EFLAGS_H__
#define __X86_EFLAGS_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

/*
 * Processor Status Word Register
 */
#define X86_PSW_CF				_ONEBIT32L(0) // Carry Flag
#define X86_PSW_PF				_ONEBIT32L(2) // Parity Flag
#define X86_PSW_AF				_ONEBIT32L(4) // Auxiliary Carry Flag
#define	X86_PSW_ZF				_ONEBIT32L(6) // Zero Flag
#define X86_PSW_SF				_ONEBIT32L(7) // Sign Flag
#define X86_PSW_TF				_ONEBIT32L(8) // Trap Flag
#define X86_PSW_IF				_ONEBIT32L(9) // Interrupt Enable Flag
#define X86_PSW_DF				_ONEBIT32L(10) // Direction Flag
#define X86_PSW_OF				_ONEBIT32L(11) // Overflow Flag
#define X86_PSW_IOPL_SHIFT				  (12) // I/O Privilege Level
	#define X86_PSW_IOPL_MASK	_BITFIELD32L(X86_PSW_IOPL_SHIFT, 0x3)
#define X86_PSW_NT				_ONEBIT32L(14) // Nested Task
#define X86_PSW_RF				_ONEBIT32L(16) // Resume Flag
#define X86_PSW_VM				_ONEBIT32L(17) // Virtual-8086 Mode
#define X86_PSW_AC				_ONEBIT32L(18) // Alignment Check
#define X86_PSW_VIF				_ONEBIT32L(19) // Virtual Interrupt Flag
#define X86_PSW_VIP				_ONEBIT32L(20) // Virtual Interrupt Pending
#define X86_PSW_ID				_ONEBIT32L(21) // ID Flag

#endif /* __X86_EFLAGS_H__ */