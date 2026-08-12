/*
 * (c) 2025-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * Elvees 1892VM14 processor with Dual ARMv7 Cortex-A9 core
 */

#ifndef	__ARM_MC1892VM14_H_INCLUDED
#define	__ARM_MC1892VM14_H_INCLUDED

#include "mc1892vm14_irq.h"


/* -------------------------------------------------------------------------
 * HWI Devices
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_HWI_UART				"uart"
#define MC1892VM14_HWI_WDT				"wdt"
#define MC1892VM14_HWI_DMA				"dma"
#define MC1892VM14_HWI_SPLL				"spll"

/* -------------------------------------------------------------------------
 * SDRAM
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_DDRMC0_BASE			0x40000000
#define MC1892VM14_DDRMC1_BASE			0xA0000000


/* -------------------------------------
 * SDMA
 * -------------------------------------
 */

#define MC1892VM14_SDMA_BASE_ADDR		0x37220000
#define MC1892VM14_SDMA_REG_SIZE		0x1000

/* -------------------------------------
 * NAND
 * -------------------------------------
 */
#define MC1892VM14_NAND_BASE_ADDR		0x38007000
#define MC1892VM14_NAND_REG_SIZE		0x1000

/* -------------------------------------------------------------------------
 * ARM / SMP
 * -------------------------------------------------------------------------
 */

#define MC1892VM14_SPRAM_START			0x20000000
#define MC1892VM14_SPRAM_SIZE			0x10000
#define MC1892VM14_PL310_BASE			0x39004000



/* -------------------------------------------------------------------------
 * CPU Private timers
 * -------------------------------------------------------------------------
 */

#define MC1892VM14_PRIVATE_TIMER_BASE				0x39000600
#define MC1892VM14_PRIVATE_TIMER_SIZE				0x40

/* Timer Load register */
#define MC1892VM14_PRIVATE_TIMER_LOAD				0x00

/* Timer Counter register */
#define MC1892VM14_PRIVATE_TIMER_COUNTER			0x04

/* Timer Control register */
#define MC1892VM14_PRIVATE_TIMER_CTRL				0x08
#define MC1892VM14_PRIVATE_TIMER_CTRL_EN			(1 << 0)
#define MC1892VM14_PRIVATE_TIMER_CTRL_AUTO_RELOAD	(1 << 1)
#define MC1892VM14_PRIVATE_TIMER_CTRL_IRQ			(1 << 2)
#define MC1892VM14_PRIVATE_TIMER_CTRL_PRESCALER_MSK	(0xFF << 8)

/* Timer Interrupt Status register */
#define MC1892VM14_PRIVATE_TIMER_INT_STS 			0x0C

#define MC1892VM14_PRIVATE_WDT_LOAD					0x20
#define MC1892VM14_PRIVATE_WDT_COUNTER				0x24
#define MC1892VM14_PRIVATE_WDT_CTRL					0x28
#define MC1892VM14_PRIVATE_WDT_STS					0x2C
#define MC1892VM14_PRIVATE_WDT_RESET				0x30
#define MC1892VM14_PRIVATE_WDT_DISABLE				0x34

/* -------------------------------------
 * Global timer
 * -------------------------------------
 */
#define MC1892VM14_GLOBAL_TIMER_COUNTER_BASE			0x39000200
/* Size of the register area (used for mapping) */
#define MC1892VM14_GLOBAL_TIMER_COUNTER_BASE_MAP_SIZE	0x20

#define MC1892VM14_GLOBAL_TIMER_COUNTER_REG0			0x00
#define MC1892VM14_GLOBAL_TIMER_COUNTER_REG1			0x04
#define MC1892VM14_GLOBAL_TIMER_CONTROL_REG				0x08
#define MC1892VM14_GLOBAL_TIMER_INT_STS					0x08
#define MC1892VM14_GLOBAL_TIMER_CMP_REG0				0x10
#define MC1892VM14_GLOBAL_TIMER_CMP_REG1				0x14

#define MC1892VM14_GLOBAL_TIMER_CONTROL_TIMER_EN		(1 << 0)
#define MC1892VM14_GLOBAL_TIMER_CONTROL_CMP_EN			(1 << 1)
#define MC1892VM14_GLOBAL_TIMER_CONTROL_IRQ				(1 << 2)
#define MC1892VM14_GLOBAL_TIMER_CONTROL_AE				(1 << 3)
#define MC1892VM14_GLOBAL_TIMER_CONTROL_PRESCALER_MSK	(0xFF << 8)

