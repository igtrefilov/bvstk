/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/idt.h
 */

#ifndef __X86_IDT_H__
#define __X86_IDT_H__

#define IDT_EXCEPTION_DE				0x00	// Divide Error
#define IDT_EXCEPTION_DB				0x01	// Debug Exception
#define IDT_EXCEPTION_NMI				0x02	// NMI
#define IDT_EXCEPTION_BP				0x03	// Breakpoint
#define IDT_EXCEPTION_OF				0x04	// Overflow
#define IDT_EXCEPTION_BR				0x05	// BOUND Range Exceeded
#define IDT_EXCEPTION_UD				0x06	// Invalid Opcode
#define IDT_EXCEPTION_NM				0x07	// No Math Coprocessor
#define IDT_EXCEPTION_DF				0x08	// Double Fault
#define IDT_EXCEPTION_RESERVED_9		0x09
#define IDT_EXCEPTION_TS				0x0A	// Invalid TSS
#define IDT_EXCEPTION_NP				0x0B	// Segment Not Present
#define IDT_EXCEPTION_SS				0x0C	// Stack Segment Fault
#define IDT_EXCEPTION_GP				0x0D	// General Protection
#define IDT_EXCEPTION_PF				0x0E	// Page Fault
#define IDT_EXCEPTION_RESERVED_15		0x0F
#define IDT_EXCEPTION_MF				0x10	// Math Fault
#define IDT_EXCEPTION_AC				0x11	// Alignment Check
#define IDT_EXCEPTION_MC				0x12	// Machine Check
#define IDT_EXCEPTION_XM				0x13	// SIMD Exception
#define IDT_EXCEPTION_VE				0x14	// Virtualization Exception
#define IDT_EXCEPTION_CP				0x15	// Control Protection Exception
#define IDT_EXCEPTION_RESERVED_22		0x16
#define IDT_EXCEPTION_RESERVED_23		0x17
#define IDT_EXCEPTION_RESERVED_24		0x18
#define IDT_EXCEPTION_RESERVED_25		0x19
#define IDT_EXCEPTION_RESERVED_26		0x1A
#define IDT_EXCEPTION_RESERVED_27		0x1B
#define IDT_EXCEPTION_RESERVED_28		0x1C
#define IDT_EXCEPTION_RESERVED_29		0x1D
#define IDT_EXCEPTION_RESERVED_30		0x1E
#define IDT_EXCEPTION_RESERVED_31		0x1F
#define IDT_EXCEPTION_FREE				0x20

#define IDT_GATE_DPL0					0
#define IDT_GATE_DPL1					1
#define IDT_GATE_DPL2					2
#define IDT_GATE_DPL3					3

#define GATE_TYPE_16_BIT_TSS_AVAILABLE	0x1
#define GATE_TYPE_16_BIT_TSS_BUSY		0x3
#define GATE_TYPE_16_BIT_CALL			0x4
#define GATE_TYPE_16_BIT_INTERRUPT		0x6
#define GATE_TYPE_16_BIT_TRAP			0x7

#define GATE_TYPE_32_BIT_TSS_AVAILABLE	0x9
#define GATE_TYPE_32_BIT_TSS_BUSY		0xB
#define GATE_TYPE_32_BIT_CALL			0xC
#define GATE_TYPE_32_BIT_INTERRUPT		0xE
#define GATE_TYPE_32_BIT_TRAP			0xF

#define GATE_TYPE_64_BIT_TSS_AVAILABLE	0x9
#define GATE_TYPE_64_BIT_TSS_BUSY		0xB
#define GATE_TYPE_64_BIT_CALL			0xC
#define GATE_TYPE_64_BIT_INTERRUPT		0xE
#define GATE_TYPE_64_BIT_TRAP			0xF

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stdbool.h>
#include <x86/priv.h>
#include <sys/syspage.h>

typedef struct {
	uint64_t
		offset_low			: 16,
		selector			: 16,
		__reserved1			:  8,
		type				:  4,
		__reserved2			:  1,
		dpl					:  2,
		present				:  1,
		offset_middle		: 16;
#ifdef __X86_64__
	uint64_t
		offset_high			: 32,
		__reserved3			: 32;
#endif
} idt_gate_descriptor;

typedef struct {
	uint16_t	limit;
#if defined(__X86_64__)
	uint64_t	base;
#else /* __X86__ */
	uint32_t	base;
#endif
} __attribute__((packed)) idt_info;

////////////////////////////////////////////////////////////////////////////////
/// IDT accessors
////////////////////////////////////////////////////////////////////////////////

static inline idt_gate_descriptor
idt_read(const uint8_t exc) {
	idt_info idt;
	sidt((const uintptr_t)&idt);
	return ((volatile idt_gate_descriptor *)idt.base)[exc];
}

static inline void
idt_write(const uint8_t exc, const idt_gate_descriptor value) {
	((volatile idt_gate_descriptor *)_syspage_ptr->un.x86.idt)[exc] = value;
}

