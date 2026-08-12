/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/lapic.h
 */

#ifndef __X86_LAPIC_H__
#define __X86_LAPIC_H__

#define LAPIC_ID							0x0020
#define LAPIC_VERSION						0x0030
#define LAPIC_TPR							0x0080
#define LAPIC_APR							0x0090
#define LAPIC_PPR							0x00A0
#define LAPIC_EOI							0x00B0
#define LAPIC_LDR							0x00D0
#define LAPIC_DFR							0x00E0
#define LAPIC_SVR							0x00F0
#define LAPIC_ISR							0x0100
#define LAPIC_TMR							0x0180
#define LAPIC_IRR							0x0200
#define LAPIC_ESR							0x0280
#define LAPIC_CMCI							0x02F0
#define LAPIC_ICR							0x0300 // LAPIC_ICRL
#define LAPIC_ICR2							0x0310 // LAPIC_ICRH
#define LAPIC_LVTT							0x0320 // LAPIC_LTR
#define LAPIC_LTMR							0x0330
#define LAPIC_LPCR							0x0340
#define LAPIC_LVT0							0x0350 // LAPIC_LL0R
#define LAPIC_LVT1							0x0360 // LAPIC_LL1R
#define LAPIC_LVERR							0x0370 // LAPIC_LER
#define LAPIC_TMICT							0x0380 // LAPIC_ICR
#define LAPIC_TMCCT							0x0390 // LAPIC_CCR
#define LAPIC_TDCR							0x03E0
#define LAPIC_SELF_IPI						0x03F0 // Only in x2APIC mode

#define LAPIC_DFR_ACCESS_MODEL_CLUSTER		0
#define LAPIC_DFR_ACCESS_MODEL_FLAT			0xF

#define LAPIC_LVTT_MODE_ONESHOOT			0
#define LAPIC_LVTT_MODE_PERIODIC			1

#define LAPIC_LVTT_MASK_MASK				0x10000

#define LAPIC_LVERR_MASK_MASK				0x10000

#define LAPIC_TDCR_DIV_2					0
#define LAPIC_TDCR_DIV_4					1
#define LAPIC_TDCR_DIV_8					2
#define LAPIC_TDCR_DIV_16					3
#define LAPIC_TDCR_DIV_32					4
#define LAPIC_TDCR_DIV_64					5
#define LAPIC_TDCR_DIV_128					6
#define LAPIC_TDCR_DIV_1					7

#define LAPIC_ICR_SHORTHAND_NO				0
#define LAPIC_ICR_SHORTHAND_SELF			1
#define LAPIC_ICR_SHORTHAND_ALL_INC_SELF	2
#define LAPIC_ICR_SHORTHAND_ALL_EXC_SELF	3

#define LAPIC_ICR_TRIGGER_MODE_EDGE			0
#define LAPIC_ICR_TRIGGER_MODE_LEVEL		1

#define LAPIC_ICR_LEVEL_DEASSERT			0
#define LAPIC_ICR_LEVEL_ASSERT				1

#define LAPIC_ICR_DESTINATION_MODE_PHYS		0
#define LAPIC_ICR_DESTINATION_MODE_LOG		1

#define LAPIC_ICR_DELIVERY_MODE_FIXED		0
#define LAPIC_ICR_DELIVERY_MODE_LOWEST		1
#define LAPIC_ICR_DELIVERY_MODE_SMI			2
#define LAPIC_ICR_DELIVERY_MODE_NMI			4
#define LAPIC_ICR_DELIVERY_MODE_INIT		5
#define LAPIC_ICR_DELIVERY_MODE_STARTUP		6

// #define LVT_VECTOR_MASK						0xff

#define LVT_TIMER_MODE_PERIODIC				(1 << 17)

#define LAPIC_LVTT_MODE_ONESHOOT			0
#define LAPIC_LVTT_MODE_PERIODIC			1
#define LAPIC_LVTT_MODE_TSC_DEADLINE		2
#define LAPIC_LVTT_MODE_TSC_RESERVED		3

#define LAPIC_LVTT_MASK_NOT_MASKED_BIT		(0 << 16)
#define LAPIC_LVTT_MASK_MASKED_BIT			(1 << 16)

// #define SVR_APIC_ENABLED					(1 << 8)
#define SVR_EOI_BROADCAST_DISABLED			(1 << 12)

