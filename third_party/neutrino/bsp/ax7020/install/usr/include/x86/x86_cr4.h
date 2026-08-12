/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_cr4.h - x86 CR4 Control Register
 */

#ifndef __X86_CR4_H__
#define __X86_CR4_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

/*
 * CR4 flags
 */
#define X86_CR4_VME				_ONEBIT32L(0) // Virtual 8086 Mode Extensions
#define X86_CR4_PVI				_ONEBIT32L(1) // Protected-mode Virtual Interrupts
#define X86_CR4_TSD				_ONEBIT32L(1) // Time Stamp Disable
#define X86_CR4_DE				_ONEBIT32L(3) // Debugging Extensions
#define X86_CR4_PSE				_ONEBIT32L(4) // Page Size Extension
#define X86_CR4_PAE				_ONEBIT32L(5) // Physical Address Extension
#define X86_CR4_MCE				_ONEBIT32L(6) // Machine Check Exception
#define X86_CR4_PGE				_ONEBIT32L(7) // Page Global Enabled
#define X86_CR4_PCE				_ONEBIT32L(8) // Performance-Monitoring Counter enable
#define X86_CR4_OSFXSR			_ONEBIT32L(9) // Operating system support for FXSAVE and FXRSTOR instructions
#define X86_CR4_OSXMMEXCPT		_ONEBIT32L(10) // Operating System Support for Unmasked SIMD Floating-Point Exceptions
#define X86_CR4_VMXE			_ONEBIT32L(13) // Virtual Machine Extensions Enable
#define X86_CR4_SMX				_ONEBIT32L(14) // Safer Mode Extensions Enable
#define X86_CR4_PCIDE			_ONEBIT32L(17) // PCID Enable
#define X86_CR4_OSXSAVE			_ONEBIT32L(18) // XSAVE and Processor Extended States Enable
#define X86_CR4_SMEP			_ONEBIT32L(20) // Supervisor Mode Execution Protection Enable
#define X86_CR4_SMAP			_ONEBIT32L(21) // Supervisor Mode Access Prevention Enable

#endif /* __X86_CR4_H__ */