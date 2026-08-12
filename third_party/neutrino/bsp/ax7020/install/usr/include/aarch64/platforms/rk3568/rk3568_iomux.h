/*
 *
 * (c) 2025-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __RK3568_IOMUX_H__
#define __RK3568_IOMUX_H__

#include <sys/platform.h>
#include <rk3568/rk3568_common.h>

#define RK3568_DECLARE_IOMUX_REGISTER(register_name, field0, field1, field2, field3)	\
typedef union {																			\
	uint32_t raw;																		\
	struct {																			\
		struct {																		\
			uint16_t																	\
				field0				: 3,												\
				reserv0				: 1,												\
				field1				: 3,												\
				reserv1				: 1,												\
				field2				: 3,												\
				reserv2				: 1,												\
				field3				: 3,												\
				reserv3				: 1;												\
		} __attribute__((__packed__)) bits, write_en;									\
	};																					\
} rk3568_##register_name##_t;

#define RK3568_SET_IOMUX_FIELD(addr, offset, register_name, field, val)	\
do {																	\
	rk3568_##register_name##_t reg = {0};								\
																		\
	reg.bits.field = RK3568_##val;										\
	reg.write_en.field = ~0;											\
																		\
	out32(RK3568_##addr + RK3568_##offset, reg.raw);					\
} while (0)

#define RK3568_SET_IOMUX_SELECTOR RK3568_SET_IOMUX_FIELD
#define RK3568_SET_IOMUX_SOLUTION RK3568_SET_IOMUX_FIELD

/* PMU_GRF */
#define RK3568_PMU_GRF_GPIO0B_IOMUX_L			0x00000008
RK3568_DECLARE_IOMUX_REGISTER(gpio0b_iomux_l, gpio0b0_sel, gpio0b1_sel, gpio0b2_sel, gpio0b3_sel)

#define RK3568_GPIO0_B3							0
#define RK3568_I2C1_SCL							1
#define RK3568_CAN0_TXM0						2
#define RK3568_PCIE30X1_BUTTONRSTN				3
#define RK3568_MCU_JTAGTDO						4

#define RK3568_GPIO0_B2							0
#define RK3568_I2C0_SDA							1

#define RK3568_GPIO0_B1							0
#define RK3568_I2C0_SCL							1

#define RK3568_GPIO0_B0							0
#define RK3568_CLK32K_IN						1
#define RK3568_CLK32K_OUT0						2
#define RK3568_PCI30X2_BUTTONRSTN				3

#define RK3568_PMU_GRF_GPIO0B_IOMUX_H			0x0000000C
RK3568_DECLARE_IOMUX_REGISTER(gpio0b_iomux_h, gpio0b4_sel, gpio0b5_sel, gpio0b6_sel, gpio0b7_sel)
#define RK3568_GPIO0_B7							0
#define RK3568_PWM0_M0 							1
#define RK3568_CPU_AVS							2

#define RK3568_GPIO0_B6							0
#define RK3568_I2C2_SDAM0						1
#define RK3568_SPI0_MOSIM0						2
#define RK3568_PCIE20_PERSTNM0					3
#define RK3568_PWM2_M1							4

#define RK3568_GPIO0_0B5						0
#define RK3568_I2C2_SCLM0						1
#define RK3568_SPI0_CLKM0						2
#define RK3568_PCIE20_WAKENM0					3
#define RK3568_PWM1_M1							4

#define RK3568_GPIO0_B4							0
#define RK3568_I2C1_SDA							1
#define RK3568_CAN0_RXM0						2
#define RK3568_PCIE20_BUTTONRSTN				3
#define RK3568_MCU_JTAGTCK						4

#define RK3568_PMU_GRF_GPIO0C_IOMUX_L			0x00000010
RK3568_DECLARE_IOMUX_REGISTER(gpio0c_iomux_l, gpio0c0_sel, gpio0c1_sel, gpio0c2_sel, gpio0c3_sel)

#define RK3568_GPIO0_C3							0
#define RK3568_PWM4								1
#define RK3568_VOP_PWMM0						2
#define RK3568_PCIE30X1_PERSTNM0				3
#define RK3568_MCU_JTAGTRSTN					4

#define RK3568_GPIO0_C2							0
#define RK3568_PWM3								1
#define RK3568_EDPDP_HPDINM1					2
#define RK3568_PCIE30X1_WAKENM0					3
#define RK3568_MCU_JTAGTMS						4

#define RK3568_GPIO0_C1							0
#define RK3568_PWM2_M0							1
#define RK3568_NPU_AVS							2
#define RK3568_UART0_TX							3
#define RK3568_MCU_JTAGTDI						4

#define RK3568_GPIO0_C0							0
#define RK3568_PWM1_M0							1
#define RK3568_GPU_AVS							2
#define RK3568_UART0_RX							3

#define RK3568_PMU_GRF_GPIO0C_IOMUX_H			0x00000014
RK3568_DECLARE_IOMUX_REGISTER(gpio0c_iomux_h, gpio0c4_sel, gpio0c5_sel, gpio0c6_sel, gpio0c7_sel)

#define RK3568_GPIO0_C7							0
#define RK3568_HDMITX_CECM1						1
#define RK3568_PWM0_M1							2
#define RK3568_UART0_CTSN						3
#define RK3568_PMU_DEBUG5						4