// Use a hardcoded value to ensure we handle the P6 and Pentium restriction that the bottom 4 bits of the vector have to be all one's.
#define LAPIC_MAX_VECTOR					0xFF
#define LAPIC_SPURIOUS_VECTOR				LAPIC_MAX_VECTOR
#define LAPIC_ERROR_VECTOR					(LAPIC_MAX_VECTOR - 1)	// 0xFE
#define LAPIC_SYSTIMER_VECTOR				(LAPIC_MAX_VECTOR - 2)	// 0xFD
#define LAPIC_IPI2_VECTOR					(LAPIC_MAX_VECTOR - 3)	// 0xFC
#define LAPIC_IPI1_VECTOR					(LAPIC_MAX_VECTOR - 4)	// 0xFB
#define LAPIC_LVT0_VECTOR					(LAPIC_MAX_VECTOR - 5)	// 0xFA
#define LAPIC_LVT1_VECTOR					(LAPIC_MAX_VECTOR - 6)	// 0xF9
// #define LAPIC_PERFTIMER_VECTOR				(LAPIC_MAX_VECTOR - 7)	// 0xF8
// #define LAPIC_TEMPSENSOR_VECTOR				(LAPIC_MAX_VECTOR - 8)	// 0xF7

#ifndef	__ASSEMBLER__
#include <stdint.h>
#include <stdbool.h>

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			__reserved			: 24,
			lapicn				:  8;
	} __attribute__((__packed__)) bits;
} lapic_id_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			version				:  8,
			__reserved_1		:  8,
			lvt					:  8,
			broadcast			:  1,
			__reserved_2		:  7;
	} __attribute__((__packed__)) bits;
} lapic_version_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			priority			:  8,
			__reserved			: 24;
	} __attribute__((__packed__)) bits;
} lapic_tpr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			priority			:  8,
			__reserved			: 24;
	} __attribute__((__packed__)) bits;
} lapic_apr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			priority			:  8,
			__reserved			: 24;
	} __attribute__((__packed__)) bits;
} lapic_ppr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			__reserved			: 24,
			destination			:  8;
	} __attribute__((__packed__)) bits;
} lapic_ldr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			__reserved			: 28,
			access_model		:  4;
	} __attribute__((__packed__)) bits;
} lapic_dfr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			spurious_vector		:  8,
			soft_en				:  1,
			focus_cpu			:  1,
			__reserved1			:  2,
			broadcast_suppress	:  1,
			__reserved2			: 19;
	} __attribute__((__packed__)) bits;
} lapic_svr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			vector				:  8,
			delivery_mode		:  3,
			destination_mode	:  1,
			delivery_status		:  1,
			__reserved_1		:  1,
			level				:  1,
			trigger				:  1,
			__reserved_2		:  2,
			shorthand			:  2,
			__reserved_3		: 12;
	} __attribute__((__packed__)) bits;
} lapic_icr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			__reserved			: 24,
			destination			:  8;
	} __attribute__((__packed__)) bits;
} lapic_icr2_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			vector				:  8,
			__reserved_1		:  4,
			delivery_status		:  1,
			__reserved_2		:  3,
			mask				:  1,
			mode				:  2,
			__reserved_3		: 13;
	} __attribute__((__packed__)) bits;
} lapic_lvtt_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			vector				:  8,
			delivery_mode		:  3,
			__reserved_1		:  1,
			delivery_status		:  1,
			polarity			:  1,
			request				:  1,
			mode				:  1,
			mask				:  1,
			__reserved_2		: 15;
	} __attribute__((__packed__)) bits;
} lapic_lvt_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			vector				:  8,
			__reserved_1		:  4,
			delivery_status		:  1,
			__reserved_2		:  3,
			mask				:  1,
			__reserved_3		: 15;
	} __attribute__((__packed__)) bits;
} lapic_lverr_t;

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			divider1			:  2,
			__reserved_1		:  1,
			divider2			:  1,
			__reserved_2		: 28;
	} __attribute__((__packed__)) bits;
} lapic_tdcr_t;

////////////////////////////////////////////////////////////////////////////////
/// LAPIC accessors
////////////////////////////////////////////////////////////////////////////////
extern const uintptr_t lapic_get_address(void);

static inline uint32_t
lapic_read(const uint16_t reg) {
	return (*(volatile uint32_t *)(lapic_get_address() + reg));
}

