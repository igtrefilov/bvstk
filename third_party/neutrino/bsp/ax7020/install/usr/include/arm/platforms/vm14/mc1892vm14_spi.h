/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __1892VM14_SPI_H__
#define __1892VM14_SPI_H__

#define VM14_SPI_SIZE				0x1000

#define VM14_SPI0_BASE				0x38032000
#define VM14_SPI1_BASE				0x38033000

#define VM14_SPI0_IRQ				106
#define VM14_SPI1_IRQ				107

/* SPI register offsets */
#define VM14_SPI_CTRLR0				0x00			/* Control Register 0 */
#define VM14_SPI_CTRLR1				0x04			/* Control Register 1 */
#define VM14_SPI_SSIENR				0x08			/* SSI Enable Register */
#define VM14_SPI_MWCR				0x0c			/* Microwire Control Register. */
#define VM14_SPI_SER				0x10			/* Slave Enable Register. */
#define VM14_SPI_BAUDR				0x14			/* Baud Rate Select. */
#define VM14_SPI_TXFTLR				0x18			/* Transmit FIFO Threshold Level. */
#define VM14_SPI_RXFTLR				0x1c			/* Receive FIFO Threshold level. */
#define VM14_SPI_TXFLR				0x20			/* Transmit FIFO Level Register */
#define VM14_SPI_RXFLR				0x24			/* Receive FIFO Level Register */
#define VM14_SPI_IMR				0x2c			/* Interrupt Mask Register */
#define VM14_SPI_ISR 				0x30			/* Interrupt Status Register */
#define VM14_SPI_RISR				0x34			/* Raw Interrupt Status Register */
#define VM14_SPI_TXOICR				0x38			/* Transmit FIFO Overflow Interrupt Clear Register */
#define VM14_SPI_RXOICR				0x3c			/* Receive FIFO Overflow Interrupt Clear Register */
#define VM14_SPI_RXUICR				0x40			/* Receive FIFO Underflow Interrupt Clear Register */
#define VM14_SPI_ICR				0x48			/* Interrupt Clear Register */
#define VM14_SPI_DMACR				0x4c			/* DMA Control Register. */
#define VM14_SPI_DMATDLR			0x50			/* DMA Transmit Data Level. */
#define VM14_SPI_DMARDLR			0x54			/* DMA Receive Data Level. */
#define VM14_SPI_DR0				0x60			/* A 16-bit read/write buffer for the transmit/receive FIFOs. */
#define VM14_SPI_DR35				0xec			/* A 16-bit read/write buffer for the transmit/receive FIFOs. */
#define VM14_SPI_RX_SAMPLE_DLY		0xf0			/* RX Sample Delay. */
#define VM14_SPI_TOGGLE				0xf4            /* Control register signal CS */

/* Bit fields in ISR, RISR, IMR */
#define VM14_SPI_INT_TXEI			0x01
#define VM14_SPI_INT_TXOI			0x02
#define VM14_SPI_INT_RXUI			0x04
#define VM14_SPI_INT_RXOI			0x08
#define VM14_SPI_INT_RXFI			0x10

/* Serial Clock Phase. */
#define VM14_SPI_SCPH_MIDDLE		0
#define VM14_SPI_SCPH_START			1

/* Serial Clock Polarity. */
#define VM14_SPI_SCPOL_LOW			0
#define VM14_SPI_SCPOL_HIGH			1

/* Enable control */
#define VM14_SPI_DISABLE			0
#define VM14_SPI_ENABLE				1

/* Transfer mode bits */
#define VM14_SPI_TMOD_TR			0x00			/* transmit and receive */
#define VM14_SPI_TMOD_TO			0x01			/* transmit only */
#define VM14_SPI_TMOD_RO			0x02			/* receive only */
#define VM14_SPI_TMOD_EPROMREAD		0x03			/* EEPROM read */

/* Structures of registers */

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	dfs               : 4,
					frf               : 2,
					scph              : 1,
					scpol             : 1,
					tomd              : 2,
					reserved_1        : 1,
					srl               : 1,
					cfs               : 4,
					reserved_2        : 16;
    } __attribute__((packed)) bits;
} vm14_spi_ctrlr0_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	ndf               : 16,
					reserved          : 16;
    } __attribute__((packed)) bits;
} vm14_spi_ctrlr1_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	ssi_en            : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_ssienr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	mwmod             : 1,
			mdd               : 1,
			mhs               : 1,
					reserved          : 29;
    } __attribute__((packed)) bits;
} vm14_spi_mwcr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	ser               : 4,
					reserved          : 28;
    } __attribute__((packed)) bits;
} vm14_spi_ser_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	sckdv             : 16,
					reserved          : 16;
    } __attribute__((packed)) bits;
} vm14_spi_baudr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	tft               : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_txftlr_t;

typedef union{
	uint32_t        raw;
	struct {
		uint32_t	rft               : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_rxftlr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	txtfl             : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_txflr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	rxtfl             : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_rxflr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	txeim             : 1,
					txoim             : 1,
					rxuim             : 1,
					rxoim             : 1,
					rxfim             : 1,
					reserved          : 27;
    } __attribute__((packed)) bits;
} vm14_spi_imr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	txeis             : 1,
					txois             : 1,
					rxuis             : 1,
					rxois             : 1,
					rxfis             : 1,
					reserved          : 27;
    } __attribute__((packed)) bits;
} vm14_spi_isr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	txeir             : 1,
					txoir             : 1,
					rxuir             : 1,
					rxoir             : 1,
					rxfir             : 1,
					reserved          : 27;
    } __attribute__((packed)) bits;
} vm14_spi_risr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	txoicr            : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_txoicr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	rxoicr            : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_rxoicr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	rxuicr            : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_rxuicr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	icr               : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_icr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	rdmae             : 1,
					tdmae             : 1,
					reserved          : 30;
    } __attribute__((packed)) bits;
} vm14_spi_dmacr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	dmatdl            : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_dmatdlr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	dmardl            : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_dmardlr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	dr                : 16,
					reserved          : 16;
    } __attribute__((packed)) bits;
} vm14_spi_dr_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	rsd               : 8,
					reserved          : 24;
    } __attribute__((packed)) bits;
} vm14_spi_rx_sample_delay_t;