#define RK3568_GPIO0_C6							0
#define RK3568_PWM7								1
#define RK3568_SPI0_CS0M0						2
#define RK3568_PCIE30X2_PERSTNM0				3
#define RK3568_PMU_DEBUG4						4

#define RK3568_GPIO0_C5							0
#define RK3568_PWM6								1
#define RK3568_SPI0_MISOM0						2
#define RK3568_PCIE30X2_WAKENM0					3
#define RK3568_PMU_DEBUG3						4

#define RK3568_GPIO0_C4							0
#define RK3568_PWM5								1
#define RK3568_SPI0_CS1M0						2
#define RK3568_UART0_RTSN						3
#define RK3568_PMU_DEBUG2						4

#define RK3568_PMU_GRF_GPIO0D_IOMUX_L			0x00000018
typedef union {
	uint32_t    raw;
	struct {
		struct {
			uint16_t
				gpio0d0_sel         : 3,
				reserved0           : 1,
				gpio0d1_sel         : 3,
				reserved1           : 5,
				gpio0d3_sel     	: 3,
				reserved2           : 1;
		} __attribute__((__packed__)) bits, write_en;
	};
} rk3568_gpio0d_iomux_l_t;

#define RK3568_GPIO0_D3							0

#define RK3568_GPIO0_D1							0
#define RK3568_UART2_TXM0						1

#define RK3568_GPIO0_DO							0
#define RK3568_UART2_RXM0						1

/* SYS_GRF */
#define RK3568_GRF_GPIO1A_IOMUX_L				0x00000000
RK3568_DECLARE_IOMUX_REGISTER(gpio1a_iomux_l, gpio1a0_sel, gpio1a1_sel, gpio1a2_sel, gpio1a3_sel)

#define RK3568_GPIO1_A3							0
#define RK3568_I2S1_SCLKTXM0					1
#define RK3568_UART3_CTSNM0						2
#define RK3568_SCR_IO							3
#define RK3568_PCIE30X1_WAKENM2					4
#define RK3568_ACODEC_DACCLK					5

#define RK3568_GPIO1_A2							0
#define RK3568_I2S1_MCLKMM0						1
#define RK3568_UART3_RTSNM0						2
#define RK3568_SCR_CLK							3
#define RK3568_PCIE30X1_PERSTNM2				4

#define RK3568_GPIO1_A1							0
#define RK3568_I2C3_SCLM0						1
#define RK3568_UART3_TXM0						2
#define RK3568_CAN1_TXM0						3
#define RK3568_AUDIOpwmROUT						4
#define RK3568_ACODEC_ADCCLK					5
#define RK3568_AUDIOpwmLOUTN					6

#define RK3568_GPIO1_A0							0
#define RK3568_I2C3_SDAM0						1
#define RK3568_UART3_RXM0						2
#define RK3568_CAN1_RXM0						3
#define RK3568_AUDIOpwmLOUT						4
#define RK3568_ACODEC_ADCDATA					5
#define RK3568_AUDIOpwmLOUTP					6

#define RK3568_GRF_GPIO1A_IOMUX_H				0x00000004
RK3568_DECLARE_IOMUX_REGISTER(gpio1a_iomux_h, gpio1a4_sel, gpio1a5_sel, gpio1a6_sel, gpio1a7_sel)

#define RK3568_GPIO1_A7							0
#define RK3568_I2S1_SDO0M0						1
#define RK3568_UART4_CTSNM0						2
#define RK3568_SCR_DET							3
#define RK3568_AUDIOpwmROUTN					4
#define RK3568_ACODEC_DAC_DATAL					5

#define RK3568_GPIO1_A6							0
#define RK3568_I2S1_LRCKRXM0					1
#define RK3568_UART4_TXM0						2
#define RK3568_PDM_CLK0M0						3
#define RK3568_AUDIOpwmROUTP					4

#define RK3568_GPIO1_A5							0
#define RK3568_I2S1_LRCKTXM0					1
#define RK3568_UART4_RTSNM0						2
#define RK3568_SCR_RST 							3
#define RK3568_PCIE30X1_CLKREQNM2				4
#define RK3568_ACODEC_DACSYNC					5

#define RK3568_GPIO1_A4							0
#define RK3568_I2S1_SCLKRXM0					1
#define RK3568_UART4_RXM0						2
#define RK3568_PDM_CLK1M0						3
#define RK3568_SPDIF_TXM0						4

#define RK3568_GRF_GPIO1D_IOMUX_H				0x0000001C
RK3568_DECLARE_IOMUX_REGISTER(gpio1d_iomux_h, gpio1d4_sel, gpio1d5_sel, gpio1d6_sel, gpio1d7_sel)

#define RK3568_GPIO1_D7							0
#define RK3568_SDMMC0_D2						1
#define RK3568_JTAG_TCK							2
#define RK3568_UART5_CTSNM0						3

#define RK3568_GPIO1_D6							0
#define RK3568_SDMMC0_D1						1
#define RK3568_UART2_RXM1						2
#define RK3568_UART6_RXM1						3
#define RK3568_PWM9_M1							4

#define RK3568_GPIO1_D5							0
#define RK3568_SDMMC0_D0						1
#define RK3568_UART2_TXM1						2
#define RK3568_UART6_TXM1						3
#define RK3568_PWM8_M1							4