/*
 * -------------------------------------------------------------------------
 * CMCTR - synchronization controller
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_CMCTR_BASE			0x38094000
#define MC1892VM14_CMCTR_SIZE			0x200
#define MC1892VM14_CMCTR_DIV_MPU_REG	0x4
#define MC1892VM14_CMCTR_DIV_SYS0_REG	0x40
#define MC1892VM14_CMCTR_DIV_SYS1_REG	0x44
#define MC1892VM14_CMCTR_GATE_CORE_REG	0x48
#define MC1892VM14_CMCTR_GATE_SYS_REG	0x4C
#define MC1892VM14_CMCTR_SEL_APLL_REG	0x100
#define MC1892VM14_CMCTR_SEL_SPLL_REG	0x10C

#define MC1892VM14_CMCTR_GATE_CORE_DDR0_EN				(1 << 1)
#define MC1892VM14_CMCTR_GATE_CORE_DDR1_EN				(1 << 2)
#define MC1892VM14_CMCTR_GATE_CORE_VPOUT_EN				(1 << 3)
#define MC1892VM14_CMCTR_GATE_CORE_VPIN_EN				(1 << 4)
#define MC1892VM14_CMCTR_GATE_CORE_VPU_EN				(1 << 5)
#define MC1892VM14_CMCTR_GATE_CORE_GPU_EN				(1 << 6)



#define MC1892VM14_CMCTR_GATE_SYS_SDMMC0_EN				(1 << 2)
#define MC1892VM14_CMCTR_GATE_SYS_SDMMC1_EN				(1 << 3)
#define MC1892VM14_CMCTR_GATE_SYS_EMAC_EN				(1 << 4)
#define MC1892VM14_CMCTR_GATE_SYS_USBIC_EN				(1 << 5)
#define MC1892VM14_CMCTR_GATE_SYS_UART2_EN				(1 << 14)
#define MC1892VM14_CMCTR_GATE_SYS_UART3_EN				(1 << 15)
#define MC1892VM14_CMCTR_GATE_SYS_I2C0_EN				(1 << 16)
#define MC1892VM14_CMCTR_GATE_SYS_I2C1_EN				(1 << 17)
#define MC1892VM14_CMCTR_GATE_SYS_I2C2_EN				(1 << 18)
#define MC1892VM14_CMCTR_GATE_SYS_SPI0_EN				(1 << 19)
#define MC1892VM14_CMCTR_GATE_SYS_SPI1_EN				(1 << 20)
#define MC1892VM14_CMCTR_GATE_SYS_NAND_EN				(1 << 21)

/*
 * -------------------------------------------------------------------------
 * PMCTR - power management controller
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_PMCTR_BASE			0x38095000
#define MC1892VM14_PMCTR_SIZE			0x1000
#define MC1892VM14_PMCTR_SYS_PWR_UP		0x00
#define MC1892VM14_PMCTR_SW_RST_REG		0x40

/*
 * -------------------------------------------------------------------------
 * SMCTR - common management controller
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_SMCTR_BASE			0x38096000
#define MC1892VM14_SMCTR_SIZE			0x1000
#define SMCTR_BOOT_REMAP				0x04

#define SMCTR_BOOT_REMAP_NORMPORT		0x0
#define SMCTR_BOOT_REMAP_SPRAM			0x1
#define SMCTR_BOOT_REMAP_SPL			0x2
#define SMCTR_BOOT_REMAP_BOOTROM		0x3


/* -------------------------------------
 * GPIO
 * -------------------------------------
 */

#define MC1892VM14_GPIO_BASE			0x38034000
#define MC1892VM14_GPIO_SIZE			0x1000

