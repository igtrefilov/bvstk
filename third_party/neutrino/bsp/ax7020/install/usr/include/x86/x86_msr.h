/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_msr.h - x86 MSR (Model-Specific Register)
 */

#ifndef __X86_MSR_H__
#define __X86_MSR_H__

#include <stdint.h>

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

#include _NTO_HDR_(x86/priv.h)

/*
 * TSC (Time Stamp Counter Register)
 */
#define X86_TSC				0x00000010 // Time Stamp Counter value
#define X86_TSC_ADJUST		0x0000003B // Time Stamp Counter adjustment
#define X86_TSC_AUX			0xC0000103 // Processor ID value
#define X86_TSC_DEADLINE	0x000006E0 // Time Stamp Counter deadline
#define X86_TSC_MPERF		0x000000E7 // Time Stamp Counter max frequency clock count
#define X86_TSC_APERF		0x000000E8 // Time Stamp Counter Actual freauency clock count

static inline uint64_t
x86_msr_tsc_aux_get(void) {
	return (rdmsr(X86_TSC_AUX));
}

static inline void
x86_msr_tsc_aux_set(uint64_t cpu_id) {
	wrmsr(X86_TSC_AUX, cpu_id);
}

/*
 * Local APIC
 */
#define X86_MSR_APICBASE				0x0000001B
#define X86_MSR_APICBASE_BSP			_ONEBIT32L(8)
#define X86_MSR_APICBASE_EXTD			_ONEBIT32L(10)
#define X86_MSR_APICBASE_ENABLE			_ONEBIT32L(11)
#define	X86_MSR_GET_APIC_BASE(x)		((x) & 0xFFFFF000) // Get LAPIC base address MSR_APICBASE[12:51]

static inline uint64_t
x86_msr_apic_base_get(void) {
	return (rdmsr(X86_MSR_APICBASE));
}

static inline void
x86_msr_apic_base_set(const uint64_t apic_base) {
	wrmsr(X86_MSR_APICBASE, apic_base);
}

/*
 * EFER (Extended Feature Enable Register)
 */
#define X86_MSR_EFER		0xC0000080
#define X86_EFER_SCE		_ONEBIT32L(0) // System Call Extensions
#define X86_EFER_LME		_ONEBIT32L(8) // Long Mode Enable
#define X86_EFER_LMA		_ONEBIT32L(10) // Long Mode Active
#define X86_EFER_NXE		_ONEBIT32L(11) // No-Execute Enable

static inline void
x86_msr_efer_lme_unset(void) {
	wrmsr(X86_MSR_EFER, rdmsr(X86_MSR_EFER) & ~X86_EFER_LME);
}

static inline void
x86_msr_efer_nx_set(void) {
	wrmsr(X86_MSR_EFER, rdmsr(X86_MSR_EFER) | X86_EFER_NXE);
}

/*
 * SYSENTER and SYSEXIT
 */
#define X86_MSR_SEP_SEL     0x174 // base selector for SYSENTER CS/SS and SYSEXIT CS/SS
#define X86_MSR_SEP_ESP     0x175 // target ESP
#define X86_MSR_SEP_EIP     0x176 // target EIP

static inline void
x86_msr_sep_sel_set(const uint64_t segment_selector) {
	wrmsr(X86_MSR_SEP_SEL, segment_selector);
}

static inline void
x86_msr_sep_esp_set(const uint64_t esp) {
	wrmsr(X86_MSR_SEP_ESP, esp);
}

static inline void
x86_msr_sep_eip_set(const uint64_t eip) {
	wrmsr(X86_MSR_SEP_EIP, eip);
}

/*
 * SYSCALL and SYSRET
 */
#define X86_MSR_STAR        0xC0000081 // System Call Target Address ([63:48] - base selector for SYSRET CS/SS | [47:32] - base selector for SYSCALL CS/SS | [13:0] - target EIP)
#define X86_MSR_LSTAR       0xC0000082 // IA-32e Mode System Call Target Address (Target RIP for the called procedure when SYSCALL is executed in 64-bit mode)
#define X86_MSR_CSTAR       0xC0000083 // IA-32e Mode System Call Target Address (Target RIP for the called procedure when SYSCALL is executed in Compatability mode) (Not used)
#define X86_MSR_FMASK       0xC0000084 // System Call Flag Mask (RFLAGS mask for SYSCALL)

/*
 * PAT (Page Attribute Table)
 */
#define X86_MSR_PAT         0x277
#define X86_PAT_UC          UINT64_C(0x00)
#define X86_PAT_WC          UINT64_C(0x01)
#define X86_PAT_WT          UINT64_C(0x04)
#define X86_PAT_WP          UINT64_C(0x05)
#define X86_PAT_WB          UINT64_C(0x06)
#define X86_PAT_UCM         UINT64_C(0x07) // PAT-only UC_WEAK can be overridden by WC from MTRRs
#define X86_PAT_MASK        UINT64_C(0x07)

/*
 * MTRR (Memory Type Range Registers)
 */
#define X86_MSR_MTRRCAP					0xfe
#define X86_MTRR_CAP_WC					_ONEBIT64L(10)
#define X86_MTRR_CAP_FIX				_ONEBIT64L(8)
#define X86_MTRR_CAP_NVAR_MASK			0x000000ff

#define X86_MSR_MTRR_DEFTYPE			0x2ff
#define X86_MTRR_DEFTYPE_FE				_ONEBIT64L(10)
#define X86_MTRR_DEFTYPE_E				_ONEBIT64L(11)
#define X86_MTRR_DEFTYPE_MASK			0x000000ff

/*
 * Variable Range MTRRs
 */
#define X86_MSR_MTRR_PHYSBASE0			0x200
#define X86_MTRR_PHYSBASE_BASE_MASK		(((uint64_t)0x0000000f << 32) | (uint64_t)0xfffff000)
#define X86_MTRR_PHYSMASK_RANGE_MASK	(((uint64_t)0x0000000f << 32) | (uint64_t)0xfffff000)
#define X86_MTRR_PHYSBASE_TYPE_MASK		0x000000ff

#define X86_MSR_MTRR_PHYSMASK0			0x201
#define X86_MTRR_PHYSMASK_VALID			_ONEBIT64L(11)

#define X86_MTRR_TYPE_UNCACHEABLE		0
#define X86_MTRR_TYPE_WRITECOMBINING	1
#define X86_MTRR_TYPE_WRITETHROUGH		4
#define X86_MTRR_TYPE_WRITEPROTECTED	5
#define X86_MTRR_TYPE_WRITEBACK			6

/*
 * Microsoft Hyper-V
 */
#define X86_MSR_HYPER_V_APIC_FREQUENCY	0x40000023

static inline uint64_t
x86_msr_hyperv_get_apic_frequency(void) {
	return (rdmsr(X86_MSR_HYPER_V_APIC_FREQUENCY));
}

#endif /* __X86_MSR_H__ */