#define RK3568_GPIO1_D4							0
#define RK3568_FSPI_D3							1
#define RK3568_FLASH_CS1N						2

#define RK3568_GRF_GPIO2A_IOMUX_L				0x00000020
RK3568_DECLARE_IOMUX_REGISTER(gpio2a_iomux_l, gpio2a0_sel, gpio2a1_sel, gpio2a2_sel, gpio2a3_sel)

#define RK3568_GPIO2_A3							0
#define RK3568_SDMMC1_D0						1
#define RK3568_GMAC0_RXD2						2
#define RK3568_UART6_RXM0						3

#define RK3568_GPIO2_A2							0
#define RK3568_SDMMC0_CLK						1
#define RK3568_TEST_CLKOUT						2
#define RK3568_UART5_TXM0						3
#define RK3568_CAN0_RXM1						4

#define RK3568_GPIO2_A1							0
#define RK3568_SDMMC0_CMD						1
#define RK3568_PWM10_M1							2
#define RK3568_UART5_RXM0						3
#define RK3568_CAN0_TXM1						4

#define RK3568_GPIO2_A0							0
#define RK3568_SDMMC0_D3						1
#define RK3568_JTAG_TMS							2
#define RK3568_UART5_RTSNM0						3

#define RK3568_GRF_GPIO2A_IOMUX_H				0x00000024
RK3568_DECLARE_IOMUX_REGISTER(gpio2a_iomux_h, gpio2a4_sel, gpio2a5_sel, gpio2a6_sel, gpio2a7_sel)

#define RK3568_GPIO2_A7							0
#define RK3568_SDMMC1_CMD						1
#define RK3568_GMAC0_TXD3						2
#define RK3568_UART9_RXM0						3

#define RK3568_GPIO2_A6							0
#define RK3568_SDMMC1_D3						1
#define RK3568_GMAC0_TXD2						2
#define RK3568_UART7_TXM0						3

#define RK3568_GPIO2_A5							0
#define RK3568_SDMMC1_D2						1
#define RK3568_GMAC0_RXCLK						2
#define RK3568_UART7_RXM0						3

#define RK3568_GPIO2_A4							0
#define RK3568_SDMMC1_D1						1
#define RK3568_GMAC0_RXD3						2
#define RK3568_UART6_TXM0						3

#define RK3568_GRF_GPIO2B_IOMUX_L				0x00000028
RK3568_DECLARE_IOMUX_REGISTER(gpio2b_iomux_l, gpio2b0_sel, gpio2b1_sel, gpio2b2_sel, gpio2b3_sel)

#define RK3568_GPIO2_B3							0
#define RK3568_GMAC0_TXD0						1
#define RK3568_UART1_RXM0						2

#define RK3568_GPIO2_B2							0
#define RK3568_SDMMC1_DET						1
#define RK3568_I2C4_SCLM1						2
#define RK3568_UART8_CTSNM0						3
#define RK3568_CAN2_TXM1						4

#define RK3568_GPIO2_B1							0
#define RK3568_SDMMC1_PWREN						1
#define RK3568_I2C4_SDAM1						2
#define RK3568_UART8_RTSNM0						3
#define RK3568_CAN2_RXM1						4

#define RK3568_GPIO2_B0							0
#define RK3568_SDMMC1_CLK						1
#define RK3568_GMAC0_TXCLK						2
#define RK3568_UART9_TXM0						3

#define RK3568_GRF_GPIO2B_IOMUX_H				0x0000002C
RK3568_DECLARE_IOMUX_REGISTER(gpio2b_iomux_h, gpio2b4_sel, gpio2b5_sel, gpio2b6_sel, gpio2b7_sel)

#define RK3568_GPIO2_B7							0
#define RK3568_I2S2_SCLKRXM0					1
#define RK3568_GMAC0_RXD1						2
#define RK3568_UART6_RTSNM0						3
#define RK3568_SPI1_MOSIM0						4

#define RK3568_GPIO2_B6							0
#define RK3568_GMAC0_RXD0						1
#define RK3568_UART1_CTSNM0						2
#define RK3568_SPI1_MISOM0						3

#define RK3568_GPIO2_B5							0
#define RK3568_GMAC0_TXEN						1
#define RK3568_UART1_RTSNM0						2
#define RK3568_SPI1_CLKM0						3

#define RK3568_GPIO2_B4							0
#define RK3568_GMAC0_TXD1						1
#define RK3568_UART1_TXM0						2

#define RK3568_GRF_GPIO2C_IOMUX_L				0x00000030
RK3568_DECLARE_IOMUX_REGISTER(gpio2c_iomux_l, gpio2c0_sel, gpio2c1_sel, gpio2c2_sel, gpio2c3_sel)

#define RK3568_GPIO2_C3							0
#define RK3568_I2S2_LRCKTXM0					1
#define RK3568_GMAC0_MDC						2
#define RK3568_UART9_RTSNM0						3
#define RK3568_SPI2_MOSIM0						4

#define RK3568_GPIO2_C2							0
#define RK3568_I2S2_SCLKTXM0					1
#define RK3568_GMAC0_MCLKINOUT					2
#define RK3568_UART7_CTSNM0						3
#define RK3568_SPI2_MISOM0						4

