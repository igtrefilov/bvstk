/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/gdt.h
 */

#ifndef __X86_GDT_H__
#define __X86_GDT_H__

typedef enum {
	X86_GDT_INDEX_NULL,
	X86_GDT_INDEX_GDT,
	X86_GDT_INDEX_IDT,
	X86_GDT_INDEX_LDT,
	X86_GDT_INDEX_V86,
	X86_GDT_INDEX_KERNEL_CODE,
	X86_GDT_INDEX_KERNEL_DATA,
	X86_GDT_INDEX_SYS_CODE,
	X86_GDT_INDEX_SYS_DATA,
	X86_GDT_INDEX_SYSENTER_CODE,
	X86_GDT_INDEX_SYSENTER_DATA,
	X86_GDT_INDEX_SYSEXIT_CODE,
	X86_GDT_INDEX_SYSEXIT_DATA,
	X86_GDT_INDEX_TSS = 16,
	X86_GDT_INDEX_CPUPAGE = 48
} x86_gdt_index_t;

typedef enum {
	X86_LDT_INDEX_NULL,
	X86_LDT_INDEX_USER_CODE,
	X86_LDT_INDEX_USER_DATA
} x86_ldt_index_t;

#define GDT_DESC_GRANULARITY_BYTE			0
#define GDT_DESC_GRANULARITY_4K_BYTE		1

#define GDT_DESC_OPERATION_SIZE_16BIT		0
#define GDT_DESC_OPERATION_SIZE_32BIT		1

#define GDT_DESC_L_32BIT_SEGMENT			0
#define GDT_DESC_L_64BIT_SEGMENT			1

#define GDT_DESC_NOT_PRESENT				0
#define GDT_DESC_PRESENT					1

#define GDT_DESC_DPL0						0
#define GDT_DESC_DPL1						1
#define GDT_DESC_DPL2						2
#define GDT_DESC_DPL3						3

#define GDT_DESC_TYPE_SYSTEM				0
#define GDT_DESC_TYPE_CODE_OR_DATA			1

#define GDT_DESC_SEG_TYPE_DATA				0
#define GDT_DESC_SEG_TYPE_CODE				8
#define GDT_DESC_SEG_TYPE_ACCESS_MASK		7
#define GDT_DESC_SEG_TYPE_DATA_RD			0 // Read-Only
#define GDT_DESC_SEG_TYPE_DATA_RDA			1 // Read-Only, accessed
#define GDT_DESC_SEG_TYPE_DATA_RDWR			2 // Read/Write
#define GDT_DESC_SEG_TYPE_DATA_RDWRA		3 // Read/Write, accessed
#define GDT_DESC_SEG_TYPE_DATA_RDEXPD		4 // Read-Only, expand-down
#define GDT_DESC_SEG_TYPE_DATA_RDEXPDA		5 // Read-Only, expand-down, accessed
#define GDT_DESC_SEG_TYPE_DATA_RDWREXPD		6 // Read/Write, expand-down
#define GDT_DESC_SEG_TYPE_DATA_RDWREXPDA	7 // Read/Write, expand-down, accessed
#define GDT_DESC_SEG_TYPE_CODE_EX			0 // Execute-Only
#define GDT_DESC_SEG_TYPE_CODE_EXA			1 // Execute-Only, accessed
#define GDT_DESC_SEG_TYPE_CODE_EXRD			2 // Execute/Read
#define GDT_DESC_SEG_TYPE_CODE_EXRDA		3 // Execute/Read, accessed
#define GDT_DESC_SEG_TYPE_CODE_EXC			4 // Execute-Only, conforming
#define GDT_DESC_SEG_TYPE_CODE_EXCA			5 // Execute-Only, conforming, accessed
#define GDT_DESC_SEG_TYPE_CODE_EXRDC		6 // Execute/Read, conforming
#define GDT_DESC_SEG_TYPE_CODE_EXRDCA		7 // Execute/Read, conforming, accessed

#define GDT_SS_TABLE_INDICATOR				0 // GDT Segment Selector Table Indicator
#define LDT_SS_TABLE_INDICATOR				1 // LDT Segment Selector Table Indicator

#define GDT_SEGMENT_SELECTOR(index, rpl)	((index) | (GDT_SS_TABLE_INDICATOR << 2) | (rpl)) // [15:3] - index, [2] - table indicator, [1:0] - rpl
#define LDT_SEGMENT_SELECTOR(index, rpl)	((index) | (LDT_SS_TABLE_INDICATOR << 2) | (rpl)) // [15:3] - index, [2] - table indicator, [1:0] - rpl

#define GDT_GET_DESC_OFFSET(index)			((index) << 3)