typedef union {
	uint32_t        raw;
	struct {
		uint32_t	toggle            : 1,
					reserved          : 31;
    } __attribute__((packed)) bits;
} vm14_spi_toggle_t;

/* Write register inline functions */
static inline
void vm14_spi_ctrlr0_set_dfs(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr0_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR0);
	reg.bits.dfs = value;
	out32(addr + VM14_SPI_CTRLR0, reg.raw);
}

static inline
void vm14_spi_ctrlr0_set_scpol(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr0_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR0);
	reg.bits.scpol = value;
	out32(addr + VM14_SPI_CTRLR0, reg.raw);
}

static inline
void vm14_spi_ctrlr0_set_scph(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr0_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR0);
	reg.bits.scph = value;
	out32(addr + VM14_SPI_CTRLR0, reg.raw);
}

static inline
void vm14_spi_ctrlr0_set_tomd(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr0_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR0);
	reg.bits.tomd = value;
	out32(addr + VM14_SPI_CTRLR0, reg.raw);
}

static inline
void vm14_spi_ctrlr0_set_srl(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr0_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR0);
	reg.bits.srl = value;
	out32(addr + VM14_SPI_CTRLR0, reg.raw);
}

static inline
void vm14_spi_ctrlr1_set(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ctrlr1_t reg;

    reg.raw = in32(addr + VM14_SPI_CTRLR1);
	reg.bits.ndf = value;
	out32(addr + VM14_SPI_CTRLR1, reg.raw);
}

static inline
void vm14_spi_ssi_set_en(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_ssienr_t reg;

    reg.raw = in32(addr + VM14_SPI_SSIENR);
	reg.bits.ssi_en = value;
	out32(addr + VM14_SPI_SSIENR, reg.raw);
}

static inline
void vm14_spi_ser_set(const uintptr_t addr, const uint32_t value, const uint8_t enable)
{
	vm14_spi_ser_t reg;

    reg.raw = in32(addr + VM14_SPI_SER);
	if (enable == 1)
	{
		reg.bits.ser |= value;
	}
	else
	{
		reg.bits.ser &= ~value;
	}
	out32(addr + VM14_SPI_SER, reg.raw);
}

static inline
void vm14_spi_baudr_set(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_baudr_t reg;

    reg.raw = in32(addr + VM14_SPI_BAUDR);
	reg.bits.sckdv = value;
	out32(addr + VM14_SPI_BAUDR, reg.raw);
}

static inline
void vm14_spi_rxftlr_set(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_rxftlr_t reg;

    reg.raw = in32(addr + VM14_SPI_RXFTLR);
	reg.bits.rft = value;
	out32(addr + VM14_SPI_RXFTLR, reg.raw);
}

static inline
void vm14_spi_imr_set(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_imr_t reg;

    reg.raw = value;
	out32(addr + VM14_SPI_IMR, reg.raw);
}

static inline
void vm14_spi_imr_set_txeim(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_imr_t reg;

    reg.raw = in32(addr + VM14_SPI_IMR);
	reg.bits.txeim = value;
	out32(addr + VM14_SPI_IMR, reg.raw);
}

static inline
void vm14_spi_imr_set_rxfim(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_imr_t reg;

    reg.raw = in32(addr + VM14_SPI_IMR);
	reg.bits.rxfim = value;
	out32(addr + VM14_SPI_IMR, reg.raw);
}

static inline
void vm14_spi_dr_set(const uintptr_t addr, const uint32_t value)
{
	vm14_spi_dr_t reg;

    reg.raw = in32(addr + VM14_SPI_DR0);
	reg.bits.dr = value;
	out32(addr + VM14_SPI_DR0, reg.raw);
}

static inline
void vm14_spi_toggle_set(uintptr_t addr, const uint32_t value)
{
	vm14_spi_toggle_t reg;

    reg.raw = in32(addr + VM14_SPI_TOGGLE);
	reg.bits.toggle = value;
	out32(addr + VM14_SPI_TOGGLE, reg.raw);
}

/* Read register inline functions */
static inline
uint32_t vm14_spi_rxflr_get(const uintptr_t addr)
{
	vm14_spi_rxflr_t reg;

    reg.raw = in32(addr + VM14_SPI_RXFLR);
	return reg.raw;
}

static inline
uint32_t vm14_spi_isr_get(const uintptr_t addr)
{
	vm14_spi_isr_t reg;

    reg.raw = in32(addr + VM14_SPI_ISR);
	return reg.raw;
}

static inline
void vm14_spi_icr_clean(const uintptr_t addr)
{
    in32(addr + VM14_SPI_ICR);
}

static inline
uint32_t vm14_spi_dr_get(const uintptr_t addr)
{
	vm14_spi_dr_t reg;

    reg.raw = in32(addr + VM14_SPI_DR0);
	return reg.raw;
}

#endif /* __1892VM14_SPI_H__ */