#define MC1892VM14_GPIOA_DR				0x00
#define MC1892VM14_GPIOA_DDR			0x04
#define MC1892VM14_GPIOA_CTL			0x08
#define MC1892VM14_GPIOB_DR				0x0C
#define MC1892VM14_GPIOB_DDR			0x10
#define MC1892VM14_GPIOB_CTL			0x14
#define MC1892VM14_GPIOC_DR				0x18
#define MC1892VM14_GPIOC_DDR			0x1C
#define MC1892VM14_GPIOC_CTL			0x20
#define MC1892VM14_GPIOD_DR				0x24
#define MC1892VM14_GPIOD_DDR			0x28
#define MC1892VM14_GPIOD_CTL			0x2C
#define MC1892VM14_GPIO_IRQ_ENABLE		0x30
#define MC1892VM14_GPIO_IRQ_MASK		0x34
#define MC1892VM14_GPIO_IRQ_LEVEL		0x38
#define MC1892VM14_GPIO_IRQ_POLARITY	0x3C
#define MC1892VM14_GPIO_IRQ_STATUS		0x40
#define MC1892VM14_GPIO_IRQ_RAWSTATUS	0x44
#define MC1892VM14_GPIO_IRQ_EOI			0x4C

/* -------------------------------------------------------------------------
 * Serial ports
 * -------------------------------------------------------------------------
 */

/* UART base addresses */
#define MC1892VM14_UART_CLK_FREQ	144000000U

#define	MC1892VM14_UART0_BASE		0x38028000
#define	MC1892VM14_UART1_BASE		0x38029000

#define	MC1892VM14_UART_IO_SIZE		0x1000

/* UART registers */
#define MC1892VM14_UART_THR			0x00  /* Transmitter */
#define MC1892VM14_UART_RBR			0x00  /* Receiver */
#define MC1892VM14_UART_DLL			0x00  /* DLL */
#define MC1892VM14_UART_DHL			0x04  /* DHL */
#define MC1892VM14_UART_LCR			0x0C  /* UART Control register */
#define MC1892VM14_UART_LSR			0x14  /* Channel status register */
#define MC1892VM14_UART_USR			0x7C  /* UART status register */

/* UART Status Regiser */
#define MC1892VM14_UART_LSR_RDR		(1 << 0)
#define MC1892VM14_UART_LSR_BI		(1 << 4)
#define MC1892VM14_UART_LSR_THRE	(1 << 5)
#define MC1892VM14_UART_LSR_TEMPT	(1 << 6)

/* UART Control register */
#define MC1892VM14_UART_LCR_DLS_5	0x00		/* 5-bit characters */
#define MC1892VM14_UART_LCR_DLS_6	0x01		/* 6-bit characters */
#define MC1892VM14_UART_LCR_DLS_7	0x02		/* 7-bit characters */
#define MC1892VM14_UART_LCR_DLS_8	0x03		/* 8-bit characters */
#define MC1892VM14_UART_LCR_STB		(1 << 2)	/* 2 stop bits */
#define MC1892VM14_UART_LCR_PEN		(1 << 3)	/* Parity enable */
#define MC1892VM14_UART_LCR_EPS		(1 << 4)	/* Even parity */
#define MC1892VM14_UART_LCR_STP		(1 << 5)	/* Sticky parity */
#define MC1892VM14_UART_LCR_BC		(1 << 6)	/* Set break */
#define MC1892VM14_UART_LCR_DLAB	(1 << 7)	/* Divisor latch access */