#define RK3568_GPIO2_C1							0
#define RK3568_I2S2_MCLKM0						1
#define RK3568_ETH0_REFCLKO25M					2
#define RK3568_UART7_RTSNM0						3
#define RK3568_SPI2_CLKM0						4

#define RK3568_GPIO2_C0							0
#define RK3568_I2S2_LRCKRXM0					1
#define RK3568_GMAC0_RXDVCRS					2
#define RK3568_UART6_CTSNM0						3
#define RK3568_SPI1_CS0M0						4

#define RK3568_GRF_GPIO2C_IOMUX_H				0x00000034
RK3568_DECLARE_IOMUX_REGISTER(gpio2c_iomux_h, gpio2c4_sel, gpio2c5_sel, gpio2c6_sel, gpio2c7_sel) /* NOTE: gpio2c7_sel does not exist */

#define RK3568_GPIO2_C6							0
#define RK3568_CLK32K_OUT1						1
#define RK3568_UART8_RXM0						2
#define RK3568_SPI1_CS1M0						3

#define RK3568_GPIO2_C5							0
#define RK3568_I2S2_SDIM0						1
#define RK3568_GMAC0_RXER						2
#define RK3568_UART8_TXM0						3
#define RK3568_SPI2_CS1M0						4

#define RK3568_GPIO2_C4							0
#define RK3568_I2S2_SDOM0						1
#define RK3568_GMAC0_MDIO						2
#define RK3568_UART9_CTSNM0						3
#define RK3568_SPI2_CS0M0						4

#define RK3568_GRF_GPIO2D_IOMUX_L				0x00000038
RK3568_DECLARE_IOMUX_REGISTER(gpio2d_iomux_l, gpio2d0_sel, gpio2d1_sel, gpio2d2_sel, gpio2d3_sel)

#define RK3568_GPIO2_D3							0
#define RK3568_LCDC_D3							1
#define RK3568_BT656_D3M0						2
#define RK3568_SPI0_CLKM1						3
#define RK3568_PCIE30X1_WAKENM1					4
#define RK3568_I2S1_SD0M2						5

#define RK3568_GPIO2_D2							0
#define RK3568_LCDC_D2							1
#define RK3568_BT656_D2M0						2
#define RK3568_SPI0_CS0M1						3
#define RK3568_PCIE30X1_CLKREQNM1				4
#define RK3568_I2S1_LRCKTXM2					5

#define RK3568_GPIO2_D1							0
#define RK3568_LCDC_D1							1
#define RK3568_BT656_D1M0						2
#define RK3568_SPI0_MOSIM1						3
#define RK3568_PCIE20_WAKENM1					4
#define RK3568_I2S1_SCLKTXM2					5

#define RK3568_GPIO2_D0							0
#define RK3568_LCDC_D0							1
#define RK3568_BT656_D0M0						2
#define RK3568_SPI0_MISOM1						3
#define RK3568_PCIE20_CLKREQNM1					4
#define RK3568_I2S1_MCLKM2						5

#define RK3568_GRF_GPIO2D_IOMUX_H				0x0000003C
RK3568_DECLARE_IOMUX_REGISTER(gpio2d_iomux_h, gpio2d4_sel, gpio2d5_sel, gpio2d6_sel, gpio2d7_sel)

#define RK3568_GPIO2_D7							0
#define RK3568_LCDC_D7							1
#define RK3568_BT656_D7M0						2
#define RK3568_SPI2_MISOM1						3
#define RK3568_UART8_TXM1 						4
#define RK3568_I2S1_SDO0M2						5

#define RK3568_GPIO2_D6							0
#define RK3568_LCDC_D6							1
#define RK3568_BT656_D6M0						2
#define RK3568_SPI2_MOSIM1						3
#define RK3568_PCIE30X2_PERSTNM1				4
#define RK3568_I2S1_SDI3M2						5

#define RK3568_GPIO2_D5							0
#define RK3568_LCDC_D5							1
#define RK3568_BT656_D5M0						2
#define RK3568_SPI2_CS0M1						3
#define RK3568_PCIE30X2_WAKENM1					4
#define RK3568_I2S1_SDI2M2						5

#define RK3568_GPIO2_D4							0
#define RK3568_LCDC_D4							1
#define RK3568_BT656_D4M0						2
#define RK3568_SPI2_CS1M1						3
#define RK3568_PCIE30X2_CLKREQNM1				4
#define RK3568_I2S1_SDI1M2						5

#define RK3568_GRF_GPIO3A_IOMUX_L				0x00000040
RK3568_DECLARE_IOMUX_REGISTER(gpio3a_iomux_l, gpio3a0_sel, gpio3a1_sel, gpio3a2_sel, gpio3a3_sel) // NOTE: union gpio3a_iomux_l

#define RK3568_GPIO3_A3							0
#define RK3568_LCDC_D10							1
#define RK3568_BT1120_D2						2
#define RK3568_GMAC1_TXD3M0						3
#define RK3568_I2S3_SCLKM0						4
#define RK3568_SDMMC2_D2M1						5

#define RK3568_GPIO3_A2							0
#define RK3568_LCDC_D9							1
#define RK3568_BT1120_D1						2
#define RK3568_GMAC1_TXD2M0						3
#define RK3568_I2S3_MCLKM0						4
#define RK3568_SDMMC2_D1M1						5

