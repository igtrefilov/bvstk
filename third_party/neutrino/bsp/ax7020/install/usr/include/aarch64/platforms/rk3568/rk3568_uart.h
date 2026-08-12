/*
 *
 * (c) 2023, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __RK3568_UART_H__
#define __RK3568_UART_H__

#define RK3568_PERIPH_CLOCK					240000000u

// UART registers, offset from base address
#define RK3568_UART_RBR						0x00	// Receive Buffer Register
#define RK3568_UART_DLL						0x00	// Divisor Latch Low Register
#define RK3568_UART_DLH						0x04	// Divisor Latch High Register
#define RK3568_UART_FCR						0x04	// FIFO Control Register
#define RK3568_UART_THR						0x00	// Transmit Buffer Register
#define RK3568_UART_LCR						0x0C	// Line Control Register
#define RK3568_UART_LSR						0x14	// Line Status Register
#define RK3568_UART_USR						0x7C	// UART Status Register
#define RK3568_UART_SRR						0x88	// Software Reset Register

// Line Controls Register bits
#define RK3568_UART_LCR_DLA_BIT				(1u << 7) // Divisor Latch Access Bit
#define RK3568_UART_LCR_BC_BIT				(1u << 6) // Break Control Bit
#define RK3568_UART_LCR_EPS_BIT				(1u << 4) // Even Parity Select Bit
#define RK3568_UART_LCR_PE_BIT				(1u << 3) // Parity Enable Bit
#define RK3568_UART_LCR_SBN_BIT				(1u << 2) // Stop bits Number Bit

// Line Status Register bits
#define RK3568_UART_LSR_RFE_BIT				(1u << 7) // Receiver FIFO Error Bit
#define RK3568_UART_LSR_TE_BIT				(1u << 6) // Transmitter Empty Bit
#define RK3568_UART_LSR_THRE_BIT			(1u << 5) // Transmit Holding Register Empty Bit
#define RK3568_UART_LSR_BI_BIT				(1u << 4) // Break Interrupt Bit
#define RK3568_UART_LSR_FE_BIT				(1u << 3) // Framing Error Bit
#define RK3568_UART_LSR_PE_BIT				(1u << 2) // Parity Error  Bit
#define RK3568_UART_LSR_OE_BIT				(1u << 1) // Overrun Error  Bit
#define RK3568_UART_LSR_RDR_BIT				(1u << 0) // Receive Data Ready Bit

// Software Reset Register bits
#define RK3568_UART_USR_RFF_BIT				(1u << 4) // Receive FIFO Full Bit
#define RK3568_UART_USR_RFNE_BIT			(1u << 3) // Receive FIFO Non Empty Bit
#define RK3568_UART_USR_TFE_BIT				(1u << 2) // Transmit FIFO Empty Bit
#define RK3568_UART_USR_TFNF_BIT			(1u << 1) // Transmit FIFO Not Full Bit
#define RK3568_UART_USR_UB_BIT				(1u << 0) // UART Busy Bit

// Software Reset Register bits
#define RK3568_UART_SRR_XFR_BIT				(1u << 2) // XMIT FIFO Reset Bit
#define RK3568_UART_SRR_RFR_BIT				(1u << 1) // RCVR FIFO Reset Bit
#define RK3568_UART_SRR_UR_BIT				(1u << 0) // UART Reset Bit

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <aarch64/inout.h>

/*
 * Accessor to write the THR
 */
static inline void
rk3568_uart_write_thr(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_THR, (uint32_t)value);
}

/*
 * Accessor to read/write the DLL
 */
static inline const uint8_t
rk3568_uart_read_dll(const uintptr_t base) {
	return (uint8_t)in32(base + RK3568_UART_DLL);
}

static inline void
rk3568_uart_write_dll(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_DLL, (uint32_t)value);
}

/*
 * Accessor to read/write the DLH
 */
static inline const uint8_t
rk3568_uart_read_dlh(const uintptr_t base) {
	return (uint8_t)in32(base + RK3568_UART_DLH);
}

static inline void
rk3568_uart_write_dlh(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_DLH, (uint32_t)value);
}

/*
 * Accessor to write the FCR
 */
static inline void
rk3568_uart_write_fcr(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_FCR, (uint32_t)value);
}

/*
 * Accessor to read/write the LCR
 */
static inline const uint8_t
rk3568_uart_read_lcr(const uintptr_t base) {
	return (uint8_t)(in32(base + RK3568_UART_LCR));
}

static inline void
rk3568_uart_write_lcr(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_LCR, (uint32_t)value);
}

/*
 * Accessor to read the LSR
 */
static inline const uint8_t
rk3568_uart_read_lsr(const uintptr_t base) {
	return (uint8_t)(in32(base + RK3568_UART_LSR));
}

/*
 * Accessor to read the USR
 */
static inline const uint8_t
rk3568_uart_read_usr(const uintptr_t base) {
	return (uint8_t)(in32(base + RK3568_UART_USR));
}

/*
 * Accessor to write the SRR
 */
static inline void
rk3568_uart_write_srr(const uintptr_t base, const uint8_t value) {
	out32(base + RK3568_UART_SRR, (uint32_t)value);
}

#endif /* __ASSEMBLER__ */
#endif /* __RK3568_UART_H__ */