////////////////////////////////////////////////////////////////////////////////
/// IDT helpers
////////////////////////////////////////////////////////////////////////////////

static inline uintptr_t
idt_get_idtr_base_address(void) {
	idt_info idt;
	sidt((const uintptr_t)&idt);
	return idt.base;
}

static inline uint16_t
idt_get_idtr_limit(void) {
	idt_info idt;
	sidt((const uintptr_t)&idt);
	return idt.limit;
}

static inline uint16_t
idt_get_number_of_exceptions(void) {
	return (1+idt_get_idtr_limit())/sizeof(idt_gate_descriptor);
}

static inline uintptr_t
idt_gate_get_offset(const uint8_t trap) {
	idt_gate_descriptor reg;

	reg = idt_read(trap);
	return
#ifdef __X86_64__
	(((uintptr_t)reg.offset_high) << 32) |
#endif
	(((uintptr_t)reg.offset_middle) << 16) | reg.offset_low;
}

static inline uint16_t
idt_gate_get_selector(const uint8_t exc) {
	return idt_read(exc).selector;
}

static inline void
idt_gate_set_selector(const uint8_t exc, const uint16_t selector) {
	idt_gate_descriptor reg;

	reg = idt_read(exc);
	reg.selector  = selector;
	idt_write(exc, reg);
}

static inline bool
idt_gate_get_present_flag(const uint8_t exc) {
	return 1 == idt_read(exc).present ? true : false;
}

static inline uint8_t
idt_gate_get_dpl(const uint8_t exc) {
	return idt_read(exc).dpl;
}

static inline void
idt_gate_set_dpl(const uint8_t exc, const uint8_t dpl) {
	idt_gate_descriptor reg;

	reg = idt_read(exc);
	reg.dpl = dpl;
	idt_write(exc, reg);
}

static inline uint8_t
idt_gate_get_type(const uint8_t exc) {
	return idt_read(exc).type;
}

static inline void
idt_interrupt_gate_clear(const uint8_t exc) {
	idt_gate_descriptor reg = {0};
	idt_write(exc, reg);
}

static inline void
idt_interrupt_gate_set(const uint8_t exc, const uintptr_t offset, const uint16_t selector, const uint8_t dpl) {
	idt_gate_descriptor reg = {0};

	reg.offset_low = UINT16_MAX & offset;
	reg.selector  = selector;
	reg.type = GATE_TYPE_32_BIT_INTERRUPT;
	reg.dpl = dpl;
	reg.present = 1;
	reg.offset_middle = (uint16_t)(offset >> 16);
#ifdef __X86_64__
	reg.offset_high = (uint32_t)(offset >> 32);
#endif
	idt_write(exc, reg);
}

static inline void
idt_trap_gate_set(const uint8_t exc, const uintptr_t offset, const uint16_t selector, const uint8_t dpl) {
	idt_gate_descriptor reg = {0};

	reg.offset_low = UINT16_MAX & offset;
	reg.selector  = selector;
	reg.type = GATE_TYPE_32_BIT_TRAP;
	reg.dpl = dpl;
	reg.present = 1;
	reg.offset_middle = (uint16_t)(offset >> 16);
#ifdef __X86_64__
	reg.offset_high = (uint32_t)(offset >> 32);
#endif
	idt_write(exc, reg);
}

static inline bool
idt_exception_is_trap(const uint8_t exc) {
	//return ((IDT_EXCEPTION_FREE >= exc) && (IDT_EXCEPTION_NMI != exc)) ? true : false;
	return false; // To handle all exceptions beside NM with disabled interrupts
}

static inline bool
idt_exception_is_interrupt(const uint8_t exc) {
	//return ((IDT_EXCEPTION_FREE < exc) || (IDT_EXCEPTION_NMI == exc)) ? true : false;
	return true; // To handle all exceptions beside NM with disabled interrupts
}

////////////////////////////////////////////////////////////////////////////////

extern void __ker_entry(void);

extern void __exc0(void), __exc1(void), __exc3(void), __exc4(void);
extern void __exc5(void), __exc6(void), __exc7(void), __exc8(void);
extern void __exc9(void), __exca(void), __excb(void), __excc(void);
extern void __excd(void), __exce(void), __excf(void), __exc10(void);
extern void __exc11(void), __exc12(void);
extern void __exc7emul(void), __exc7emul_end(void);
extern void __intr_unexpected(void);

extern void __fault_error(void), __fault_noerror(void), __fault_crash(void);
extern void __ipi1(void), __ipi2(void); // SMP interprocessor interrupt vectors

extern uintptr_t __fpuemu_stub;

extern void idt_init(void);
#ifdef EXTRADEBUG
extern void idt_debug(const uint8_t core);
#else
#define idt_debug(core)
#endif

#endif /* !(__ASSEMBLER__) */
#endif	/* __X86_IDT_H__ */