#define RK3568_GPIO3_A1							0
#define RK3568_LCDC_D8							1
#define RK3568_BT1120_D0						2
#define RK3568_SPI1_CS0M1						3
#define RK3568_PCIE30X1_PERSTNM1				4
#define RK3568_SDMMC2_D0M1						5

#define RK3568_GPIO3_A0							0
#define RK3568_LCDC_CLK							1
#define RK3568_BT656_CLKM0						2
#define RK3568_SPI2_CLKM1						3
#define RK3568_UART8_RXM1						4
#define RK3568_I2S1_SDO1M2						5

#define RK3568_GRF_GPIO3A_IOMUX_H				0x00000044
RK3568_DECLARE_IOMUX_REGISTER(gpio3a_iomux_h, gpio3a4_sel, gpio3a5_sel, gpio3a6_sel, gpio3a7_sel) // NOTE: union gpio3a_iomux_h

#define RK3568_GPIO3_A7							0
#define RK3568_LCDC_D14							1
#define RK3568_BT1120_D5						2
#define RK3568_GMAC1_RXCLKM0					3
#define RK3568_SDMMC2_DETM1						4

#define RK3568_GPIO3_A6							0
#define RK3568_LCDC_D13							1
#define RK3568_BT1120_CLK						2
#define RK3568_GMAC1_TXCLKM0					3
#define RK3568_I2S3_SDIM0						4
#define RK3568_SDMMC2_CLKM1						5

#define RK3568_GPIO3_A5							0
#define RK3568_LCDC_D12							1
#define RK3568_BT1120_D4						2
#define RK3568_GMAC1_RXD3M0						3
#define RK3568_I2S3_SDOM0						4
#define RK3568_SDMMC2_CMDM1						5

#define RK3568_GPIO3_A4							0
#define RK3568_LCDC_D11							1
#define RK3568_BT1120_D3						2
#define RK3568_GMAC1_RXD2M0						3
#define RK3568_I2S3_LRCKM0						4
#define RK3568_SDMMC2_D3M1						5

#define RK3568_GRF_GPIO3B_IOMUX_L				0x00000048
RK3568_DECLARE_IOMUX_REGISTER(gpio3b_iomux_l, gpio3b0_sel, gpio3b1_sel, gpio3b2_sel, gpio3b3_sel) // NOTE: union gpio3b_iomux_l

#define RK3568_GPIO3_B3							0
#define RK3568_LCDC_D18							1
#define RK3568_BT1120_D9						2
#define RK3568_GMAC1_RXDVCRSM0					3
#define RK3568_I2C5_SCLM0						4
#define RK3568_PDM_SDI0M2						5

#define RK3568_GPIO3_B2							0
#define RK3568_LCDC_D17							1
#define RK3568_BT1120_D8						2
#define RK3568_GMAC1_RXD1M0						3
#define RK3568_UART4_TXM1						4
#define RK3568_PWM9_M0							5

#define RK3568_GPIO3_B1							0
#define RK3568_LCDC_D16							1
#define RK3568_BT1120_D7						2
#define RK3568_GMAC1_RXD0M0						3
#define RK3568_UART4_RXM1						4
#define RK3568_PWM8_M0							5

#define RK3568_GRF_GPIO3B_IOMUX_H				0x0000004c
RK3568_DECLARE_IOMUX_REGISTER(gpio3b_iomux_h, gpio3b4_sel, gpio3b5_sel, gpio3b6_sel, gpio3b7_sel) // NOTE: union gpio3b_iomux_h

#define RK3568_GPIO3_B7							0
#define RK3568_LCDC_D22							1
#define RK3568_PWM12_M0							2
#define RK3568_GMAC1_TXENM0						3
#define RK3568_UART3_TXM1						4
#define RK3568_PDM_SDI2M2						5

#define RK3568_GPIO3_B6							0
#define RK3568_LCDC_D21							1
#define RK3568_BT1120_D12						2
#define RK3568_GMAC1_TXD1M0						3
#define RK3568_I2C3_SDAM1						4
#define RK3568_PWM11_M0							5

#define RK3568_GPIO3_B5							0
#define RK3568_LCDC_D20							1
#define RK3568_BT1120_D11						2
#define RK3568_GMAC1_TXD0M0						3
#define RK3568_I2C3_SCLM1						4
#define RK3568_PWM10_M0							5

#define RK3568_GPIO3_B4							0
#define RK3568_LCDC_D19							1
#define RK3568_BT1120_D10						2
#define RK3568_GMAC1_RXERM0						3
#define RK3568_I2C5_SDAM0						4
#define RK3568_PDM_SDI1M2						5

#define RK3568_GRF_GPIO3C_IOMUX_L				0x00000050
RK3568_DECLARE_IOMUX_REGISTER(gpio3c_iomux_l, gpio3c0_sel, gpio3c1_sel, gpio3c2_sel, gpio3c3_sel) // NOTE: union gpio3c_iomux_l

#define RK3568_GPIO3_C3							0
#define RK3568_LCDC_DEN							1
#define RK3568_BT1120_D14						2
#define RK3568_SPI1_CLKM1						3
#define RK3568_UART5_RXM1						4
#define RK3568_I2S1_SCLKRXM2					5

#define RK3568_GPIO3_C2							0
#define RK3568_LCDC_VSYNC						1
#define RK3568_BT1120_D14						2
#define RK3568_SPI1_MISOM1						3
#define RK3568_UART5_TXM1						4
#define RK3568_I2S1_SDO3M2						5