static inline void
lapic_write(const uint16_t reg, const uint32_t val) {
	*(volatile uint32_t *)(lapic_get_address() + reg) = val;
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC ID register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_get_lapic_number(void) {
	lapic_id_t reg;

	reg.raw = lapic_read(LAPIC_ID);
	return (reg.bits.lapicn);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC version register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_get_version(void) {
	lapic_version_t reg;

	reg.raw = lapic_read(LAPIC_VERSION);
	return (reg.bits.version);
}

static inline uint8_t
lapic_get_lvt(void) {
	lapic_version_t reg;

	reg.raw = lapic_read(LAPIC_VERSION);
	return (reg.bits.lvt);
}

static inline bool
lapic_is_broadcast_suppression_supported(void) {
	lapic_version_t reg;

	reg.raw = lapic_read(LAPIC_VERSION);
	return (1 == reg.bits.broadcast ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC TPR register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_get_task_priority(void) {
	lapic_tpr_t reg;

	reg.raw = lapic_read(LAPIC_TPR);
	return (reg.bits.priority);
}

static inline void
lapic_set_task_priority(const uint8_t priority) {
	lapic_tpr_t reg;

	reg.raw = lapic_read(LAPIC_TPR);
	reg.bits.priority = priority;
	lapic_write(LAPIC_TPR, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC DFR register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_get_access_model(void) {
	lapic_dfr_t reg;

	reg.raw = lapic_read(LAPIC_DFR);
	return (reg.bits.access_model);
}

static inline void
lapic_set_access_model(uint8_t access_model) {
	lapic_dfr_t reg;

	reg.raw = lapic_read(LAPIC_DFR);
	reg.bits.access_model = access_model;
	lapic_write(LAPIC_DFR, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC SVR register helpers
////////////////////////////////////////////////////////////////////////////////

static inline void
lapic_disable(void) {
	lapic_svr_t reg;

	reg.raw = lapic_read(LAPIC_SVR);
	reg.bits.soft_en = 0;
	lapic_write(LAPIC_SVR, reg.raw);
}

static inline void
lapic_enable(void) {
	lapic_svr_t reg;

	reg.raw = lapic_read(LAPIC_SVR);
	reg.bits.soft_en = 1;
	lapic_write(LAPIC_SVR, reg.raw);
}

static inline void
lapic_spurious_set_vector(const uint8_t vector) {
	lapic_svr_t reg;

	reg.raw = lapic_read(LAPIC_SVR);
	reg.bits.spurious_vector = vector;
	lapic_write(LAPIC_SVR, reg.raw);
}

static inline bool
lapic_is_broadcast_suppression_enabled(void) {
	lapic_svr_t reg;

	reg.raw = lapic_read(LAPIC_SVR);
	return (1 == reg.bits.broadcast_suppress ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC ICR register helpers
////////////////////////////////////////////////////////////////////////////////

static inline bool
lapic_is_interrupt_pending(void) {
	lapic_icr_t reg;

	reg.raw = lapic_read(LAPIC_ICR);
	return (1 == reg.bits.delivery_status ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC ICR2 register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_get_interrupt_destination(void) {
	lapic_icr2_t reg;

	reg.raw = lapic_read(LAPIC_ICR2);
	return (reg.bits.destination);
}

static inline void
lapic_set_interrupt_destination(const uint8_t destination) {
	lapic_icr2_t reg;

	reg.raw = 0; // Don't preserve reserved bits
	reg.bits.destination = destination;
	lapic_write(LAPIC_ICR2, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC LVTT register helpers
////////////////////////////////////////////////////////////////////////////////

static inline void
lapic_timer_set_vector(const uint8_t vector) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.vector = vector;
	lapic_write(LAPIC_LVTT, reg.raw);
}

static inline void
lapic_timer_disable_interrupt(void) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.mask = 1;
	lapic_write(LAPIC_LVTT, reg.raw);
}

static inline void
lapic_timer_enable_interrupt(void) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.mask = 0;
	lapic_write(LAPIC_LVTT, reg.raw);
}

static inline void
lapic_timer_set_mode(const uint8_t mode) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.mode = mode;
	lapic_write(LAPIC_LVTT, reg.raw);
}

static inline void
lapic_timer_set_mode_oneshot(void) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.mode = LAPIC_LVTT_MODE_ONESHOOT;
	lapic_write(LAPIC_LVTT, reg.raw);
}

static inline void
lapic_timer_set_mode_periodic(void) {
	lapic_lvtt_t reg;

	reg.raw = lapic_read(LAPIC_LVTT);
	reg.bits.mode = LAPIC_LVTT_MODE_PERIODIC;
	lapic_write(LAPIC_LVTT, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC LVT0 register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_lvt0_get_delivery_mode(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT0);
	return (reg.bits.delivery_mode);
}

static inline void
lapic_lvt0_set_delivery_mode(const uint8_t mode) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT0);
	reg.bits.delivery_mode = mode;
	lapic_write(LAPIC_LVT0, reg.raw);
}

static inline void
lapic_lvt0_set_vector(const uint8_t vector) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT0);
	reg.bits.vector = vector;
	lapic_write(LAPIC_LVT0, reg.raw);
}

static inline void
lapic_lvt0_disable_interrupt(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT0);
	reg.bits.mask = 1;
	lapic_write(LAPIC_LVT0, reg.raw);
}

static inline bool
lapic_is_lvt0_interrupt_disabled(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT0);
	return (1 == reg.bits.mask ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC LVT1 register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_lvt1_get_delivery_mode(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT1);
	return (reg.bits.delivery_mode);
}

static inline void
lapic_lvt1_set_delivery_mode(const uint8_t mode) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT1);
	reg.bits.delivery_mode = mode;
	lapic_write(LAPIC_LVT1, reg.raw);
}

static inline void
lapic_lvt1_set_vector(const uint8_t vector) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT1);
	reg.bits.vector = vector;
	lapic_write(LAPIC_LVT1, reg.raw);
}

static inline void
lapic_lvt1_disable_interrupt(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT1);
	reg.bits.mask = 1;
	lapic_write(LAPIC_LVT1, reg.raw);
}