#ifndef	__ASSEMBLER__
#include <stdint.h>

typedef union {
	uint64_t raw;
	struct {
		uint64_t
			segment_limit0		: 16,
			segment_base0		: 16,
			segment_base1		:  8,
			segment_type		:  4,
			descriptor_type		:  1,
			dpl					:  2,
			segment_present		:  1,
			segment_limit1		:  4,
			available			:  1,
			large_code_segment	:  1,
			operation_size		:  1,
			granularity			:  1,
			segment_base2		:  8;
	} __attribute__((__packed__)) bits;
} gdt_descriptor_t;

typedef struct {
	uint16_t	limit;
#if defined(__X86_64__)
	uint64_t	base;
#else /* __X86__ */
	uint32_t	base;
#endif
} __attribute__((packed)) gdt_info_t;

////////////////////////////////////////////////////////////////////////////////
/// GDT accessors
////////////////////////////////////////////////////////////////////////////////

static inline gdt_descriptor_t
gdt_read(const gdt_descriptor_t * const gdt) {
	return *(volatile gdt_descriptor_t *)(gdt);
}

static inline void
gdt_write(const gdt_descriptor_t * const gdt, const gdt_descriptor_t value) {
	*(volatile gdt_descriptor_t *)(gdt) = value;
}

////////////////////////////////////////////////////////////////////////////////
/// GDT helpers
////////////////////////////////////////////////////////////////////////////////

static inline const uint8_t
gdt_get_segment_granularity(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.granularity;
}

static inline const uint8_t
gdt_get_segment_operation_size(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.operation_size;
}

static inline const uint8_t
gdt_get_segment_large(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.large_code_segment;
}

static inline const uint8_t
gdt_get_segment_present(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.segment_present;
}

static inline const uint8_t
gdt_get_segment_dpl(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.dpl;
}

static inline const uint8_t
gdt_get_segment_descriptor_type(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.descriptor_type;
}

static inline const uint8_t
gdt_get_segment_type(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return d.bits.segment_type;
}

static inline const uint32_t
gdt_get_segment_base(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return (d.bits.segment_base2 << 24) + (d.bits.segment_base1 << 16) + d.bits.segment_base0;
}

static inline const uint32_t
gdt_get_segment_limit(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	return (d.bits.segment_limit1 << 16) + d.bits.segment_limit0;
}

static inline const uint64_t
gdt_get_segment_length(const gdt_descriptor_t *gdt) {
	gdt_descriptor_t d = gdt_read(gdt);
	const uint64_t length = (d.bits.segment_limit1 << 16) + d.bits.segment_limit0 + 1;
	if (GDT_DESC_GRANULARITY_4K_BYTE == d.bits.granularity) {
		return length * 0x1000;
	} else
		return length;
}

static inline const gdt_descriptor_t
gdt_create_segment_descriptor(const uint32_t base,
		uint64_t length,
		const uint8_t segment_type,
		const uint8_t descriptor_type,
		const uint8_t dpl,
		const uint8_t present,
		const uint8_t large,
		const uint8_t operation_size,
		const uint8_t granularity) {
	gdt_descriptor_t d;

	if (GDT_DESC_GRANULARITY_4K_BYTE == granularity) {
		if (length & 0x0FFF) length += 0x1000;
		length /= 0x1000;
	}
	d.raw = 0;
	d.bits.segment_limit0     = ((length - 1)  >>  0) & 0xFFFF;
	d.bits.segment_limit1     = ((length - 1)  >> 16) & 0x000F;
	d.bits.segment_base0      = (base          >>  0) & 0xFFFF;
	d.bits.segment_base1      = (base          >> 16) & 0x00FF;
	d.bits.segment_base2      = (base          >> 24) & 0x00FF;
	d.bits.segment_type       = (segment_type  >>  0) & 0x000F;
	d.bits.descriptor_type    = descriptor_type;
	d.bits.dpl                = (dpl           >>  0) & 0x0003;
	d.bits.segment_present    = present;
	d.bits.large_code_segment = large;
	d.bits.operation_size     = operation_size;
	d.bits.granularity        = granularity;
	return d;
}

static inline const gdt_descriptor_t
gdt_create_64bit_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t descriptor_type,
		const uint8_t dpl,
		const uint8_t granularity) {
	return gdt_create_segment_descriptor(base,
			length,
			segment_type,
			descriptor_type,
			dpl,
			GDT_DESC_PRESENT,				// present
			GDT_DESC_L_64BIT_SEGMENT,		// large_code_segment
			GDT_DESC_OPERATION_SIZE_16BIT,	// operation_size, must be zero for 64-bit segment
			granularity);
}