#define RK3568_GPIO3_C1							0
#define RK3568_LCDC_HSYNC						1
#define RK3568_BT1120_D13						2
#define RK3568_SPI1_MOSIM1						3
#define RK3568_PCIE20_PERSTNM1					4
#define RK3568_I2S1_SDO2M2						5

#define RK3568_GPIO3_C0							0
#define RK3568_LCDC_D23							1
#define RK3568_PWM13_M0							2
#define RK3568_GMAC1_MCLKINOUTM0				3
#define RK3568_UART3_RXM1						4
#define RK3568_PDM_SDI3M2						5

#define RK3568_GRF_GPIO3C_IOMUX_H				0x00000054
RK3568_DECLARE_IOMUX_REGISTER(gpio3c_iomux_h, gpio3c4_sel, gpio3c5_sel, gpio3c6_sel, gpio3c7_sel) // NOTE: union gpio3c_iomux_h

#define RK3568_GPIO3_C5							0
#define RK3568_PWM15_M0							1
#define RK3568_SPDIF_TXM1						2
#define RK3568_GMAC1_MDIOM0						3
#define RK3568_UART7_RXM1						4
#define RK3568_I2S1_LRCKRXM2					5

#define RK3568_GPIO3_C4							0
#define RK3568_PWM14_M0							1
#define RK3568_VOP_PWMM1						2
#define RK3568_GMAC1_MDCM0						3
#define RK3568_UART7_TXM1						4
#define RK3568_PDM_CLK1M2						5

#define RK3568_GRF_GPIO3D_IOMUX_H				0x0000005c
RK3568_DECLARE_IOMUX_REGISTER(gpio3d_iomux_h, gpio3d4_sel, gpio3d5_sel, gpio3d6_sel, gpio3d7_sel) // NOTE: union gpio3d_iomux_h

#define RK3568_GPIO3_D7							0
#define RK3568_CIF_D9							1
#define RK3568_EBC_SDDO9						2
#define RK3568_GMAC1_TXD3M1						3
#define RK3568_UART1_RXM1						4
#define RK3568_PDM_SDI0M1						5

#define RK3568_GPIO3_D6							0
#define RK3568_CIF_D8							1
#define RK3568_EBC_SDDO8						2
#define RK3568_GMAC1_TXD2M1						3
#define RK3568_UART1_TXM1						4
#define RK3568_PDM_CLK0M1						5

#define RK3568_GRF_GPIO4A_IOMUX_L				0x00000060
RK3568_DECLARE_IOMUX_REGISTER(gpio4a_iomux_l, gpio4a0_sel, gpio4a1_sel, gpio4a2_sel, gpio4a3_sel) // NOTE: union gpio4a_iomux_l

#define RK3568_GPIO4_A3							0
#define RK3568_CIF_D13							1
#define RK3568_EBC_SDDO13						2
#define RK3568_GMAC1_RXCLKM1					3
#define RK3568_UART7_RXM2						4
#define RK3568_PDM_SDI3M1						5

#define RK3568_GPIO4_A2							0
#define RK3568_CIF_D12							1
#define RK3568_EBC_SDDO12						2
#define RK3568_GMAC1_RXD3M1						3
#define RK3568_UART7_TXM2						4
#define RK3568_PDM_SDI2M1						5

#define RK3568_GPIO4_A1							0
#define RK3568_CIF_D11							1
#define RK3568_EBC_SDDO11						2
#define RK3568_GMAC1_RXD2M1						3
#define RK3568_PDM_SDI1M1						4

#define RK3568_GPIO4_A0							0
#define RK3568_CIF_D10							1
#define RK3568_EBC_SDDO10						2
#define RK3568_GMAC1_TXCLKM1					3
#define RK3568_PDM_CLK1M1						4

#define RK3568_GRF_GPIO4A_IOMUX_H				0x00000064
RK3568_DECLARE_IOMUX_REGISTER(gpio4a_iomux_h, gpio4a4_sel, gpio4a5_sel, gpio4a6_sel, gpio4a7_sel) // NOTE: union gpio4a_iomux_h

#define RK3568_GPIO4_A7							0
#define RK3568_CAM_CLKOUT0						1
#define RK3568_EBC_SDCE1						2
#define RK3568_GMAC1_RXD0M1						3
#define RK3568_SPI3_CS1M0						4
#define RK3568_I2S1_LRCKRXM1					5

#define RK3568_GPIO4_A6							0
#define RK3568_ISP_FLASHTRIGOUT					1
#define RK3568_EBC_SDCE0						2
#define RK3568_GMAC1_TXENM1						3
#define RK3568_SPI3_CS0M0						4
#define RK3568_I2S1_SCLKRXM1					5

#define RK3568_GPIO4_A5							0
#define RK3568_CIF_D15							1
#define RK3568_EBC_SDDO15						2
#define RK3568_GMAC1_TXD1M1						3
#define RK3568_UART9_RXM2						4
#define RK3568_I2S2_LRCKRXM1					5

#define RK3568_GPIO4_A4							0
#define RK3568_CIF_D14							1
#define RK3568_EBC_SDDO14						2
#define RK3568_GMAC1_TXD0M1						3
#define RK3568_UART9_TXM2						4
#define RK3568_I2S2_LRCKTXM1					5