/*
 * -------------------------------------------------------------------------
 * TIMER - synchronization controller
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_TIMERS_BASE		0x38026000
#define MC1892VM14_TIMERS_SIZE		0x1000

// LOAD COUNT REGISTER
#define MC1892VM14_TIMER0_LC		0x00
#define MC1892VM14_TIMER1_LC		0x14
#define MC1892VM14_TIMER2_LC		0x28
#define MC1892VM14_TIMER3_LC		0x3C
#define MC1892VM14_TIMER4_LC		0x50
#define MC1892VM14_TIMER5_LC		0x64
#define MC1892VM14_TIMER6_LC		0x78
#define MC1892VM14_TIMER7_LC		0x8C

// LOAD COUNT 2 REGISTER
#define MC1892VM14_TIMER0_LC2		0xB0
#define MC1892VM14_TIMER1_LC2		0xB4
#define MC1892VM14_TIMERN_LC2		0

// CURRENT VALUE REGISTER
#define MC1892VM14_TIMER0_CV		0x04
#define MC1892VM14_TIMER1_CV		0x18
#define MC1892VM14_TIMER2_CV		0x2C
#define MC1892VM14_TIMER3_CV		0x40
#define MC1892VM14_TIMER4_CV		0x54
#define MC1892VM14_TIMER5_CV		0x68
#define MC1892VM14_TIMER6_CV		0x7C
#define MC1892VM14_TIMER7_CV		0x90

// CONTROL REGISTER
#define MC1892VM14_TIMER0_CNTR	0x08
#define MC1892VM14_TIMER1_CNTR	0x1C
#define MC1892VM14_TIMER2_CNTR	0x30
#define MC1892VM14_TIMER3_CNTR	0x44
#define MC1892VM14_TIMER4_CNTR	0x58
#define MC1892VM14_TIMER5_CNTR	0x6C
#define MC1892VM14_TIMER6_CNTR	0x80
#define MC1892VM14_TIMER7_CNTR	0x94

// END OF INTERRUPT REGISTER
#define MC1892VM14_TIMER0_EOI		0x0C
#define MC1892VM14_TIMER1_EOI		0x20
#define MC1892VM14_TIMER2_EOI		0x34
#define MC1892VM14_TIMER3_EOI		0x48
#define MC1892VM14_TIMER4_EOI		0x5C
#define MC1892VM14_TIMER5_EOI		0x70
#define MC1892VM14_TIMER6_EOI		0x84
#define MC1892VM14_TIMER7_EOI		0x98

// INTERRUPT STATUS REGISTER
#define MC1892VM14_TIMER0_IS		0x10
#define MC1892VM14_TIMER1_IS		0x24
#define MC1892VM14_TIMER2_IS		0x38
#define MC1892VM14_TIMER3_IS		0x4C
#define MC1892VM14_TIMER4_IS		0x60
#define MC1892VM14_TIMER5_IS		0x74
#define MC1892VM14_TIMER6_IS		0x88
#define MC1892VM14_TIMER7_IS		0x9C

#define MC1892VM14_TIMERS_IS		0xA0	// Common interrupt status register
#define MC1892VM14_TIMERS_EOI		0xA4	// Common end of interrupt register
#define MC1892VM14_TIMERS_IRS		0xA8	// Common raw interrupt status register

#define MC1892VM14_TIMERS_ENABLE	0x01	// Timer enable
#define MC1892VM14_TIMERS_MODE		0x02	// Timer mode
#define MC1892VM14_TIMERS_IRQ_MASK	0x04	// Timer interrupnt mask
#define MC1892VM14_TIMERS_TPWM		0x08	// Timer pulse-width modulation mode


/* -------------------------------------------------------------------------
 * Watchdog
 * -------------------------------------------------------------------------
 */
#define MC1892VM14_WDT_CLOCK_FREQ		144000000U
#define MC1892VM14_WDT_BASEADDR			0x38031000
#define MC1892VM14_WDT_SIZE				0x20

#define MC1892VM14_WDT_CR				0x00		/* Control Register */
#define MC1892VM14_WDT_TORR				0x04		/* Init Timeout Register */
#define MC1892VM14_WDT_CCVR				0x08		/* Current Counter Value Register */
#define MC1892VM14_WDT_CRR 				0x0C		/* Counter Restart Register */
#define MC1892VM14_WDT_STAT				0x10		/* Interrupt Status Register */
#define MC1892VM14_WDT_EOI				0x14		/* Interrupt Reset Register */

#define MC1892VM14_WDT_CR_WDEN			0x1			/* enable WDT */
#define MC1892VM14_WDT_CR_RMOD_IRQ		0x2			/* generate IRQ not reset */
#define MC1892VM14_WDT_CR_RPL_2			(0 << 2) 	/* 2 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_4			(1 << 2) 	/* 4 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_8			(2 << 2) 	/* 8 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_16		(3 << 2) 	/* 16 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_32		(4 << 2) 	/* 32 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_64		(5 << 2) 	/* 64 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_128		(6 << 2) 	/* 128 SPLLCLK reset pulse */
#define MC1892VM14_WDT_CR_RPL_256		(7 << 2) 	/* 256 SPLLCLK reset pulse */

#define MC1892VM14_WDT_TORR_TOP_MASK	0xF			/* TORR TOP field mask */
#define MC1892VM14_WDT_TOPINT_SHIFT		4			/* TORR TOP_INIT field shift */
#define WDT_MAX_TOP						0xF			/* Max value in TORR TOP_INIT */

#define MC1892VM14_WDT_RESTART_KEY_VAL	0x76		/* Counter Restart Valid Key */

#endif // __ARM_MC1892VM14_H_INCLUDED