static inline bool
lapic_lvt1_is_interrupt_disabled(void) {
	lapic_lvt_t reg;

	reg.raw = lapic_read(LAPIC_LVT1);
	return (1 == reg.bits.mask ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC LVERR register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_error_get_vector(void) {
	lapic_lverr_t reg;

	reg.raw = lapic_read(LAPIC_LVERR);
	return (reg.bits.vector);
}

static inline void
lapic_error_set_vector(const uint8_t vector) {
	lapic_lverr_t reg;

	reg.raw = lapic_read(LAPIC_LVERR);
	reg.bits.vector = vector;
	lapic_write(LAPIC_LVERR, reg.raw);
}

static inline void
lapic_error_disable_interrupt(void) {
	lapic_lverr_t reg;

	reg.raw = lapic_read(LAPIC_LVERR);
	reg.bits.mask = 1;
	lapic_write(LAPIC_LVERR, reg.raw);
}

static inline void
lapic_error_enable_interrupt(void) {
	lapic_lverr_t reg;

	reg.raw = lapic_read(LAPIC_LVERR);
	reg.bits.mask = 0;
	lapic_write(LAPIC_LVERR, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC timer initial count register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t
lapic_timer_get_init_value(void) {
	return lapic_read(LAPIC_TMICT);
}

static inline void
lapic_timer_set_init_value(const uint32_t value) {
	return lapic_write(LAPIC_TMICT, value);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC timer current register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t
lapic_timer_get_current_value(void) {
	return lapic_read(LAPIC_TMCCT);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC timer divider register helpers
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t
lapic_timer_get_divider(void) {
	lapic_tdcr_t reg;

	reg.raw = lapic_read(LAPIC_TDCR);
	return (reg.bits.divider2 << 2 | reg.bits.divider1);
}

static inline void
lapic_timer_set_divider(const uint8_t divider) {
	lapic_tdcr_t reg;

	reg.raw = lapic_read(LAPIC_TDCR);
	reg.bits.divider1 = divider & 3;
	reg.bits.divider2 = (divider & (1 << 2)) >> 2;
	lapic_write(LAPIC_TDCR, reg.raw);
}

////////////////////////////////////////////////////////////////////////////////
/// LAPIC helpers
////////////////////////////////////////////////////////////////////////////////

static inline void
lapic_start_ipi(const uint8_t destination, const uint16_t vector) {
	lapic_set_interrupt_destination(destination);

	lapic_icr_t reg;
	reg.raw = 0;
	reg.bits.vector = vector;
	lapic_write(LAPIC_ICR, reg.raw);
}

static inline void
lapic_send_command(const uint8_t destination,
		const uint8_t shorthand,
		const bool trigger,
		const bool level,
		const uint8_t destination_mode,
		const uint8_t delivery_mode,
		const uint16_t vector) {
	lapic_set_interrupt_destination(destination);

	lapic_icr_t reg;
	reg.raw = 0;
	reg.bits.shorthand = shorthand;
	reg.bits.trigger = trigger;
	reg.bits.level = level;
	reg.bits.destination_mode = destination_mode;
	reg.bits.delivery_mode = delivery_mode;
	reg.bits.vector = vector;
	lapic_write(LAPIC_ICR, reg.raw);

	while (lapic_is_interrupt_pending());
}

static inline void
lapic_clean_irqs(void) {
	/*
	 * After a crash, we no longer service the interrupts and a pending
	 * interrupt from previous kernel might still have ISR bit set.
	 *
	 * Most probably by now CPU has serviced that pending interrupt and
	 * it might not have done the ack_APIC_irq() because it thought,
	 * interrupt came from i8259 as ExtInt. LAPIC did not get EOI so it
	 * does not clear the ISR bit and CPU thinks it has already serviced
	 * the interrupt. Hence a vector might get locked. It was noticed
	 * for timer IRQ. Issue an extra EOI to clear ISR.
	 */
	#define LAPIC_ISR_NR	8	/* Number of 32 bit ISR registers. */

	for (uint8_t i = 0; i < LAPIC_ISR_NR; i++) {
		uint32_t val = lapic_read(LAPIC_ISR + (i << sizeof(uint32_t)));
		for (uint8_t j = 0; j < 8 * sizeof(uint32_t); j++) {
			if (val & (1UL << j))
				lapic_write(LAPIC_EOI, 0);
		}
	}
}
#endif /* __ASSEMBLER__ */
#endif /* __X86_LAPIC_H__ */