#define RK3568_GRF_GPIO4B_IOMUX_L				0x00000068
RK3568_DECLARE_IOMUX_REGISTER(gpio4b_iomux_l, gpio4b0_sel, gpio4b1_sel, gpio4b2_sel, gpio4b3_sel) // NOTE: union gpio4b_iomux_l

#define RK3568_GPIO4_B3							0
#define RK3568_I2C4_SCLM0						1
#define RK3568_EBC_GDOE							2
#define RK3568_ETH1_REFCLKO25MM1				3
#define RK3568_SPI3_CLKM0						4
#define RK3568_I2S_SDOM1						5

#define RK3568_GPIO4_B2							0
#define RK3568_I2C4_SDAM0						1
#define RK3568_EBC_VCOM							2
#define RK3568_GMAC1_RXERM1						3
#define RK3568_SPI3_MOSIM0						4
#define RK3568_I2S2_SDIM1						5

#define RK3568_GPIO4_B1							0
#define RK3568_ISP_PRELIGHTTRIG					1
#define RK3568_EBC_SDCE3						2
#define RK3568_GMAC1_RXDVCRSM1					3
#define RK3568_I2S1_SDO2M1						4

#define RK3568_GPIO4_B0							0
#define RK3568_CAM_CLKOUT1						1
#define RK3568_EBC_SDCE2						2
#define RK3568_GMAC1_RXD1M1						3
#define RK3568_SPI3_MISOM0						4
#define RK3568_I2S1_SDO1M1						5

#define RK3568_GRF_GPIO4B_IOMUX_H				0x0000006c
RK3568_DECLARE_IOMUX_REGISTER(gpio4b_iomux_h, gpio4b4_sel, gpio4b5_sel, gpio4b6_sel, gpio4b7_sel) // NOTE: union gpio4b_iomux_h

#define RK3568_GPIO4_B7							0
#define RK3568_CIF_VSYNC						1
#define RK3568_EBC_SDOE							2
#define RK3568_GMAC1_MDIOM1						3
#define RK3568_I2S2_SCLKTXM1					4

#define RK3568_GPIO4_B6							0
#define RK3568_CIF_HREF							1
#define RK3568_EBC_SDLE							2
#define RK3568_GMAC1_MDCM1						3
#define RK3568_UART1_RTSNM1						4
#define RK3568_I2S2_MCLKM1						5

#define RK3568_GPIO4_B5							0
#define RK3568_I2C2_SCLM1						1
#define RK3568_EBC_SDSHR						2
#define RK3568_CAN2_TXM0						3
#define RK3568_I2S1_SDO3M1						4

#define RK3568_GPIO4_B4							0
#define RK3568_I2C2_SDAM1						1
#define RK3568_EBC_GDSP							2
#define RK3568_CAN2_RXM0						3
#define RK3568_ISP_FLASHTRIGIN					4
#define RK3568_BT656_CLKM1						5

#define RK3568_GRF_GPIO4C_IOMUX_L				0x00000070
RK3568_DECLARE_IOMUX_REGISTER(gpio4c_iomux_l, gpio4c0_sel, gpio4c1_sel, gpio4c2_sel, gpio4c3_sel) // NOTE: union gpio4c_iomux_l

#define RK3568_GPIO4_C3							0
#define RK3568_PWM15_M1							1
#define RK3568_SPI3_MOSIM1						2
#define RK3568_CAN1_TXM1						3
#define RK3568_PCIE30X2_WAKENM2					4
#define RK3568_I2S3_SCLKM1						5

#define RK3568_GPIO4_C2							0
#define RK3568_PWM14_M1							1
#define RK3568_SPI3_CLKM1						2
#define RK3568_CAN1_RXM1						3
#define RK3568_PCIE30X2_CLKREQNM2				4
#define RK3568_I2S3_MCLKM1						5

#define RK3568_GPIO4_C1							0
#define RK3568_CIF_CLKIN						1
#define RK3568_EBC_SDCLK						2
#define RK3568_GMAC1_MCLKINOUTM1				3
#define RK3568_UART1_CTSNM1						4
#define RK3568_I2S2_SCLKRXM1					5

#define RK3568_GPIO4_C0							0
#define RK3568_CIF_CLKOUT						1
#define RK3568_EBC_GDCLK						2
#define RK3568_PWM11_M1							3

#define RK3568_GRF_GPIO4C_IOMUX_H				0x00000074
RK3568_DECLARE_IOMUX_REGISTER(gpio4c_iomux_h, gpio4c4_sel, gpio4c5_sel, gpio4c6_sel, gpio4c7_sel) // NOTE: union gpio4c_iomux_h

#define RK3568_GPIO4_C7							0
#define RK3568_HDMITX_SCL						1
#define RK3568_I2C5_SCLM1						2

#define RK3568_GPIO4_C6							0
#define RK3568_PWM13_M1							1
#define RK3568_SPI3_CS0M1						2
#define RK3568_SATA0_ACTLED						3
#define RK3568_UART9_RXM1						4
#define RK3568_I2S3_SDIM1						5

#define RK3568_GPIO4_C5							0
#define RK3568_PWM12_M1							1
#define RK3568_SPI3_MISOM1						2
#define RK3568_SATA1_ACTLED						3
#define RK3568_UART9_TXM1						4
#define RK3568_I2S3_SDOM1						5

