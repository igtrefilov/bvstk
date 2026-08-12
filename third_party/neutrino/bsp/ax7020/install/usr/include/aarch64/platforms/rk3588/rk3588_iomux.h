/*
 * (c) 2025-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __RK3588_IOMUX_H__
#define __RK3588_IOMUX_H__

#include <sys/platform.h>

#define RK3588_BUS_IOC_BASE 0xfd5f8000

#define RK3588_DECLARE_IOMUX_REGISTER(register_name, field0, field1, field2, field3) \
typedef union { 										\
	uint32_t raw;										\
	struct {											\
		struct {										\
			uint16_t									\
				field0				: 4,				\
				field1				: 4,				\
				field2				: 4,				\
				field3				: 4;				\
		} __attribute__((__packed__)) bits, write_en; 	\
	}; 													\
} register_name##_t;

#define RK3588_SET_IOMUX_FIELD(addr, offset, register_name, field, val) {	\
	register_name##_t reg = {0};						\
														\
	reg.bits.field = val;								\
	reg.write_en.field = ~0;							\
														\
	out32(addr + offset, reg.raw);						\
}

#define RK3588_SET_IOMUX_SELECTOR RK3588_SET_IOMUX_FIELD

#define GPIO_MUX 0

#define RK3588_BUS_IOC_GPIO0B_IOMUX_SEL_H 		0x0000000c
RK3588_DECLARE_IOMUX_REGISTER(gpio0b_iomux_h, reserved0, gpio0b5_sel, gpio0b6_sel, gpio0b7_sel)

#define I2C1_SCL_M0						0x9
#define UART2_TX_M0						0xa
#define PCIE30X1_1_CLKREQN_M0			0xc

#define I2C1_SDA_M0						0x9
#define UART2_RX_M0						0xa
#define PCIE30X1_1_WAKEN_M0				0xc

#define SPI0_CS1_M0						0x8
#define I2C2_SCL_M0						0x9
#define CAN0_TX_M0						0xb
#define PCIE30X1_1_PERSTN_M0			0xc

#define RK3588_BUS_IOC_GPIO0C_IOMUX_SEL_L		0x00000010
RK3588_DECLARE_IOMUX_REGISTER(gpio0c_iomux_l, gpio0c0_sel, reserved0, reserved1, reserved2)

#define SPI0_MOSI_M0					0x8
#define I2C2_SDA_M0						0x9
#define CAN0_RX_M0						0xb
#define PCIE30X1_0_CLKREQN_M0			0xc

#define RK3588_BUS_IOC_GPIO0D_IOMUX_SEL_H		0x0000001c
RK3588_DECLARE_IOMUX_REGISTER(gpio0d_iomux_h, gpio0d4_sel, gpio0d5_sel, reserved0, reserved1)

#define SPI3_CS0_M2						0x8
#define I2C1_SCL_M2						0x9
#define CAN2_RX_M1						0xa
#define HDMI_TX0_SDA_M1					0xb
#define PCIE30X2_PERSTN_M0				0xc
#define SATA_CPDET						0xd

#define SPI3_CS1_M2						0x8
#define I2C1_SDA_M2						0x9
#define CAN2_TX_M1						0xa
#define HDMI_TX0_SCL_M1					0xb
#define SATA_MP_SWITCH					0xd

#define RK3588_BUS_IOC_GPIO2A_IOMUX_SEL_H 		0x00000044
RK3588_DECLARE_IOMUX_REGISTER(gpio2a_iomux_h, reserved0, reserved1, gpio2a6_sel, gpio2a7_sel)

#define GMAC0_RXD2						0x1
#define SDIO_D0_M0						0x2
#define FSPI_D0_M1						0x3
#define UART6_RX_M0						0xa

#define GMAC0_RXD3						0x1
#define SDIO_D1_M0						0x2
#define FSPI_D1_M1						0x3
#define UART6_TX_M0						0xa

#define RK3588_BUS_IOC_GPIO2B_IOMUX_SEL_L 		0x00000048
RK3588_DECLARE_IOMUX_REGISTER(gpio2b_iomux_l, gpio2b0_sel, gpio2b1_sel, gpio2b2_sel, gpio2b3_sel)

#define GMAC0_RXCLK						0x1
#define SDIO_D2_M0						0x2
#define FSPI_D2_M1						0x3
#define I2C8_SCL_M1						0x9
#define UART6_RTSN_M0					0xa

#define GMAC0_TXD2						0x1
#define SDIO_D3_M0						0x2
#define FSPI_D3_M1						0x3
#define I2C8_SDA_M1						0x9
#define UART6_CTSN_M0					0xa

#define GMAC0_TXD3						0x1
#define SDIO_CMD_M0						0x2
#define I2C3_SCL_M3						0x9

#define GMAC0_TXCLK						0x1
#define SDIO_CLK_M0						0x2
#define FSPI_CLK_M1						0x3
#define I2C3_SDA_M3						0x9

#define RK3588_BUS_IOC_GPIO2B_IOMUX_SEL_H 		0x0000004c
RK3588_DECLARE_IOMUX_REGISTER(gpio2b_iomux_h, gpio2b4_sel, gpio2b5_sel, gpio2b6_sel, gpio2b7_sel)

#define GMAC0_PTP_REFCLK				0x1
#define FSPI_CS0N_M1					0x3
#define HDMI_TX1_SDA_M0					0x4
#define I2C4_SDA_M1						0x9
#define UART7_RX_M0						0xa

#define GMAC0_PPSTRING					0x1
#define FSPI_CS1N_M1					0x3
#define HDMI_TX1_SCL_M0					0x4
#define I2C4_SCL_M1						0x9
#define UART7_TX_M0						0xa

#define GMAC0_TXD0						0x1
#define I2S2_MCLK_M0					0x2
#define I2C5_SCL_M4						0x9
#define UART1_RX_M0						0xa

#define GMAC0_TXD1						0x1
#define I2S2_SCLK_M0					0x2
#define I2C5_SDA_M4						0x9
#define UART1_TX_M0						0xa

#define RK3588_BUS_IOC_GPIO2C_IOMUX_SEL_L 		0x00000050
RK3588_DECLARE_IOMUX_REGISTER(gpio2c_iomux_l, gpio2c0_sel, gpio2c1_sel, gpio2c2_sel, gpio2c3_sel)

#define GMAC0_TXEN						0x1
#define I2S2_LRCK_M0					0x2
#define SPI1_CLK_M0						0x8
#define I2C2_SDA_M1						0x9
#define UART1_RTSN_M0					0xa

#define GMAC0_RXD0						0x1
#define SPI1_MISO_M0					0x8
#define I2C2_SCL_M1						0x9
#define UART1_CTSN_M0					0xa

#define GMAC0_RXD1						0x1
#define SPI1_MOSI_M0					0x8
#define I2C6_SDA_M2						0x9
#define UART9_TX_M0						0xa

#define ETH0_REFCLKO_25M				0x1
#define I2S2_SDI_M0						0x2
#define SPI1_CS0_M0						0x8
#define I2C6_SCL_M2						0x9

#define RK3588_BUS_IOC_GPIO2C_IOMUX_SEL_H 		0x00000054
RK3588_DECLARE_IOMUX_REGISTER(gpio2c_iomux_h, gpio2c4_sel, gpio2c5_sel, reserved0, reserved1)

#define GMAC0_PPSCLK					0x1
#define TEST_CLKOUT_M1					0x3
#define HDMI_TX1_CEC_M0					0x4
#define SPI1_CS1_M0						0x8
#define UART9_RX_M0						0xa

#define CLK32K_OUT1						0x1

#define RK3588_BUS_IOC_GPIO3A_IOMUX_SEL_L 		0x00000060
RK3588_DECLARE_IOMUX_REGISTER(gpio3a_iomux_l, gpio3a0_sel, gpio3a1_sel, gpio3a2_sel, gpio3a3_sel)

#define GMAC1_TXD2						0x1
#define SDIO_D0_M1						0x2
#define I2S3_MCLK						0x3
#define FSPI_D0_M2						0x5
#define SPI4_MISO_M1					0x8
#define I2C6_SDA_M4						0x9
#define PWM10_M0						0xb

#define GMAC1_TXD3						0x1
#define SDIO_D1_M1						0x2
#define I2S3_SCLK						0x3
#define AUDDSM_LN						0x4
#define FSPI_D1_M2						0x5
#define SPI4_MOSI_M1					0x8
#define I2C6_SCL_M4						0x9
#define PWM11_IR_M0						0xb

#define GMAC1_RXD2						0x1
#define SDIO_D2_M1						0x2
#define I2S3_LRCK						0x3
#define AUDDSM_LP						0x4
#define FSPI_D2_M2						0x5
#define SPI4_CLK_M1						0x8
#define UART8_TX_M1						0xa

#define GMAC1_RXD3						0x1
#define SDIO_D3_M1						0x2
#define I2S3_SDO						0x3
#define AUDDSM_RN						0x4
#define FSPI_D3_M2						0x5
#define SPI4_CS0_M1						0x8
#define UART8_RX_M1						0xa

#define RK3588_BUS_IOC_GPIO3A_IOMUX_SEL_H 		0x00000064
RK3588_DECLARE_IOMUX_REGISTER(gpio3a_iomux_h, gpio3a4_sel, gpio3a5_sel, gpio3a6_sel, gpio3a7_sel)

#define GMAC1_TXCLK						0x1
#define SDIO_CMD_M1						0x2
#define I2S3_SDI						0x3
#define AUDDSM_RP						0x4
#define SPI4_CS1_M1						0x8
#define UART8_RTSN_M1					0xa

#define GMAC1_RXCLK						0x1
#define SDIO_CLK_M1						0x2
#define MIPI_CAMERA0_CLK_M1				0x4
#define FSPI_CLK_M2						0x5
#define I2C4_SDA_M0						0x9
#define UART8_CTSN_M1					0xa

#define ETH1_REFCLKO_25M				0x1
#define MIPI_CAMERA1_CLK_M1				0x4
#define I2C4_SCL_M0						0x9

#define GMAC1_RXD0						0x1
#define MIPI_CAMERA2_CLK_M1				0x4
#define PWM8_M0							0xb

#define RK3588_BUS_IOC_GPIO3B_IOMUX_SEL_L 		0x00000068
RK3588_DECLARE_IOMUX_REGISTER(gpio3b_iomux_l, gpio3b0_sel, gpio3b1_sel, gpio3b2_sel, gpio3b3_sel)

#define GMAC1_RXD1						0x1
#define MIPI_CAMERA3_CLK_M1				0x4
#define PWM9_M0							0xb

#define GMAC1_RXDV_CRS					0x1
#define MIPI_CAMERA4_CLK_M1				0x4
#define UART2_TX_M2						0xa
#define PWM2_M1							0xb

#define GMAC1_TXER						0x1
#define I2S2_SDI_M1						0x3
#define UART2_RX_M2						0xa
#define PWM3_IR_M1						0xb

#define GMAC1_TXD0						0x1
#define I2S2_SDO_M1						0x3
#define UART2_RTSN						0xa

#define RK3588_BUS_IOC_GPIO3B_IOMUX_SEL_H 		0x0000006c
RK3588_DECLARE_IOMUX_REGISTER(gpio3b_iomux_h, gpio3b4_sel, gpio3b5_sel, gpio3b6_sel, gpio3b7_sel)

#define GMAC1_TXD1						0x1
#define I2S2_MCLK_M1					0x3
#define UART2_CTSN						0xa

#define GMAC1_TXEN						0x1
#define I2S2_SCLK_M1					0x3
#define CAN1_RX_M0						0x9
#define UART3_TX_M1						0xa
#define PWM12_M0						0xb

#define GMAC1_MCLKINOUT					0x1
#define I2S2_LRCK_M1					0x3
#define CAN1_TX_M0						0x9
#define UART3_RX_M1						0xa
#define PWM13_M0						0xb

#define GMAC1_PTP_REF_CLK				0x1
#define HDMI_TX1_HPD_M1					0x5
#define SPI1_MOSI_M1					0x8
#define I2C3_SCL_M1						0x9

#define RK3588_BUS_IOC_GPIO3C_IOMUX_SEL_L 		0x00000070
RK3588_DECLARE_IOMUX_REGISTER(gpio3c_iomux_l, gpio3c0_sel, gpio3c1_sel, gpio3c2_sel, gpio3c3_sel)

#define GMAC1_PPSTRIG					0x1
#define SPI1_MISO_M1					0x8
#define I2C3_SDA_M1						0x9
#define UART7_TX_M1						0xa

#define GMAC1_PPSCLK					0x1
#define PCIE30X2_BUTTON_RSTN			0x4
#define SPI1_CLK_M1						0x8
#define UART7_RX_M1						0xa

#define GMAC1_MDC						0x1
#define MIPI_TE0						0x2
#define SPI1_CS0_M1						0x8
#define I2C8_SCL_M4						0x9
#define UART7_RTSN_M1					0xa
#define PWM14_M0						0xb

#define GMAC1_MDIO						0x1
#define MIPI_TE1						0x2
#define SPI1_CS1_M1						0x8
#define I2C8_SDA_M4						0x9
#define UART7_CTSN_M1					0xa
#define PWM15_IR_M0						0xb

#define RK3588_BUS_IOC_GPIO3C_IOMUX_SEL_H 		0x00000074
RK3588_DECLARE_IOMUX_REGISTER(gpio3c_iomux_h, gpio3c4_sel, gpio3c5_sel, gpio3c6_sel, gpio3c7_sel)

#define CIF_D8							0x1
#define FSPI_CS0N_M2					0x2
#define PCIE30X4_CLKREQN_M2				0x4
#define HDMI_TX1_CEC_M2					0x5
#define SPI3_CS0_M3						0x8
#define CAN2_RX_M0						0x9
#define UART5_TX_M1						0xa

#define CIF_D9							0x1
#define FSPI_CS1N_M2					0x2
#define PCIE30X4_WAKEN_M2				0x4
#define HDMI_TX1_SDA_M1					0x5
#define SPI3_CS1_M3						0x8
#define CAN2_TX_M0						0x9
#define UART5_RX_M1						0xa

#define CIF_D10							0x1
#define PCIE30X4_PERSTN_M2				0x4
#define HDMI_TX1_SCL_M1					0x5
#define SPI3_MISO_M3					0x8

#define CIF_D11							0x1
#define PCIE20X1_2_CLKREQN_M0			0x4
#define HDMI_TX0_SCL_M2					0x5
#define SPI3_MOSI_M3					0x8
#define I2C5_SCL_M0						0x9

#define RK3588_BUS_IOC_GPIO4B_IOMUX_SEL_L 		0x00000088
RK3588_DECLARE_IOMUX_REGISTER(gpio4b_iomux_l, gpio4b0_sel, gpio4b1_sel, gpio4b2_sel, gpio4b3_sel)

#define CIF_CLKIN						0x1
#define BT1120_CLKOUT					0x2
#define I2S1_SDI3_M0					0x3
#define PCIE30X2_PERSTN_M1				0x4
#define DDRPHYCH2_DTB0					0x7
#define SPI2_CS1_M1						0x8
#define I2C6_SDA_M3						0x9
#define UART8_TX_M0						0xa

#define MIPI_CAMERA0_CLK_M0				0x1
#define SPDIF1_TX_M1					0x2
#define I2S1_SDO0_M0					0x3
#define PCIE30X1_0_BUTTON_RSTN			0x4
#define SATA2_ACT_LED_M0				0x6
#define DDRPHYCH2_DTB1					0x7
#define SPI0_CS1_M1						0x8
#define I2C6_SCL_M3						0x9
#define UART8_RX_M0						0xa

#define CIF_HREF						0x1
#define BT1120_D8						0x2
#define I2S1_SDO1_M0					0x3
#define PCIE30X1_1_BUTTON_RSTN			0x4
#define DDRPHYCH2_DTB2					0x7
#define SPI0_CS0_M1						0x8
#define I2C7_SCL_M3						0x9
#define UART8_RTSN_M0					0xa
#define PWM14_M1						0xb
#define CAN1_RX_M1						0xc

#define CIF_VSYNC						0x1
#define BT1120_D9						0x2
#define I2S1_SDO2_M0					0x3
#define PCIE20X1_2_BUTTON_RSTN			0x4
#define DDRPHYCH2_DTB3					0x7
#define I2C7_SDA_M3						0x9
#define UART8_CTSN_M0					0xa
#define PWM15_IR_M1						0xb
#define CAN1_TX_M1						0xc

#define RK3588_BUS_IOC_GPIO4C_IOMUX_SEL_L 		0x00000090
RK3588_DECLARE_IOMUX_REGISTER(gpio4c_iomux_l, gpio4c0_sel, gpio4c1_sel, gpio4c2_sel, gpio4c3_sel)

#define BT1120_D14						0x2
#define PCIE20X1_2_WAKEN_M1				0x4
#define HDMI_TX0_SDA_M0					0x5
#define SPI3_CS0_M1						0x8
#define I2C8_SCL_M3						0x9

#define BT1120_D15						0x2
#define SPDIF1_TX_M2					0x3
#define PCIE20X1_2_PERSTN_M1			0x4
#define HDMI_TX0_CEC_M0					0x5
#define SPI3_CS1_M1						0x8
#define I2C8_SDA_M3						0x9
#define PWM6_M1							0xb

#define GMAC0_RXDV_CRS					0x1
#define SPI3_CS0_M0						0x8
#define UART7_RTSN_M0					0xa
#define PWM2_M2							0xb

#define GMAC0_MCLKINOUT					0x1
#define I2S2_SDO_M0						0x2
#define SPI3_CS1_M0						0x8
#define I2C7_SCL_M1						0x9
#define PWM4_M1							0xb

#define RK3588_BUS_IOC_GPIO4C_IOMUX_SEL_H 		0x00000094
RK3588_DECLARE_IOMUX_REGISTER(gpio4c_iomux_h, gpio4c4_sel, gpio4c5_sel, gpio4c6_sel, reserved0)

#define GMAC0_MDC						0x1
#define SPI3_MISO_M0					0x8
#define I2C7_SDA_M1						0x9
#define UART9_RTSN_M0					0xa
#define PWM5_M2							0xb

#define GMAC0_MDIO						0x1
#define SPI3_MOSI_M0					0x8
#define I2C0_SCL_M1						0x9
#define UART9_CTSN_M0					0xa
#define PWM6_M2							0xb

#define GMAC0_TXER						0x1
#define SPI3_CLK_M0						0x8
#define I2C0_SDA_M1						0x9
#define UART7_CTSN_M0					0xa
#define PWM7_IR_M3						0xb

#define RK3588_BUS_IOC_GPIO4D_IOMUX_SEL_H 		0x0000009C
RK3588_DECLARE_IOMUX_REGISTER(gpio4d_iomux_h, gpio4d4_sel, gpio4d5_sel, reserved0, reserved1)

#define SDMMC_CMD						0x1
#define PDM1_CLK1_M0					0x2
#define MCU_JTAG_TCK_M0					0x5
#define CAN0_TX_M1						0x9
#define UART5_RX_M0						0xa
#define PWM7_IR_M1						0xb

#define SDMMC_CLK						0x1
#define PDM1_CLK0_M0					0x2
#define TEST_CLKOUT_M0					0x4
#define MCU_JTAG_TMS_M0					0x5
#define CAN0_RX_M1						0x9
#define UART5_TX_M0						0xa

#endif