static inline const gdt_descriptor_t
gdt_create_64bit_system_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t granularity) {
	return gdt_create_64bit_segment_descriptor(base,
			length,
			segment_type,
			GDT_DESC_TYPE_SYSTEM,
			dpl, granularity);
}

static inline const gdt_descriptor_t
gdt_create_64bit_code_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t granularity) {
	return gdt_create_64bit_segment_descriptor(base,
			length,
			GDT_DESC_SEG_TYPE_CODE | (segment_type & GDT_DESC_SEG_TYPE_ACCESS_MASK),
			GDT_DESC_TYPE_CODE_OR_DATA,
			dpl, granularity);
}

static inline const gdt_descriptor_t
gdt_create_64bit_data_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t granularity) {
	return gdt_create_64bit_segment_descriptor(base,
			length,
			GDT_DESC_SEG_TYPE_DATA | (segment_type & GDT_DESC_SEG_TYPE_ACCESS_MASK),
			GDT_DESC_TYPE_CODE_OR_DATA,
			dpl, granularity);
}

static inline const gdt_descriptor_t
gdt_create_32bit_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t descriptor_type,
		const uint8_t dpl,
		const uint8_t operation_size,
		const uint8_t granularity) {
	return gdt_create_segment_descriptor(base,
			length,
			segment_type,
			descriptor_type,
			dpl,
			GDT_DESC_PRESENT,				// present
			GDT_DESC_L_32BIT_SEGMENT,		// large_code_segment
			operation_size,					// operation_size
			granularity);
}

static inline const gdt_descriptor_t
gdt_create_32bit_system_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t operation_size,
		const uint8_t granularity) {
	return gdt_create_32bit_segment_descriptor(base,
			length,
			segment_type,
			GDT_DESC_TYPE_SYSTEM,
			dpl, operation_size, granularity);
}

static inline const gdt_descriptor_t
gdt_create_32bit_code_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t operation_size,
		const uint8_t granularity) {
	return gdt_create_32bit_segment_descriptor(base,
			length,
			GDT_DESC_SEG_TYPE_CODE | (segment_type & GDT_DESC_SEG_TYPE_ACCESS_MASK),
			GDT_DESC_TYPE_CODE_OR_DATA,
			dpl, operation_size, granularity);
}

static inline const gdt_descriptor_t
gdt_create_32bit_data_segment_descriptor(const uint32_t base,
		const uint64_t length,
		const uint8_t segment_type,
		const uint8_t dpl,
		const uint8_t operation_size,
		const uint8_t granularity) {
	return gdt_create_32bit_segment_descriptor(base,
			length,
			GDT_DESC_SEG_TYPE_DATA | (segment_type & GDT_DESC_SEG_TYPE_ACCESS_MASK),
			GDT_DESC_TYPE_CODE_OR_DATA,
			dpl, operation_size, granularity);
}

////////////////////////////////////////////////////////////////////////////////
/// GDT register helpers
////////////////////////////////////////////////////////////////////////////////

static inline const void
gdt_setup_code_segment(uint16_t segment_offset) {
	// Set offset of CODE segment according to the GDT code descriptor
#if defined(__X86_64__)
	__asm__ __volatile__("mov %0, %%rax" : : "r"((uint64_t)segment_offset));
	__asm__ __volatile__("pushq %rax");
#else /* __X86__ */
    __asm__ __volatile__("mov %0, %%eax" : : "r"((uint32_t)segment_offset));
	__asm__ __volatile__("pushl %eax");
#endif
}

static inline const void
gdt_setup_entry_point(uintptr_t jump_addr) {
	// Push to stack entry point address
#if defined(__X86_64__)
	__asm__ __volatile__("mov %0, %%rax" : : "r"(jump_addr));
	__asm__ __volatile__("pushq %rax");
#else /* __X86__ */
	__asm__ __volatile__("mov %0, %%eax" : : "r"(jump_addr));
	__asm__ __volatile__("pushl %eax");
#endif
}

static inline const void
gdt_setup_data_segment(uint16_t segment_offset) {
    // Set offset of DATA segment according to the GDT data descriptor
    __asm__ __volatile__("mov %0, %%ax" : : "r"(segment_offset));
    __asm__ __volatile__("mov %ax, %ds");
    __asm__ __volatile__("mov %ax, %es");
    __asm__ __volatile__("mov %ax, %fs");
    __asm__ __volatile__("mov %ax, %gs");
    __asm__ __volatile__("mov %ax, %ss");
}
#endif /* __ASSEMBLER__ */
#endif /* __X86_GDT_H__ */