#define RK3568_GPIO4_C4							0
#define RK3568_EDPDP_HPDINM0					1
#define RK3568_SPDIF_TXM2						2
#define RK3568_SATA2_ACTLED						3
#define RK3568_PCIE30X2_PERSTNM2				4
#define RK3568_I2S3_LRCKM1						5

#define RK3568_GRF_GPIO4D_IOMUX_L				0x00000078
RK3568_DECLARE_IOMUX_REGISTER(gpio4d_iomux_l, gpio4d0_sel, gpio4d1_sel, gpio4d2_sel, gpio4d3_sel_reserved) // NOTE: union gpio4d_iomux_l

#define RK3568_GPIO4_D2							0

#define RK3568_GPIO4_D1							0
#define RK3568_HDMITX_CECM0						1
#define RK3568_SPI3_CS1M1						2

#define RK3568_GPIO4_D0							0
#define RK3568_HDMITX_SDA						1
#define RK3568_I2C5_SDAM1						2

#define RK3568_GRF_IOFUNC_SEL0					0x00000300
#define RK3568_GRF_IOFUNC_SEL1					0x00000304
#define RK3568_GRF_IOFUNC_SEL3					0x0000030C
#define RK3568_GRF_IOFUNC_SEL4					0x00000310

#define RK3568_I2C2_IOMUX_SEL					_ONEBIT32L(14)
#define RK3568_HDMITX_IOMUX_SEL					_ONEBIT32L(10)
#define RK3568_GMAC1_IOMUX_SEL					_ONEBIT32L(8)
#define RK3568_EDP_HPD_IOMUX_SEL				_ONEBIT32L(6)
#define RK3568_CAN2_IOMUX_SEL					_ONEBIT32L(4)
#define RK3568_CAN1_IOMUX_SEL					_ONEBIT32L(2)
#define RK3568_CAN0_IOMUX_SEL					_ONEBIT32L(0)

#define RK3568_PWM8_IOMUX_SEL					_ONEBIT32L(14)
#define RK3568_I2C5_IOMUX_SEL					_ONEBIT32L(4)
#define RK3568_I2C4_IOMUX_SEL					_ONEBIT32L(2)
#define RK3568_I2C3_IOMUX_SEL					_ONEBIT32L(0)

#define RK3568_SPI1_IOMUX_SEL					_ONEBIT32L(2)
#define RK3568_SPI0_IOMUX_SEL					_ONEBIT32L(0)

typedef union {
	uint32_t    raw;
	struct {
		struct {
			uint16_t
				can0_iomux_sel      : 1,
				reserved0           : 1,
				can1_iomux_sel      : 1,
				reserved1           : 1,
				can2_iomux_sel      : 1,
				reserved2           : 1,
				edp_hpd_iomux_sel   : 1,
				reserved3           : 1,
				gmac1_iomux_sel     : 1,
				reserved4           : 1,
				hdmitx_iomux_sel    : 1,
				reserved5           : 3,
				i2c2_iomux_sel      : 1,
				reserved6           : 1;
		} __attribute__((__packed__)) bits, write_en;
	};
} rk3568_grf_iofunc_sel0_t;

typedef union {
	uint32_t    raw;
	struct {
		struct {
			uint16_t
				i2c3_iomux_sel      : 1,
				reserved0           : 1,
				i2c4_iomux_sel      : 1,
				reserved1           : 1,
				i2c5_iomux_sel      : 1,
				reserved2           : 9,
				pwm8_iomux_sel      : 1,
				reserved3           : 1;
		} __attribute__((__packed__)) bits, write_en;
	};
} rk3568_grf_iofunc_sel1_t;

typedef union {
	uint32_t    raw;
	struct {
		struct {
			uint16_t
				spi0_iomux_sel      : 1,
				reserved0           : 1,
				spi1_iomux_sel      : 1,
				reserved1           : 1,
				spi2_iomux_sel      : 1,
				reserved2           : 1,
				spi3_iomux_sel      : 1,
				reserved3           : 1,
				uart1_iomux_sel     : 1,
				reserved4           : 1,
				uart2_iomux_sel     : 1,
				reserved5           : 1,
				uart3_iomux_sel     : 1,
				reserved6           : 1,
				uart4_iomux_sel     : 1,
				reserved7           : 1;
		} __attribute__((__packed__)) bits, write_en;
	};
} rk3568_grf_iofunc_sel3_t;

typedef union {
	uint32_t    raw;
	struct {
		struct {
			uint16_t
				uart5_iomux_sel     : 1,
				reserved0           : 1,
				uart6_iomux_sel     : 1,
				reserved1           : 1,
				uart7_iomux_sel     : 2,
				uart8_iomux_sel     : 1,
				reserved2           : 1,
				uart9_iomux_sel     : 2,
				i2s1_iomux_sel      : 2,
				i2s2_iomux_sel      : 1,
				reserved3           : 1,
				i2s3_iomux_sel      : 1,
				reserved4           : 1;
		} __attribute__((__packed__)) bits, write_en;
	};
} rk3568_grf_iofunc_sel4_t;

#define RK3568_SOLUTION_M0 0
#define RK3568_SOLUTION_M1 1
#define RK3568_SOLUTION_M2 2

#endif
