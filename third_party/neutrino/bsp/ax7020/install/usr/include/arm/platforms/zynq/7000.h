/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * Xilinx Zynq 7000 processor with Dual ARMv7 Cortex-A9
 */

#ifndef __ZYNQ_7000_H__
#define __ZYNQ_7000_H__

#include <hw/vendor/xilinx/zynq.h>

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <sys/bitfield.h>
#endif /* __ASSEMBLER__ */


#define ZYNQ7000_STATIC_ASSERT_OFFSET_OF(type, field, offset)               \
    _Static_assert(offsetof(type, field) == (offset), "wrong field offset")


/* ----------------------------------------------------------------------------
 * System-level Control registers (SLCR)
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_SLCR_BASE_ADDR 0xF8000000
#define ZYNQ7000_SLCR_REG_SIZE  0x00000B78


/* SLCR registers offsets */

#define ZYNQ7000_SLCR_SCL_OFFSET                     0x00000000
#define ZYNQ7000_SLCR_LOCK_OFFSET                    0x00000004
#define ZYNQ7000_SLCR_UNLOCK_OFFSET                  0x00000008
#define ZYNQ7000_SLCR_LOCKSTA_OFFSET                 0x0000000C
#define ZYNQ7000_SLCR_ARM_PLL_CTRL_OFFSET            0x00000100
#define ZYNQ7000_SLCR_DDR_PLL_CTRL_OFFSET            0x00000104
#define ZYNQ7000_SLCR_IO_PLL_CTRL_OFFSET             0x00000108
#define ZYNQ7000_SLCR_PLL_STATUS_OFFSET              0x0000010C
#define ZYNQ7000_SLCR_ARM_PLL_CFG_OFFSET             0x00000110
#define ZYNQ7000_SLCR_DDR_PLL_CFG_OFFSET             0x00000114
#define ZYNQ7000_SLCR_IO_PLL_CFG_OFFSET              0x00000118
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_OFFSET            0x00000120
#define ZYNQ7000_SLCR_DDR_CLK_CTRL_OFFSET            0x00000124
#define ZYNQ7000_SLCR_DCI_CLK_CTRL_OFFSET            0x00000128
#define ZYNQ7000_SLCR_APER_CLK_CTRL_OFFSET           0x0000012C
#define ZYNQ7000_SLCR_USB0_CLK_CTRL_OFFSET           0x00000130
#define ZYNQ7000_SLCR_USB1_CLK_CTRL_OFFSET           0x00000134
#define ZYNQ7000_SLCR_GEM0_RCLK_CTRL_OFFSET          0x00000138
#define ZYNQ7000_SLCR_GEM1_RCLK_CTRL_OFFSET          0x0000013C
#define ZYNQ7000_SLCR_GEM0_CLK_CTRL_OFFSET           0x00000140
#define ZYNQ7000_SLCR_GEM1_CLK_CTRL_OFFSET           0x00000144
#define ZYNQ7000_SLCR_SMC_CLK_CTRL_OFFSET            0x00000148
#define ZYNQ7000_SLCR_LQSPI_CLK_CTRL_OFFSET          0x0000014C
#define ZYNQ7000_SLCR_SDIO_CLK_CTRL_OFFSET           0x00000150
#define ZYNQ7000_SLCR_UART_CLK_CTRL_OFFSET           0x00000154
#define ZYNQ7000_SLCR_SPI_CLK_CTRL_OFFSET            0x00000158
#define ZYNQ7000_SLCR_CAN_CLK_CTRL_OFFSET            0x0000015C
#define ZYNQ7000_SLCR_CAN_MIOCLK_CTRL_OFFSET         0x00000160
#define ZYNQ7000_SLCR_DBG_CLK_CTRL_OFFSET            0x00000164
#define ZYNQ7000_SLCR_PCAP_CLK_CTRL_OFFSET           0x00000168
#define ZYNQ7000_SLCR_TOPSW_CLK_CTRL_OFFSET          0x0000016C
#define ZYNQ7000_SLCR_FPGA0_CLK_CTRL_OFFSET          0x00000170
#define ZYNQ7000_SLCR_FPGA0_THR_CTRL_OFFSET          0x00000174
#define ZYNQ7000_SLCR_FPGA0_THR_CNT_OFFSET           0x00000178
#define ZYNQ7000_SLCR_FPGA0_THR_STA_OFFSET           0x0000017C
#define ZYNQ7000_SLCR_FPGA1_CLK_CTRL_OFFSET          0x00000180
#define ZYNQ7000_SLCR_FPGA1_THR_CTRL_OFFSET          0x00000184
#define ZYNQ7000_SLCR_FPGA1_THR_CNT_OFFSET           0x00000188
#define ZYNQ7000_SLCR_FPGA1_THR_STA_OFFSET           0x0000018C
#define ZYNQ7000_SLCR_FPGA2_CLK_CTRL_OFFSET          0x00000190
#define ZYNQ7000_SLCR_FPGA2_THR_CTRL_OFFSET          0x00000194
#define ZYNQ7000_SLCR_FPGA2_THR_CNT_OFFSET           0x00000198
#define ZYNQ7000_SLCR_FPGA2_THR_STA_OFFSET           0x0000019C
#define ZYNQ7000_SLCR_FPGA3_CLK_CTRL_OFFSET          0x000001A0
#define ZYNQ7000_SLCR_FPGA3_THR_CTRL_OFFSET          0x000001A4
#define ZYNQ7000_SLCR_FPGA3_THR_CNT_OFFSET           0x000001A8
#define ZYNQ7000_SLCR_FPGA3_THR_STA_OFFSET           0x000001AC
#define ZYNQ7000_SLCR_CLK_621_TRUE_OFFSET            0x000001C4
#define ZYNQ7000_SLCR_PSS_RST_CTRL_OFFSET            0x00000200
#define ZYNQ7000_SLCR_DDR_RST_CTRL_OFFSET            0x00000204
#define ZYNQ7000_SLCR_TOPSW_RST_CTRL_OFFSET          0x00000208
#define ZYNQ7000_SLCR_DMAC_RST_CTRL_OFFSET           0x0000020C
#define ZYNQ7000_SLCR_USB_RST_CTRL_OFFSET            0x00000210
#define ZYNQ7000_SLCR_GEM_RST_CTRL_OFFSET            0x00000214
#define ZYNQ7000_SLCR_SDIO_RST_CTRL_OFFSET           0x00000218
#define ZYNQ7000_SLCR_SPI_RST_CTRL_OFFSET            0x0000021C
#define ZYNQ7000_SLCR_CAN_RST_CTRL_OFFSET            0x00000220
#define ZYNQ7000_SLCR_I2C_RST_CTRL_OFFSET            0x00000224
#define ZYNQ7000_SLCR_UART_RST_CTRL_OFFSET           0x00000228
#define ZYNQ7000_SLCR_GPIO_RST_CTRL_OFFSET           0x0000022C
#define ZYNQ7000_SLCR_LQSPI_RST_CTRL_OFFSET          0x00000230
#define ZYNQ7000_SLCR_SMC_RST_CTRL_OFFSET            0x00000234
#define ZYNQ7000_SLCR_OCM_RST_CTRL_OFFSET            0x00000238
#define ZYNQ7000_SLCR_FPGA_RST_CTRL_OFFSET           0x00000240
#define ZYNQ7000_SLCR_A9_CPU_RST_CTRL_OFFSET         0x00000244
#define ZYNQ7000_SLCR_RS_AWDT_CTRL_OFFSET            0x0000024C
#define ZYNQ7000_SLCR_REBOOT_STATUS_OFFSET           0x00000258
#define ZYNQ7000_SLCR_BOOT_MODE_OFFSET               0x0000025C
#define ZYNQ7000_SLCR_APU_CTRL_OFFSET                0x00000300
#define ZYNQ7000_SLCR_WDT_CLK_SEL_OFFSET             0x00000304
#define ZYNQ7000_SLCR_TZ_DMA_NS_OFFSET               0x00000440
#define ZYNQ7000_SLCR_TZ_DMA_IRQ_NS_OFFSET           0x00000444
#define ZYNQ7000_SLCR_TZ_DMA_PERIPH_NS_OFFSET        0x00000448
#define ZYNQ7000_SLCR_PSS_IDCODE_OFFSET              0x00000530
#define ZYNQ7000_SLCR_DDR_URGENT_OFFSET              0x00000600
#define ZYNQ7000_SLCR_DDR_CAL_START_OFFSET           0x0000060C
#define ZYNQ7000_SLCR_DDR_REF_START_OFFSET           0x00000614
#define ZYNQ7000_SLCR_DDR_CMD_STA_OFFSET             0x00000618
#define ZYNQ7000_SLCR_DDR_URGENT_SEL_OFFSET          0x0000061C
#define ZYNQ7000_SLCR_DDR_DFI_STATUS_OFFSET          0x00000620
#define ZYNQ7000_SLCR_MIO_PIN_OFFSET(n)              (0x00000700 + (n) * 4)
#define ZYNQ7000_SLCR_MIO_LOOPBACK_OFFSET            0x00000804
#define ZYNQ7000_SLCR_MIO_MST_TRI0_OFFSET            0x0000080C
#define ZYNQ7000_SLCR_MIO_MST_TRI1_OFFSET            0x00000810
#define ZYNQ7000_SLCR_SD0_WP_CD_SEL_OFFSET           0x00000830
#define ZYNQ7000_SLCR_SD1_WP_CD_SEL_OFFSET           0x00000834
#define ZYNQ7000_SLCR_LVL_SHFTR_EN_OFFSET            0x00000900
#define ZYNQ7000_SLCR_OCM_CFG_OFFSET                 0x00000910
#define ZYNQ7000_SLCR_GPIOB_CTRL_OFFSET              0x00000B00
#define ZYNQ7000_SLCR_GPIOB_CFG_CMOS18_OFFSET        0x00000B04
#define ZYNQ7000_SLCR_GPIOB_CFG_CMOS25_OFFSET        0x00000B08
#define ZYNQ7000_SLCR_GPIOB_CFG_CMOS33_OFFSET        0x00000B0C
#define ZYNQ7000_SLCR_GPIOB_CFG_HSTL_OFFSET          0x00000B14
#define ZYNQ7000_SLCR_GPIOB_DRVR_BIAS_CTRL_OFFSET    0x00000B18
#define ZYNQ7000_SLCR_DDRIOB_ADDR0_OFFSET            0x00000B40
#define ZYNQ7000_SLCR_DDRIOB_ADDR1_OFFSET            0x00000B44
#define ZYNQ7000_SLCR_DDRIOB_DATA0_OFFSET            0x00000B48
#define ZYNQ7000_SLCR_DDRIOB_DATA1_OFFSET            0x00000B4C
#define ZYNQ7000_SLCR_DDRIOB_DIFF0_OFFSET            0x00000B50
#define ZYNQ7000_SLCR_DDRIOB_DIFF1_OFFSET            0x00000B54
#define ZYNQ7000_SLCR_DDRIOB_CLOCK_OFFSET            0x00000B58
#define ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_ADDR_OFFSET  0x00000B5C
#define ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_DATA_OFFSET  0x00000B60
#define ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_DIFF_OFFSET  0x00000B64
#define ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_CLOCK_OFFSET 0x00000B68
#define ZYNQ7000_SLCR_DDRIOB_DDR_CTRL_OFFSET         0x00000B6C
#define ZYNQ7000_SLCR_DDRIOB_DCI_CTRL_OFFSET         0x00000B70
#define ZYNQ7000_SLCR_DDRIOB_DCI_STATUS_OFFSET       0x00000B74


/* SLCR Write Protection Lock register */

#define ZYNQ7000_SLCR_LOCK_KEY 0x767B

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            lock_key : 16,
            reserved : 16;
    } __attribute__((packed)) bits;
} zynq7000_slcr_lock_reg_t;
#endif /* __ASSEMBLER__ */


/* SLCR Write Protection Unlock register */

#define ZYNQ7000_SLCR_UNLOCK_KEY 0xDF0D

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            unlock_key : 16,
            reserved   : 16;
    } __attribute__((packed)) bits;
} zynq7000_slcr_unlock_reg_t;
#endif /* __ASSEMBLER__ */


/* SLCR Write Protection Status register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            lock_status : 1,
            reserved    : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_locksta_reg_t;
#endif /* __ASSEMBLER__ */


/* PLL Control registers */

#ifndef __ASSEMBLER__
#define ZYNQ7000_SLCR_PLL_CTRL_FDIV_MASK __BITS32(18, 12)

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            pll_reset        : 1,
            pll_pwrdwn       : 1,
            reserved_0       : 1,
            pll_bypass_qual  : 1,
            pll_bypass_force : 1,
            reserved_1       : 7,
            pll_fdiv         : 7,
            reserved_2       : 13;
    } __attribute__((packed)) bits;
} zynq7000_slcr_pll_ctrl_reg_t;

typedef zynq7000_slcr_pll_ctrl_reg_t zynq7000_slcr_arm_pll_ctrl_reg_t;
typedef zynq7000_slcr_pll_ctrl_reg_t zynq7000_slcr_ddr_pll_ctrl_reg_t;
typedef zynq7000_slcr_pll_ctrl_reg_t zynq7000_slcr_io_pll_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* PLL Status register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            arm_pll_lock   : 1,
            ddr_pll_lock   : 1,
            io_pll_lock    : 1,
            arm_pll_stable : 1,
            ddr_pll_stable : 1,
            io_pll_stable  : 1,
            reserved       : 26;
    } __attribute__((packed)) bits;
} zynq7000_slcr_pll_status_reg_t;
#endif /* __ASSEMBLER__ */


/* PLL Configuration registers */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0 : 4,
            pll_res    : 4,
            pll_cp     : 4,
            lock_cnt   : 10,
            reserved_1 : 10;
    } __attribute__((packed)) bits;
} zynq7000_slcr_pll_cfg_reg_t;

typedef zynq7000_slcr_pll_cfg_reg_t zynq7000_slcr_arm_pll_cfg_reg_t;
typedef zynq7000_slcr_pll_cfg_reg_t zynq7000_slcr_ddr_pll_cfg_reg_t;
typedef zynq7000_slcr_pll_cfg_reg_t zynq7000_slcr_io_pll_cfg_reg_t;
#endif /* __ASSEMBLER__ */


/* CPU Clock Control register */

#define ZYNQ7000_SLCR_ARM_CLK_CTRL_SRCSEL_ARM_PLL  0x0
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_SRCSEL_ARM1_PLL 0x1
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_SRCSEL_DDR_PLL  0x2
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_SRCSEL_IO_PLL   0x3

#ifndef __ASSEMBLER__
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_SRCSEL_MASK    __BITS32(5, 4)
#define ZYNQ7000_SLCR_ARM_CLK_CTRL_DIVISOR_MASK   __BITS32(13, 8)

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0    : 4,
            srcsel        : 2,
            reserved_1    : 2,
            divisor       : 6,
            reserved_2    : 10,
            cpu_6or4x_act : 1,
            cpu_3or2x_act : 1,
            cpu_2x_act    : 1,
            cpu_1x_act    : 1,
            cpu_peri_act  : 1,
            reserved_3    : 3;
    } __attribute__((packed)) bits;
} zynq7000_slcr_arm_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* DDR Clock Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ddr_3x_act     : 1,
            ddr_2x_act     : 1,
            reserved       : 18,
            ddr_3x_divisor : 6,
            ddr_2x_divisor : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_ddr_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* DCI Clock Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 7,
            divisor0   : 6,
            reserved_1 : 6,
            divisor1   : 6,
            reserved_2 : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_dci_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* AMBA Peripheral Clock Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dma_cpu2x_act   : 1,
            reserved_0      : 1,
            usb0_cpu1x_act  : 1,
            usb1_cpu1x_act  : 1,
            reserved_1      : 2,
            gem0_cpu1x_act  : 1,
            gem1_cpu1x_act  : 1,
            reserved_2      : 2,
            sdi0_cpu1x_act  : 1,
            sdi1_cpu1x_act  : 1,
            reserved_3      : 2,
            spi0_cpu1x_act  : 1,
            spi1_cpu1x_act  : 1,
            can0_cpu1x_act  : 1,
            can1_cpu1x_act  : 1,
            i2c0_cpu1x_act  : 1,
            i2c1_cpu1x_act  : 1,
            uart0_cpu1x_act : 1,
            uart1_cpu1x_act : 1,
            gpio_cpu1x_act  : 1,
            lqspi_cpu1x_act : 1,
            smc_cpu1x_act   : 1,
            reserved_4      : 7;
    } __attribute__((packed)) bits;
} zynq7000_slcr_aper_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* USB ULPI Clock Control registers */

#define ZYNQ7000_SLCR_USB_CLK_CTRL_SRCSEL_MIO_ULPI 0x4

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0 : 1, /* do not modify */
            reserved_1 : 3,
            srcsel     : 3,
            reserved_2 : 1,
            reserved_3 : 6, /* do not modify */
            reserved_4 : 6,
            reserved_5 : 6, /* do not modify */
            reserved_6 : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_usb_clk_ctrl_reg_t;

typedef zynq7000_slcr_usb_clk_ctrl_reg_t zynq7000_slcr_usb0_clk_ctrl_reg_t;
typedef zynq7000_slcr_usb_clk_ctrl_reg_t zynq7000_slcr_usb1_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* GigE Rx Clock and Rx Signals Select registers */

#define ZYNQ7000_SLCR_GEM_RCLK_CTRL_SRCSEL_MIO  0x0
#define ZYNQ7000_SLCR_GEM_RCLK_CTRL_SRCSEL_EMIO 0x1

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 3,
            srcsel     : 1,
            reserved_1 : 27;
    } __attribute__((packed)) bits;
} zynq7000_slcr_gem_rclk_ctrl_reg_t;

typedef zynq7000_slcr_gem_rclk_ctrl_reg_t zynq7000_slcr_gem0_rclk_ctrl_reg_t;
typedef zynq7000_slcr_gem_rclk_ctrl_reg_t zynq7000_slcr_gem1_rclk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* GigE Ref Clock Control register */

#define ZYNQ7000_SLCR_GEM_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_GEM_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_GEM_CLK_CTRL_SRCSEL_DDR_PLL 0x3
#define ZYNQ7000_SLCR_GEM_CLK_CTRL_SRCSEL_EMIO    0x4

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 3,
            srcsel     : 3,
            reserved_1 : 1,
            divisor    : 6,
            reserved_2 : 6,
            divisor1   : 6,
            reserved_3 : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_gem_clk_ctrl_reg_t;

typedef zynq7000_slcr_gem_clk_ctrl_reg_t zynq7000_slcr_gem0_clk_ctrl_reg_t;
typedef zynq7000_slcr_gem_clk_ctrl_reg_t zynq7000_slcr_gem1_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SMC Ref Clock Control register */

#define ZYNQ7000_SLCR_SMC_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_SMC_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_SMC_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 3,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_smc_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Quad SPI Ref Clock Control register */

#define ZYNQ7000_SLCR_LQSPI_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_LQSPI_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_LQSPI_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 3,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_lqspi_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SDIO Ref Clock Control register */

#define ZYNQ7000_SLCR_SDIO_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_SDIO_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_SDIO_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact0    : 1,
            clkact1    : 1,
            reserved_0 : 2,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_sdio_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* UART Ref Clock Control register */

#define ZYNQ7000_SLCR_UART_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_UART_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_UART_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact0    : 1,
            clkact1    : 1,
            reserved_0 : 2,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_uart_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SPI Ref Clock Control register */

#define ZYNQ7000_SLCR_SPI_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_SPI_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_SPI_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact0    : 1,
            clkact1    : 1,
            reserved_0 : 2,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_spi_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* CAN Ref Clock Control register */

#define ZYNQ7000_SLCR_CAN_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_CAN_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_CAN_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact0    : 1,
            clkact1    : 1,
            reserved_0 : 2,
            srcsel     : 2,
            reserved_1 : 2,
            divisor0   : 6,
            reserved_2 : 6,
            divisor1   : 6,
            reserved_3 : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_can_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* CAN MIO Clock Control register */

#define ZYNQ7000_SLCR_CAN_MIOCLK_CTRL_REF_SEL_PLL 0x0
#define ZYNQ7000_SLCR_CAN_MIOCLK_CTRL_REF_SEL_MIO 0x1

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            can0_mux     : 6,
            can0_ref_sel : 1,
            reserved_0   : 9,
            can1_mux     : 6,
            can1_ref_sel : 1,
            reserved_1   : 9;
    } __attribute__((packed)) bits;
} zynq7000_slcr_can_mioclk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SoC Debug Clock Control register */

#define ZYNQ7000_SLCR_DBG_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_DBG_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_DBG_CLK_CTRL_SRCSEL_DDR_PLL 0x3
#define ZYNQ7000_SLCR_DBG_CLK_CTRL_SRCSEL_EMIO    0x4

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact_trc : 1,
            cpu_1x_act : 1,
            reserved_0 : 2,
            srcsel     : 3,
            reserved_1 : 1,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_dbg_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* PCAP Clock Control register */

#define ZYNQ7000_SLCR_PCAP_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_PCAP_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_PCAP_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clkact     : 1,
            reserved_0 : 3,
            srcsel     : 2,
            reserved_1 : 2,
            divisor    : 6,
            reserved_2 : 18;
    } __attribute__((packed)) bits;
} zynq7000_slcr_pcap_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Central Interconnect Clock Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clk_dis  : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_topsw_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* PL Clock Output Control registers */

#define ZYNQ7000_SLCR_FPGA_CLK_CTRL_SRCSEL_IO_PLL  0x0
#define ZYNQ7000_SLCR_FPGA_CLK_CTRL_SRCSEL_ARM_PLL 0x2
#define ZYNQ7000_SLCR_FPGA_CLK_CTRL_SRCSEL_DDR_PLL 0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0 : 4,
            srcsel     : 2,
            reserved_1 : 2,
            divisor0   : 6,
            reserved_2 : 6,
            divisor1   : 6,
            reserved_3 : 6;
    } __attribute__((packed)) bits;
} zynq7000_slcr_fpga_clk_ctrl_reg_t;

typedef zynq7000_slcr_fpga_clk_ctrl_reg_t zynq7000_slcr_fpga0_clk_ctrl_reg_t;
typedef zynq7000_slcr_fpga_clk_ctrl_reg_t zynq7000_slcr_fpga1_clk_ctrl_reg_t;
typedef zynq7000_slcr_fpga_clk_ctrl_reg_t zynq7000_slcr_fpga2_clk_ctrl_reg_t;
typedef zynq7000_slcr_fpga_clk_ctrl_reg_t zynq7000_slcr_fpga3_clk_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* CPU Clock Ratio Mode Select register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clk_621_true : 1,
            reserved     : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_clk_621_true_reg_t;
#endif /* __ASSEMBLER__ */


/* PS Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            soft_rst : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_pss_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* DDR Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ddr_rst  : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_ddr_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Central Interconnect Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            topsw_rst : 1,
            reserved  : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_topsw_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* DMAC Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dmac_rst : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_dmac_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* USB Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            usb0_cpu1x_rst : 1,
            usb1_cpu1x_rst : 1,
            reserved       : 30;
    } __attribute__((packed)) bits;
} zynq7000_slcr_usb_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Gigabit Ethernet SW Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            gem0_cpu1x_rst : 1,
            gem1_cpu1x_rst : 1,
            reserved_0     : 2,
            gem0_rx_rst    : 1,
            gem1_rx_rst    : 1,
            gem0_ref_rst   : 1,
            gem1_ref_rst   : 1,
            reserved_1     : 24;
    } __attribute__((packed)) bits;
} zynq7000_slcr_gem_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SDIO Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            sdio0_cpu1x_rst : 1,
            sdio1_cpu1x_rst : 1,
            reserved_0      : 2,
            sdio0_ref_rst   : 1,
            sdio1_ref_rst   : 1,
            reserved_1      : 26;
    } __attribute__((packed)) bits;
} zynq7000_slcr_sdio_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SPI Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            spi0_cpu1x_rst : 1,
            spi1_cpu1x_rst : 1,
            spi0_ref_rst   : 1,
            spi1_ref_rst   : 1,
            reserved       : 28;
    } __attribute__((packed)) bits;
} zynq7000_slcr_spi_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* CAN Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            can0_cpu1x_rst : 1,
            can1_cpu1x_rst : 1,
            reserved_0     : 2, /* writes will maintain value */
            reserved_1     : 28;
    } __attribute__((packed)) bits;
} zynq7000_slcr_can_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* I2C Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            i2c0_cpu1x_rst : 1,
            i2c1_cpu1x_rst : 1,
            reserved       : 30;
    } __attribute__((packed)) bits;
} zynq7000_slcr_i2c_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* UART Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            uart0_cpu1x_rst : 1,
            uart1_cpu1x_rst : 1,
            uart0_ref_rst   : 1,
            uart1_ref_rst   : 1,
            reserved        : 28;
    } __attribute__((packed)) bits;
} zynq7000_slcr_uart_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* GPIO Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            gpio_cpu1x_rst : 1,
            reserved       : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_gpio_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Quad SPI Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            lqspi_cpu1x_rst : 1,
            qspi_ref_rst    : 1,
            reserved        : 30;
    } __attribute__((packed)) bits;
} zynq7000_slcr_lqspi_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* SMC Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            smc_cpu1x_rst : 1,
            smc_ref_rst   : 1,
            reserved      : 30;
    } __attribute__((packed)) bits;
} zynq7000_slcr_smc_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* OCM Software Reset Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ocm_rst  : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_slcr_ocm_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* FPGA Software Reset Control register */

#define ZYNQ7000_SLCR_RST_CTRL_ALL_ASSERT   0xF
#define ZYNQ7000_SLCR_RST_CTRL_ALL_DEASSERT 0x0

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            fpga0_out_rst : 1,
            fpga1_out_rst : 1,
            fpga2_out_rst : 1,
            fpga3_out_rst : 1,
            reserved_0    : 4,
            reserved_1    : 6, /* always write with 0 */
            reserved_2    : 2,
            reserved_3    : 2, /* always write with 0 */
            reserved_4    : 2,
            reserved_5    : 5, /* always write with 0 */
            reserved_6    : 7;
    } __attribute__((packed)) bits;
} zynq7000_slcr_fpga_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* CPU Reset and Clock Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            a9_rst0     : 1,
            a9_rst1     : 1,
            reserved_0  : 2,
            a9_clkstop0 : 1,
            a9_clkstop1 : 1,
            reserved_1  : 2,
            peri_rst    : 1,
            reserved_2  : 23;
    } __attribute__((packed)) bits;
} zynq7000_slcr_a9_cpu_rst_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Boot Mode Strapping Pins register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            boot_mode  : 4,
            pll_bypass : 1,
            reserved   : 27;
    } __attribute__((packed)) bits;
} zynq7000_slcr_boot_mode_reg_t;
#endif /* __ASSEMBLER__ */


/* Level Shifters Enable register */

#define ZYNQ7000_SLCR_LVL_SHFTR_EN_DISABLE 0x0
#define ZYNQ7000_SLCR_LVL_SHFTR_EN_PS_PL   0xA
#define ZYNQ7000_SLCR_LVL_SHFTR_EN_ALL     0xF

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            user_lvl_shftr_en : 4,
            reserved_0        : 1, /* do not modify */
            reserved_1        : 27;
    } __attribute__((packed)) bits;
} zynq7000_slcr_lvl_shftr_en_reg_t;
#endif /* __ASSEMBLER__ */


/* SLCR registers map */

#ifndef __ASSEMBLER__
typedef struct {
    uint32_t scl;
    zynq7000_slcr_lock_reg_t slcr_lock;
    zynq7000_slcr_unlock_reg_t slcr_unlock;
    zynq7000_slcr_locksta_reg_t slcr_locksta;
    uint8_t padding_1[0xF0];
    zynq7000_slcr_arm_pll_ctrl_reg_t arm_pll_ctrl;
    zynq7000_slcr_ddr_pll_ctrl_reg_t ddr_pll_ctrl;
    zynq7000_slcr_io_pll_ctrl_reg_t io_pll_ctrl;
    zynq7000_slcr_pll_status_reg_t pll_status;
    zynq7000_slcr_arm_pll_cfg_reg_t arm_pll_cfg;
    zynq7000_slcr_ddr_pll_cfg_reg_t ddr_pll_cfg;
    zynq7000_slcr_io_pll_cfg_reg_t io_pll_cfg;
    uint8_t padding_2[0x4];
    zynq7000_slcr_arm_clk_ctrl_reg_t arm_clk_ctrl;
    zynq7000_slcr_ddr_clk_ctrl_reg_t ddr_clk_ctrl;
    zynq7000_slcr_dci_clk_ctrl_reg_t dci_clk_ctrl;
    zynq7000_slcr_aper_clk_ctrl_reg_t aper_clk_ctrl;
    zynq7000_slcr_usb_clk_ctrl_reg_t usb_clk_ctrl[2];
    zynq7000_slcr_gem_rclk_ctrl_reg_t gem_rclk_ctrl[2];
    zynq7000_slcr_gem_clk_ctrl_reg_t gem_clk_ctrl[2];
    zynq7000_slcr_smc_clk_ctrl_reg_t smc_clk_ctrl;
    zynq7000_slcr_lqspi_clk_ctrl_reg_t lqspi_clk_ctrl;
    zynq7000_slcr_sdio_clk_ctrl_reg_t sdio_clk_ctrl;
    zynq7000_slcr_uart_clk_ctrl_reg_t uart_clk_ctrl;
    zynq7000_slcr_spi_clk_ctrl_reg_t spi_clk_ctrl;
    zynq7000_slcr_can_clk_ctrl_reg_t can_clk_ctrl;
    zynq7000_slcr_can_mioclk_ctrl_reg_t can_mioclk_ctrl;
    zynq7000_slcr_dbg_clk_ctrl_reg_t dbg_clk_ctrl;
    zynq7000_slcr_pcap_clk_ctrl_reg_t pcap_clk_ctrl;
    zynq7000_slcr_topsw_clk_ctrl_reg_t topsw_clk_ctrl;
    struct {
        zynq7000_slcr_fpga_clk_ctrl_reg_t clk_ctrl;
        uint32_t thr_ctrl;
        uint32_t thr_cnt;
        uint32_t thr_sta;
    } fpga[4];
    uint8_t padding_3[0x14];
    zynq7000_slcr_clk_621_true_reg_t clk_621_true;
    uint8_t padding_4[0x38];
    zynq7000_slcr_pss_rst_ctrl_reg_t pss_rst_ctrl;
    zynq7000_slcr_ddr_rst_ctrl_reg_t ddr_rst_ctrl;
    zynq7000_slcr_topsw_rst_ctrl_reg_t topsw_rst_ctrl;
    zynq7000_slcr_dmac_rst_ctrl_reg_t dmac_rst_ctrl;
    zynq7000_slcr_usb_rst_ctrl_reg_t usb_rst_ctrl;
    zynq7000_slcr_gem_rst_ctrl_reg_t gem_rst_ctrl;
    zynq7000_slcr_sdio_rst_ctrl_reg_t sdio_rst_ctrl;
    zynq7000_slcr_spi_rst_ctrl_reg_t spi_rst_ctrl;
    zynq7000_slcr_can_rst_ctrl_reg_t can_rst_ctrl;
    zynq7000_slcr_i2c_rst_ctrl_reg_t i2c_rst_ctrl;
    zynq7000_slcr_uart_rst_ctrl_reg_t uart_rst_ctrl;
    zynq7000_slcr_gpio_rst_ctrl_reg_t gpio_rst_ctrl;
    zynq7000_slcr_lqspi_rst_ctrl_reg_t lqspi_rst_ctrl;
    zynq7000_slcr_smc_rst_ctrl_reg_t smc_rst_ctrl;
    zynq7000_slcr_ocm_rst_ctrl_reg_t ocm_rst_ctrl;
    uint8_t padding_5[0x4];
    zynq7000_slcr_fpga_rst_ctrl_reg_t fpga_rst_ctrl;
    zynq7000_slcr_a9_cpu_rst_ctrl_reg_t a9_cpu_rst_ctrl;
    uint8_t padding_6[0x4];
    uint32_t rs_awdt_ctrl;
    uint8_t padding_7[0x8];
    uint32_t reboot_status;
    zynq7000_slcr_boot_mode_reg_t boot_mode;
    uint8_t padding_8[0xA0];
    uint32_t apu_ctrl;
    uint32_t wdt_clk_sel;
    uint8_t padding_9[0x138];
    uint32_t tz_dma_ns;
    uint32_t tz_dma_irq_ns;
    uint32_t tz_dma_periph_ns;
    uint8_t padding_11[0xE4];
    uint32_t pss_idcode;
    uint8_t padding_12[0xCC];
    uint32_t ddr_urgent;
    uint8_t padding_13[0x8];
    uint32_t ddr_cal_start;
    uint8_t padding_14[0x4];
    uint32_t ddr_ref_start;
    uint32_t ddr_cmd_sta;
    uint32_t ddr_urgent_sel;
    uint32_t ddr_dfi_status;
    uint8_t padding_15[0xDC];
    uint32_t mio_pin[54];
    uint8_t padding_16[0x2C];
    uint32_t mio_loopback;
    uint8_t padding_17[0x4];
    uint32_t mio_mst_tri[2];
    uint8_t padding_18[0x1C];
    struct {
        uint32_t wp_cd_sel;
    } sd[2];
    uint8_t padding_19[0xC8];
    zynq7000_slcr_lvl_shftr_en_reg_t lvl_shftr_en;
    uint8_t padding_20[0xC];
    uint32_t ocm_cfg;
    uint8_t padding_21[0x108];
    uint32_t reserved;
    uint8_t padding_22[0xE0];
    uint32_t gpiob_ctrl;
    uint32_t gpiob_cfg_cmos18;
    uint32_t gpiob_cfg_cmos25;
    uint32_t gpiob_cfg_cmos33;
    uint8_t padding_23[0x4];
    uint32_t gpiob_cfg_hstl;
    uint32_t gpiob_drvr_bias_ctrl;
    uint8_t padding_24[0x24];
    uint32_t ddriob_addr[2];
    uint32_t ddriob_data[2];
    uint32_t ddriob_diff[2];
    uint32_t ddriob_clock;
    uint32_t ddriob_drive_slew_addr;
    uint32_t ddriob_drive_slew_data;
    uint32_t ddriob_drive_slew_diff;
    uint32_t ddriob_drive_slew_clock;
    uint32_t ddriob_ddr_ctrl;
    uint32_t ddriob_dci_ctrl;
    uint32_t ddriob_dci_status;
} __attribute__((packed)) zynq7000_slcr_regs_t;

_Static_assert(sizeof(zynq7000_slcr_regs_t) == ZYNQ7000_SLCR_REG_SIZE, "SLCR size mismatch");

ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, scl,                     ZYNQ7000_SLCR_SCL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, slcr_lock,               ZYNQ7000_SLCR_LOCK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, slcr_unlock,             ZYNQ7000_SLCR_UNLOCK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, slcr_locksta,            ZYNQ7000_SLCR_LOCKSTA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, arm_pll_ctrl,            ZYNQ7000_SLCR_ARM_PLL_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_pll_ctrl,            ZYNQ7000_SLCR_DDR_PLL_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, io_pll_ctrl,             ZYNQ7000_SLCR_IO_PLL_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, pll_status,              ZYNQ7000_SLCR_PLL_STATUS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, arm_pll_cfg,             ZYNQ7000_SLCR_ARM_PLL_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_pll_cfg,             ZYNQ7000_SLCR_DDR_PLL_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, io_pll_cfg,              ZYNQ7000_SLCR_IO_PLL_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, arm_clk_ctrl,            ZYNQ7000_SLCR_ARM_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_clk_ctrl,            ZYNQ7000_SLCR_DDR_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, dci_clk_ctrl,            ZYNQ7000_SLCR_DCI_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, aper_clk_ctrl,           ZYNQ7000_SLCR_APER_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, usb_clk_ctrl[0],         ZYNQ7000_SLCR_USB0_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, usb_clk_ctrl[1],         ZYNQ7000_SLCR_USB1_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gem_rclk_ctrl[0],        ZYNQ7000_SLCR_GEM0_RCLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gem_rclk_ctrl[1],        ZYNQ7000_SLCR_GEM1_RCLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gem_clk_ctrl[0],         ZYNQ7000_SLCR_GEM0_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gem_clk_ctrl[1],         ZYNQ7000_SLCR_GEM1_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, smc_clk_ctrl,            ZYNQ7000_SLCR_SMC_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, lqspi_clk_ctrl,          ZYNQ7000_SLCR_LQSPI_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, sdio_clk_ctrl,           ZYNQ7000_SLCR_SDIO_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, uart_clk_ctrl,           ZYNQ7000_SLCR_UART_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, spi_clk_ctrl,            ZYNQ7000_SLCR_SPI_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, can_clk_ctrl,            ZYNQ7000_SLCR_CAN_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, can_mioclk_ctrl,         ZYNQ7000_SLCR_CAN_MIOCLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, dbg_clk_ctrl,            ZYNQ7000_SLCR_DBG_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, pcap_clk_ctrl,           ZYNQ7000_SLCR_PCAP_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, topsw_clk_ctrl,          ZYNQ7000_SLCR_TOPSW_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[0].clk_ctrl,        ZYNQ7000_SLCR_FPGA0_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[0].thr_ctrl,        ZYNQ7000_SLCR_FPGA0_THR_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[0].thr_cnt,         ZYNQ7000_SLCR_FPGA0_THR_CNT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[0].thr_sta,         ZYNQ7000_SLCR_FPGA0_THR_STA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[1].clk_ctrl,        ZYNQ7000_SLCR_FPGA1_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[1].thr_ctrl,        ZYNQ7000_SLCR_FPGA1_THR_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[1].thr_cnt,         ZYNQ7000_SLCR_FPGA1_THR_CNT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[1].thr_sta,         ZYNQ7000_SLCR_FPGA1_THR_STA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[2].clk_ctrl,        ZYNQ7000_SLCR_FPGA2_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[2].thr_ctrl,        ZYNQ7000_SLCR_FPGA2_THR_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[2].thr_cnt,         ZYNQ7000_SLCR_FPGA2_THR_CNT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[2].thr_sta,         ZYNQ7000_SLCR_FPGA2_THR_STA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[3].clk_ctrl,        ZYNQ7000_SLCR_FPGA3_CLK_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[3].thr_ctrl,        ZYNQ7000_SLCR_FPGA3_THR_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[3].thr_cnt,         ZYNQ7000_SLCR_FPGA3_THR_CNT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga[3].thr_sta,         ZYNQ7000_SLCR_FPGA3_THR_STA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, clk_621_true,            ZYNQ7000_SLCR_CLK_621_TRUE_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, pss_rst_ctrl,            ZYNQ7000_SLCR_PSS_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_rst_ctrl,            ZYNQ7000_SLCR_DDR_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, topsw_rst_ctrl,          ZYNQ7000_SLCR_TOPSW_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, dmac_rst_ctrl,           ZYNQ7000_SLCR_DMAC_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, usb_rst_ctrl,            ZYNQ7000_SLCR_USB_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gem_rst_ctrl,            ZYNQ7000_SLCR_GEM_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, sdio_rst_ctrl,           ZYNQ7000_SLCR_SDIO_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, spi_rst_ctrl,            ZYNQ7000_SLCR_SPI_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, can_rst_ctrl,            ZYNQ7000_SLCR_CAN_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, i2c_rst_ctrl,            ZYNQ7000_SLCR_I2C_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, uart_rst_ctrl,           ZYNQ7000_SLCR_UART_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpio_rst_ctrl,           ZYNQ7000_SLCR_GPIO_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, lqspi_rst_ctrl,          ZYNQ7000_SLCR_LQSPI_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, smc_rst_ctrl,            ZYNQ7000_SLCR_SMC_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ocm_rst_ctrl,            ZYNQ7000_SLCR_OCM_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, fpga_rst_ctrl,           ZYNQ7000_SLCR_FPGA_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, a9_cpu_rst_ctrl,         ZYNQ7000_SLCR_A9_CPU_RST_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, rs_awdt_ctrl,            ZYNQ7000_SLCR_RS_AWDT_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, reboot_status,           ZYNQ7000_SLCR_REBOOT_STATUS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, boot_mode,               ZYNQ7000_SLCR_BOOT_MODE_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, apu_ctrl,                ZYNQ7000_SLCR_APU_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, wdt_clk_sel,             ZYNQ7000_SLCR_WDT_CLK_SEL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, tz_dma_ns,               ZYNQ7000_SLCR_TZ_DMA_NS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, tz_dma_irq_ns,           ZYNQ7000_SLCR_TZ_DMA_IRQ_NS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, tz_dma_periph_ns,        ZYNQ7000_SLCR_TZ_DMA_PERIPH_NS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, pss_idcode,              ZYNQ7000_SLCR_PSS_IDCODE_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_urgent,              ZYNQ7000_SLCR_DDR_URGENT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_cal_start,           ZYNQ7000_SLCR_DDR_CAL_START_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_ref_start,           ZYNQ7000_SLCR_DDR_REF_START_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_cmd_sta,             ZYNQ7000_SLCR_DDR_CMD_STA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_urgent_sel,          ZYNQ7000_SLCR_DDR_URGENT_SEL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddr_dfi_status,          ZYNQ7000_SLCR_DDR_DFI_STATUS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, mio_pin[0],              ZYNQ7000_SLCR_MIO_PIN_OFFSET(0));
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, mio_pin[53],             ZYNQ7000_SLCR_MIO_PIN_OFFSET(53));
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, mio_loopback,            ZYNQ7000_SLCR_MIO_LOOPBACK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, mio_mst_tri[0],          ZYNQ7000_SLCR_MIO_MST_TRI0_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, mio_mst_tri[1],          ZYNQ7000_SLCR_MIO_MST_TRI1_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, sd[0].wp_cd_sel,         ZYNQ7000_SLCR_SD0_WP_CD_SEL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, sd[1].wp_cd_sel,         ZYNQ7000_SLCR_SD1_WP_CD_SEL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, lvl_shftr_en,            ZYNQ7000_SLCR_LVL_SHFTR_EN_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ocm_cfg,                 ZYNQ7000_SLCR_OCM_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_ctrl,              ZYNQ7000_SLCR_GPIOB_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_cfg_cmos18,        ZYNQ7000_SLCR_GPIOB_CFG_CMOS18_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_cfg_cmos25,        ZYNQ7000_SLCR_GPIOB_CFG_CMOS25_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_cfg_cmos33,        ZYNQ7000_SLCR_GPIOB_CFG_CMOS33_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_cfg_hstl,          ZYNQ7000_SLCR_GPIOB_CFG_HSTL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, gpiob_drvr_bias_ctrl,    ZYNQ7000_SLCR_GPIOB_DRVR_BIAS_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_addr[0],          ZYNQ7000_SLCR_DDRIOB_ADDR0_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_addr[1],          ZYNQ7000_SLCR_DDRIOB_ADDR1_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_data[0],          ZYNQ7000_SLCR_DDRIOB_DATA0_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_data[1],          ZYNQ7000_SLCR_DDRIOB_DATA1_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_diff[0],          ZYNQ7000_SLCR_DDRIOB_DIFF0_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_diff[1],          ZYNQ7000_SLCR_DDRIOB_DIFF1_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_clock,            ZYNQ7000_SLCR_DDRIOB_CLOCK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_drive_slew_addr,  ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_ADDR_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_drive_slew_data,  ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_DATA_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_drive_slew_diff,  ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_DIFF_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_drive_slew_clock, ZYNQ7000_SLCR_DDRIOB_DRIVE_SLEW_CLOCK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_ddr_ctrl,         ZYNQ7000_SLCR_DDRIOB_DDR_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_dci_ctrl,         ZYNQ7000_SLCR_DDRIOB_DCI_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_slcr_regs_t, ddriob_dci_status,       ZYNQ7000_SLCR_DDRIOB_DCI_STATUS_OFFSET);
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * UART
 * ----------------------------------------------------------------------------
 */

/* UART base addresses */

#define ZYNQ7000_XUARTPS_UART0_BASE 0xE0000000
#define ZYNQ7000_XUARTPS_UART1_BASE 0xE0001000


/* UART registers */

#ifndef __ASSEMBLER__
typedef xzynq_xuartps_cr_reg_t zynq7000_xuartps_cr_reg_t;
typedef xzynq_xuartps_mr_reg_t zynq7000_xuartps_mr_reg_t;
typedef xzynq_xuartps_ier_reg_t zynq7000_xuartps_ier_reg_t;
typedef xzynq_xuartps_idr_reg_t zynq7000_xuartps_idr_reg_t;
typedef xzynq_xuartps_imr_reg_t zynq7000_xuartps_imr_reg_t;
typedef xzynq_xuartps_isr_reg_t zynq7000_xuartps_isr_reg_t;
typedef xzynq_xuartps_baudgen_reg_t zynq7000_xuartps_baudgen_reg_t;
typedef xzynq_xuartps_rxtout_reg_t zynq7000_xuartps_rxtout_reg_t;
typedef xzynq_xuartps_rxwm_reg_t zynq7000_xuartps_rxwm_reg_t;
typedef xzynq_xuartps_modemcr_reg_t zynq7000_xuartps_modemcr_reg_t;
typedef xzynq_xuartps_modemsr_reg_t zynq7000_xuartps_modemsr_reg_t;
typedef xzynq_xuartps_sr_reg_t zynq7000_xuartps_sr_reg_t;
typedef xzynq_xuartps_fifo_reg_t zynq7000_xuartps_fifo_reg_t;
typedef xzynq_xuartps_bauddiv_reg_t zynq7000_xuartps_bauddiv_reg_t;
typedef xzynq_xuartps_flowdel_reg_t zynq7000_xuartps_flowdel_reg_t;
typedef xzynq_xuartps_txwm_reg_t zynq7000_xuartps_txwm_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * SDRAM
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_SDRAM_64_BIT_BASE 0x00100000


/* ----------------------------------------------------------------------------
 * CAN
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_CAN0_BASEADDR 0xE0008000
#define ZYNQ7000_CAN1_BASEADDR 0xE0009000

/* ----------------------------------------------------------------------------
 * Gigabit Ethernet Controller (GEM)
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_XEMACPS0_BASE_ADDR 0xE000B000
#define ZYNQ7000_XEMACPS1_BASE_ADDR 0xE000C000

#define ZYNQ7000_XEMACPS_REG_SIZE 0x294


/* GEM registers offsets */

#define ZYNQ7000_XEMACPS_NWCTRL_OFFSET 0x000


/* Network Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0              : 1, /* do not modify */
            loopen                  : 1,
            rxen                    : 1,
            txen                    : 1,
            mden                    : 1,
            statclr                 : 1,
            statinc                 : 1,
            statwen                 : 1,
            back_pressure           : 1,
            starttx                 : 1,
            halttx                  : 1,
            pausetx                 : 1,
            zeropausetx             : 1,
            reserved_1              : 3, /* do not modify */
            en_pfc_pri_pause_rx     : 1,
            tx_pfc_pri_pause_frame  : 1,
            flush_next_rx_dpram_pkt : 1,
            reserved_2              : 13;
    } __attribute__((packed)) bits;
} zynq7000_xemacps_nwctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * DMA
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_DMA_NS_BASE_ADDR 0xF8004000
#define ZYNQ7000_DMA_S_BASE_ADDR  0xF8003000
#define ZYNQ7000_DMA_REG_SIZE     0x1000


/* ----------------------------------------------------------------------------
 * I2C
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_XIICPS1_BASE_ADDR 0xE0004000
#define ZYNQ7000_XIICPS2_BASE_ADDR 0xE0005000
#define ZYNQ7000_XIICPS_REG_SIZE   0x2C


/* I2C registers */

#ifndef __ASSEMBLER__
typedef xzynq_xiicps_cr_reg_t zynq7000_xiicps_cr_reg_t;
typedef xzynq_xiicps_sr_reg_t zynq7000_xiicps_sr_reg_t;
typedef xzynq_xiicps_addr_reg_t zynq7000_xiicps_addr_reg_t;
typedef xzynq_xiicps_data_reg_t zynq7000_xiicps_data_reg_t;
typedef xzynq_xiicps_isr_reg_t zynq7000_xiicps_isr_reg_t;
typedef xzynq_xiicps_imr_reg_t zynq7000_xiicps_imr_reg_t;
typedef xzynq_xiicps_ier_reg_t zynq7000_xiicps_ier_reg_t;
typedef xzynq_xiicps_idr_reg_t zynq7000_xiicps_idr_reg_t;
typedef xzynq_xiicps_trans_size_reg_t zynq7000_xiicps_trans_size_reg_t;
typedef xzynq_xiicps_slv_pause_reg_t zynq7000_xiicps_slv_pause_reg_t;
typedef xzynq_xiicps_time_out_reg_t zynq7000_xiicps_time_out_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * SPI
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_XSPIPS0_BASE_ADDR 0xE0006000
#define ZYNQ7000_XSPIPS1_BASE_ADDR 0xE0007000
#define ZYNQ7000_XSPIPS_REG_SIZE   0x100

#define ZYNQ7000_XSPIPS0_IRQ 58
#define ZYNQ7000_XSPIPS1_IRQ 81


/* SPI registers */

#ifndef __ASSEMBLER__
typedef xzynq_xspips_cr_reg_t zynq7000_xspips_cr_reg_t;
typedef xzynq_xspips_isr_reg_t zynq7000_xspips_isr_reg_t;
typedef xzynq_xspips_ier_reg_t zynq7000_xspips_ier_reg_t;
typedef xzynq_xspips_idr_reg_t zynq7000_xspips_idr_reg_t;
typedef xzynq_xspips_imr_reg_t zynq7000_xspips_imr_reg_t;
typedef xzynq_xspips_en_reg_t zynq7000_xspips_en_reg_t;
typedef xzynq_xspips_dly_reg_t zynq7000_xspips_dly_reg_t;
typedef xzynq_xspips_txd_reg_t zynq7000_xspips_txd_reg_t;
typedef xzynq_xspips_rxd_reg_t zynq7000_xspips_rxd_reg_t;
typedef xzynq_xspips_sicr_reg_t zynq7000_xspips_sicr_reg_t;
typedef xzynq_xspips_txwr_reg_t zynq7000_xspips_txwr_reg_t;
typedef xzynq_xspips_rxwr_reg_t zynq7000_xspips_rxwr_reg_t;
typedef xzynq_xspips_mod_id_reg_t zynq7000_xspips_mod_id_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * GPIO
 * ----------------------------------------------------------------------------
 */

/* Register offsets for the GPIO. Each register is 32 bits. */
#define ZYNQ7000_GPIOPS_DATA_LSW_OFFSET  0x000  /* Mask and Data Register LSW, WO */
#define ZYNQ7000_GPIOPS_DATA_MSW_OFFSET  0x004  /* Mask and Data Register MSW, WO */
#define ZYNQ7000_GPIOPS_DATA_OFFSET      0x040  /* Data Register, RW */
#define ZYNQ7000_GPIOPS_DATA_RO_OFFSET   0x060  /* Data Register, RO */
#define ZYNQ7000_GPIOPS_DIRM_OFFSET      0x204  /* Direction Mode Register, RW */
#define ZYNQ7000_GPIOPS_OUTEN_OFFSET     0x208  /* Output Enable Register, RW */
#define ZYNQ7000_GPIOPS_INTMASK_OFFSET   0x20C  /* Interrupt Mask Register, RO */
#define ZYNQ7000_GPIOPS_INTEN_OFFSET     0x210  /* Interrupt Enable Register, WO */
#define ZYNQ7000_GPIOPS_INTDIS_OFFSET    0x214  /* Interrupt Disable Register, WO */
#define ZYNQ7000_GPIOPS_INTSTS_OFFSET    0x218  /* Interrupt Status Register, RO */
#define ZYNQ7000_GPIOPS_INTTYPE_OFFSET   0x21C  /* Interrupt Type Register, RW */
#define ZYNQ7000_GPIOPS_INTPOL_OFFSET    0x220  /* Interrupt Polarity Register, RW */
#define ZYNQ7000_GPIOPS_INTANY_OFFSET    0x224  /* Interrupt On Any Register, RW */

#define ZYNQ7000_GPIO_BASE               0xE000A000
#define ZYNQ7000_GPIO_SIZE               0x300
#define ZYNQ7000_GPIOPS_DATA_BANK_OFFSET 0x4
#define ZYNQ7000_GPIOPS_REG_BANK_OFFSET  0x40

#define ZYNQ7000_GPIOPS_REG_BYTE_SIZE    4


/* ----------------------------------------------------------------------------
 * ARM / SMP
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_CPU1_START_ADDRESS 0xFFFFFFF0
#define ZYNQ7000_PL310_BASE         0xF8F02000


/* ----------------------------------------------------------------------------
 * CPU Private timers
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_PRIVATE_TIMER_BASEADDR 0xF8F00600
#define ZYNQ7000_PRIVATE_TIMER_SIZE     0x10

/* Timer Load register */
#define ZYNQ7000_PRIVATE_TIMER_LOAD 0x00

/* Timer Counter register */
#define ZYNQ7000_PRIVATE_TIMER_COUNTER 0x04

/* Timer Control register */
#define ZYNQ7000_PRIVATE_TIMER_CTRL               0x08
#define ZYNQ7000_PRIVATE_TIMER_CTRL_EN            (1 << 0)
#define ZYNQ7000_PRIVATE_TIMER_CTRL_AUTO_RELOAD   (1 << 1)
#define ZYNQ7000_PRIVATE_TIMER_CTRL_IRQ           (1 << 2)
#define ZYNQ7000_PRIVATE_TIMER_CTRL_PRESCALER_MSK (0xFF << 8)

/* Timer Interrupt Status register */
#define ZYNQ7000_PRIVATE_TIMER_INT_STS 0x0C


/* ----------------------------------------------------------------------------
 * MPCORE
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_MPCORE_BASE              0xF8F00000
#define ZYNQ7000_MPCORE_LEN               0x00002000
#define ZYNQ7000_MPCORE_FILTER_START_ADDR 0x00000040
#define ZYNQ7000_MPCORE_FILTER_END_ADDR   0x00000044
#define ZYNQ7000_MPCORE_FILTER_ADDR_MSK   0xFFF00000


/* ----------------------------------------------------------------------------
 * Global timer
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_GLOBAL_TIMER_COUNTER_BASE 0xF8F00200

/* Size of the register area (used for mapping) */
#define ZYNQ7000_GLOBAL_TIMER_COUNTER_BASE_MAP_SIZE 0x610

#define ZYNQ7000_GLOBAL_TIMER_COUNTER_REG0 0x00
#define ZYNQ7000_GLOBAL_TIMER_COUNTER_REG1 0x04
#define ZYNQ7000_GLOBAL_TIMER_CONTROL_REG  0x08
#define ZYNQ7000_GLOBAL_TIMER_INT_STS      0x08
#define ZYNQ7000_GLOBAL_TIMER_CMP_REG0     0x10
#define ZYNQ7000_GLOBAL_TIMER_CMP_REG1     0x14

#define ZYNQ7000_GLOBAL_TIMER_CONTROL_TIMER_EN      (1 << 0)
#define ZYNQ7000_GLOBAL_TIMER_CONTROL_CMP_EN        (1 << 1)
#define ZYNQ7000_GLOBAL_TIMER_CONTROL_IRQ           (1 << 2)
#define ZYNQ7000_GLOBAL_TIMER_CONTROL_AE            (1 << 3)
#define ZYNQ7000_GLOBAL_TIMER_CONTROL_PRESCALER_MSK (0xFF << 8)


/* ----------------------------------------------------------------------------
 * TTC timers
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_TTC0_BASE(timer) (unsigned int)(0xF8001000 + 4 * timer)
#define ZYNQ7000_TTC1_BASE(timer) (unsigned int)(0xF8002000 + 4 * timer)
#define ZYNQ7000_TTC_MAP_SIZE     0x84

/* Address used in the timer callout
 * Make sure it is consistent with the C code (cannot use the C definition
 * in assembly code).
 */
#define ZYNQ7000_TTC0_BASE_CALLOUT 0xF8001000


#define ZYNQ7000_TTC_CLOCK_CTRL   0x00
#define ZYNQ7000_TTC_COUNTER_CTRL 0x0C
#define ZYNQ7000_TTC_COUNTER_VAL  0x18
#define ZYNQ7000_TTC_INTR_VAL     0x24
#define ZYNQ7000_TTC_MATCH_1      0x30
#define ZYNQ7000_TTC_MATCH_2      0x3C
#define ZYNQ7000_TTC_MATCH_3      0x48
#define ZYNQ7000_TTC_ISR          0x54
#define ZYNQ7000_TTC_IER          0x60

#define ZYNQ7000_TTC_COUNTER_CTRL_DIS      (1 << 0)
#define ZYNQ7000_TTC_COUNTER_CTRL_INT      (1 << 1)
#define ZYNQ7000_TTC_COUNTER_CTRL_DEC      (1 << 2)
#define ZYNQ7000_TTC_COUNTER_CTRL_MATCH    (1 << 3)
#define ZYNQ7000_TTC_COUNTER_CTRL_RST      (1 << 4)
#define ZYNQ7000_TTC_COUNTER_CTRL_EN_WAVE  (1 << 5)
#define ZYNQ7000_TTC_COUNTER_CTRL_POL_WAVE (1 << 6)

#define ZYNQ7000_TTC_CLOCK_CTRL_PS_EN (1 << 0)

#define ZYNQ7000_TTC_IER_IXR_INTERVAL (1 << 0)
#define ZYNQ7000_TTC_IER_IXR_MATCH_0  (1 << 1)
#define ZYNQ7000_TTC_IER_IXR_MATCH_1  (1 << 2)
#define ZYNQ7000_TTC_IER_IXR_MATCH_2  (1 << 3)
#define ZYNQ7000_TTC_IER_IXR_CNT_OVR  (1 << 4)


/* ----------------------------------------------------------------------------
 * SDIO ports
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_SDIO0_BASEADDR 0xE0100000
#define ZYNQ7000_SDIO1_BASEADDR 0xE0101000

/* ----------------------------------------------------------------------------
 * Watchdog
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_WDT_BASEADDR 0xF8005000

#ifndef __ASSEMBLER__
typedef xzynq_wdt_zmr_reg_t zynq7000_wdt_zmr_reg_t;
typedef xzynq_wdt_ccr_reg_t zynq7000_wdt_ccr_reg_t;
typedef xzynq_wdt_rst_reg_t zynq7000_wdt_rst_reg_t;
typedef xzynq_wdt_sr_reg_t zynq7000_wdt_sr_reg_t;
#endif /* __ASSEMBLER__ */

/* ----------------------------------------------------------------------------
 * QSPI
 * ----------------------------------------------------------------------------
 */

/* Registers base address */
#define ZYNQ7000_QSPI_REG_ADDRESS 0xE000D000
#define ZYNQ7000_QSPI_REG_SIZE    0x100

/* Interrupt number */
#define ZYNQ7000_QSPI_IRQ 51

/* FIFO */
#define ZYNQ7000_QSPI_FIFO_WIDTH 4
#define ZYNQ7000_QSPI_FIFO_DEPTH 63

/* QSPI registers */

#define ZYNQ7000_QSPI_CR_OFFSET       0x00 /* 32-bit Control */
#define ZYNQ7000_QSPI_ISR_OFFSET      0x04 /* Interrupt Status */
#define ZYNQ7000_QSPI_IER_OFFSET      0x08 /* Interrupt Enable */
#define ZYNQ7000_QSPI_IDR_OFFSET      0x0C /* Interrupt Disable */
#define ZYNQ7000_QSPI_IMR_OFFSET      0x10 /* Interrupt Enabled Mask */
#define ZYNQ7000_QSPI_ER_OFFSET       0x14 /* Enable/Disable Register */
#define ZYNQ7000_QSPI_DR_OFFSET       0x18 /* Delay Register */
#define ZYNQ7000_QSPI_TXD_00_OFFSET   0x1C /* Transmit 4-byte inst/data */
#define ZYNQ7000_QSPI_RXD_OFFSET      0x20 /* Data Receive Register */
#define ZYNQ7000_QSPI_SICR_OFFSET     0x24 /* Slave Idle Count */
#define ZYNQ7000_QSPI_TXWR_OFFSET     0x28 /* Transmit FIFO Watermark */
#define ZYNQ7000_QSPI_RXWR_OFFSET     0x2C /* Receive FIFO Watermark */
#define ZYNQ7000_QSPI_GPIO_OFFSET     0x30 /* GPIO Register for the QSPI */
#define ZYNQ7000_QSPI_LPBK_DLY_OFFSET 0x38 /* Loopback Master Clock Delay Adjustment Register */
#define ZYNQ7000_QSPI_TXD_01_OFFSET   0x80 /* Transmit 1-byte inst */
#define ZYNQ7000_QSPI_TXD_10_OFFSET   0x84 /* Transmit 2-byte inst */
#define ZYNQ7000_QSPI_TXD_11_OFFSET   0x88 /* Transmit 3-byte inst */
#define ZYNQ7000_QSPI_LQSPI_CR_OFFSET 0xA0 /* Linear QSPI config register */
#define ZYNQ7000_QSPI_LQSPI_SR_OFFSET 0xA4 /* Linear QSPI status register */
#define ZYNQ7000_QSPI_MOD_ID_OFFSET   0xFC /* Module Identification register */

/* QSPI Control register */

#define ZYNQ7000_QSPI_CR_FIFO_WIDTH 0x03 /* FIFO width. Must be set to 2'b11 (32bits). */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            mstren     : 1,
            cpol       : 1,
            cpha       : 1,
            baud_div   : 3,
            fifo_width : 2,
            ref_clk    : 1, /* Must be 0 */
            reserved_0 : 1,
            pcs        : 1,
            reserved_1 : 3,
            ssforce    : 1,
            manstrten  : 1,
            manstrt    : 1,
            reserved_2 : 2,
            holdb_dr   : 1,
            reserved_3 : 6,
            endian     : 1,
            reserved_4 : 4,
            ifmode     : 1;
    } __attribute__((packed)) bits;
} zynq7000_qspi_cr_reg_t;
#endif /* __ASSEMBLER__ */

/* Interrupt Registers */

#define ZYNQ7000_QSPI_ISR_ALL_MASK 0x0000007F

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ixr_rxovr    : 1,
            reserved_0   : 1,
            ixr_txow     : 1,
            ixr_txfull   : 1,
            ixr_rxnempty : 1,
            ixr_rxfull   : 1,
            ixr_txuf     : 1,
            reserved_1   : 25;
    } __attribute__((packed)) bits;
} zynq7000_qspi_ir_reg_t;

typedef zynq7000_qspi_ir_reg_t zynq7000_qspi_sr_reg_t;
typedef zynq7000_qspi_ir_reg_t zynq7000_qspi_ier_reg_t;
typedef zynq7000_qspi_ir_reg_t zynq7000_qspi_idr_reg_t;
typedef zynq7000_qspi_ir_reg_t zynq7000_qspi_imr_reg_t;
#endif /* __ASSEMBLER__ */

/* Enable Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            enable   : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} zynq7000_qspi_er_reg_t;
#endif /* __ASSEMBLER__ */

/* Delay Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            init  : 8,
            after : 8,
            btwn  : 8,
            d_nss : 8;
    } __attribute__((packed)) bits;
} zynq7000_qspi_dr_reg_t;
#endif /* __ASSEMBLER__ */

/* Transmit Data Registers */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            txd : 32;
    } __attribute__((packed)) bits;
} zynq7000_qspi_txd_reg_t;

typedef zynq7000_qspi_txd_reg_t zynq7000_qspi_txd_00_reg_t;
typedef zynq7000_qspi_txd_reg_t zynq7000_qspi_txd_01_reg_t;
typedef zynq7000_qspi_txd_reg_t zynq7000_qspi_txd_10_reg_t;
typedef zynq7000_qspi_txd_reg_t zynq7000_qspi_txd_11_reg_t;
#endif /* __ASSEMBLER__ */

/* Receive Data Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rxd : 32;
    } __attribute__((packed)) bits;
} zynq7000_qspi_rxd_reg_t;
#endif /* __ASSEMBLER__ */

/* Slave Idle Count Registers */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            mask     : 8,
            reserved : 24;
    } __attribute__((packed)) bits;
} zynq7000_qspi_sicr_reg_t;
#endif /* __ASSEMBLER__ */

/* TX FIFO Threshold Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            wr : 32;
    } __attribute__((packed)) bits;
} zynq7000_qspi_tx_wr_reg_t;
#endif /* __ASSEMBLER__ */

/* RX FIFO Threshold Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            wr : 32;
    } __attribute__((packed)) bits;
} zynq7000_qspi_rx_wr_reg_t;
#endif /* __ASSEMBLER__ */

/* General Purpose Inputs and Outputs Register for the Quad-SPI Controller core */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            wp_n     : 1,
            reserver : 31;
    } __attribute__((packed)) bits;
} zynq7000_qspi_gpio_reg_t;
#endif /* __ASSEMBLER__ */

/* Loopback Master Clock Delay Adjustment Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dly0     : 3,
            dly1     : 2,
            use_lpbk : 1,
            reserved : 26;
    } __attribute__((packed)) bits;
} zynq7000_qspi_lpbk_dly_reg_t;
#endif /* __ASSEMBLER__ */

/* Linear QSPI Configuration Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            inst       : 8,
            dummy      : 3,
            reserved_0 : 5,
            mode_bits  : 8,
            mode_on    : 1,
            mode_en    : 1,
            reserved_1 : 2,
            u_page     : 1,
            sep_bus    : 1,
            two_mem    : 1,
            linear     : 1;
    } __attribute__((packed)) bits;
} zynq7000_qspi_lqspi_cr_reg_t;
#endif /* __ASSEMBLER__ */

/* Linear QSPI Status Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0 : 1,
            wr_recvd   : 1,
            fb_recvd   : 1,
            reserved_1 : 29;
    } __attribute__((packed)) bits;
} zynq7000_qspi_lqspi_sr_reg_t;
#endif /* __ASSEMBLER__ */

/* Module Identification register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            mod_id : 32;
    } __attribute__((packed)) bits;
} zynq7000_qspi_mod_id_reg_t;
#endif /* __ASSEMBLER__ */

/* ----------------------------------------------------------------------------
 * Device Config
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_DEVCFG_BASE 0xF8007000
#define ZYNQ7000_DEVCFG_SIZE 0x11C

/* ----------------------------------------------------------------------------
 * FPGA
 * Part of the Device Config Interface. All Offsets are based on the devcfg
 * base address.
 * ----------------------------------------------------------------------------
 */

/* Register offsets */
#define ZYNQ7000_XDCFG_CTRL_OFFSET          0x00
#define ZYNQ7000_XDCFG_LOCK_OFFSET          0x04
#define ZYNQ7000_XDCFG_CFG_OFFSET           0x08
#define ZYNQ7000_XDCFG_INT_STS_OFFSET       0x0C
#define ZYNQ7000_XDCFG_INT_MASK_OFFSET      0x10
#define ZYNQ7000_XDCFG_STATUS_OFFSET        0x14
#define ZYNQ7000_XDCFG_DMA_SRC_ADDR_OFFSET  0x18
#define ZYNQ7000_XDCFG_DMA_DEST_ADDR_OFFSET 0x1C
#define ZYNQ7000_XDCFG_DMA_SRC_LEN_OFFSET   0x20
#define ZYNQ7000_XDCFG_DMA_DEST_LEN_OFFSET  0x24
#define ZYNQ7000_XDCFG_MULTIBOOT_OFFSET     0x2C
#define ZYNQ7000_XDCFG_UNLOCK_OFFSET        0x34
#define ZYNQ7000_XDCFG_MCTRL_OFFSET         0x80


/* Control register */
#define ZYNQ7000_XDCFG_UNLOCK_KEY      0x757BDF0D /* Write to Unlock Register to use the devcfg interface */
#define ZYNQ7000_XDCFG_CTRL_ENBALE_AES 0x7        /* Enable AES engine*/

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dap_en          : 3,
            dbgen           : 1,
            niden           : 1,
            spiden          : 1,
            spniden         : 1,
            sec_en          : 1,
            seu_en          : 1,
            pcfg_aes_en     : 3,
            pcfg_aes_fuse   : 1,
            reserved_0      : 10,
            jtag_chain_dis  : 1,
            multiboot_en    : 1,
            pcap_rate_en    : 1,
            pcap_mode       : 1,
            pcap_pr         : 1,
            reserved_1      : 1,
            pcfg_por_cnt_4k : 1,
            pcfg_prog_b     : 1,
            force_rst       : 1;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_ctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Locks for the Control register */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dbg           : 1,
            sec           : 1,
            seu           : 1,
            aes_en        : 1,
            aes_fuse_lock : 1,
            reserved      : 27;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_lock_reg_t;
#endif /* __ASSEMBLER__ */


/* Configuration register : contains configuration information for the AXI transfers, and other general setup */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0      : 4, /* do not modify */
            disable_dst_inc : 1,
            disable_src_inc : 1,
            wclk_edge       : 1,
            rclk_edge       : 1,
            wfifo_th        : 2,
            rfifo_th        : 2,
            reserved_1      : 20;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_cfg_reg_t;
#endif /* __ASSEMBLER__ */


/* Interrupt Status and Mask registers*/
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ixr_pcfg_init_ne    : 1,
            ixr_pcfg_init_pe    : 1,
            ixr_pcfg_done       : 1,
            ixr_pcfg_cfg_rst    : 1,
            ixr_pcfg_por_b      : 1,
            ixr_pcfg_seu_err    : 1,
            ixr_pcfg_hmac_err   : 1,
            reserved_0          : 4,
            ixr_p2d_len_err     : 1,
            ixr_d_p_done        : 1,
            ixr_dma_done        : 1,
            ixr_dma_q_ov        : 1,
            ixr_dma_cmd_err     : 1,
            ixr_rd_fifo_lvl     : 1,
            ixr_wr_fifo_lvl     : 1,
            ixr_rx_fifo_ov      : 1,
            reserved_1          : 1,
            ixr_axi_rerr        : 1,
            ixr_axi_rto         : 1,
            ixr_axi_werr        : 1,
            ixr_axi_wto         : 1,
            reserved_2          : 3,
            pss_cfg_reset_b_int : 1,
            pss_gts_cfg_b_int   : 1,
            pss_gpwrdwn_b_int   : 1,
            pss_fst_cfg_b_int   : 1,
            pss_gts_usr_b_int   : 1;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_int_reg_t;
#endif /* __ASSEMBLER__ */


#ifndef __ASSEMBLER__
typedef zynq7000_xdcfg_int_reg_t zynq7000_xdcfg_int_sts_reg_t;
typedef zynq7000_xdcfg_int_reg_t zynq7000_xdcfg_int_mask_reg_t;
#endif /* __ASSEMBLER__ */


/* Miscellaneous Status register*/
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0         : 1, /* do not modify */
            efuse_jtag_dis     : 1,
            efuse_sec_en       : 1,
            efuse_sw_reserve   : 1,
            pcfg_init          : 1,
            pss_cfg_reset_b    : 1,
            illegal_apb_access : 1,
            secure_rst         : 1,
            pss_gts_cfg_b      : 1,
            pss_gpwrdwn_b      : 1,
            pss_fst_cfg_b      : 1,
            pss_gts_usr_b      : 1,
            tx_fifo_lvl        : 7,
            reserved_1         : 1,
            rx_fifo_lvl        : 5,
            reserved_2         : 3,
            dma_done_cnt       : 2,
            dma_cmd_q_e        : 1,
            dma_cmd_q_f        : 1;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_status_reg_t;
#endif /* __ASSEMBLER__ */


/* DMA Source Address register */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            src_addr : 32;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_dma_src_addr_reg_t;
#endif /* __ASSEMBLER__ */


/* DMA Destination Address register*/
#define ZYNQ7000_XDCFG_DMA_INVALID_ADDRESS 0xFFFFFFFF

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dst_addr : 32;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_dma_dest_addr_reg_t;
#endif /* __ASSEMBLER__ */


/* DMA Source and Destination Transfer Length registers */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dma_len  : 27,
            reserved : 5;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_dma_len_reg_t;
#endif /* __ASSEMBLER__ */

#ifndef __ASSEMBLER__
typedef zynq7000_xdcfg_dma_len_reg_t zynq7000_xdcfg_dma_src_len_reg_t;
typedef zynq7000_xdcfg_dma_len_reg_t zynq7000_xdcfg_dma_dest_len_reg_t;
#endif /* __ASSEMBLER__ */


/* Multi-Boot Address Pointer register */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            multiboot_addr : 13,
            reserved       : 19;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_multiboot_addr_reg_t;
#endif /* __ASSEMBLER__ */


/* Unlock Control register */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            unlock : 32;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_unlock_reg_t;
#endif /* __ASSEMBLER__ */


/* Miscellaneous Control register */
#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved_0 : 2, /* always write with 0 */
            reserved_1 : 2,
            pcap_lpbk  : 1,
            reserved_2 : 3,
            pcfg_por_b : 1,
            reserved_3 : 3,
            reserved_4 : 2, /* do not modify */
            reserved_5 : 9,
            reserved_6 : 1, /* always write with 1 */
            reserved_7 : 4, /* do not modify */
            ps_version : 4;
    } __attribute__((packed)) bits;
} zynq7000_xdcfg_mctrl_reg_t;
#endif /* __ASSEMBLER__ */


/* Return Codes */
#define ZYNQ7000_SUCCESS 0
#define ZYNQ7000_FAILURE 1


/* ----------------------------------------------------------------------------
 * XADC
 * Part of the Device Config Interface. All Offsets are based on the devcfg
 * base address.
 * ----------------------------------------------------------------------------
 */

/* Register offsets */
#define ZYNQ7000_XADC_CFG_OFFSET      0x100 /* XADC Configuration Register */
#define ZYNQ7000_XADC_INT_STS_OFFSET  0x104 /* XADC Interrupt Status Register */
#define ZYNQ7000_XADC_INT_MASK_OFFSET 0x108 /* XADC Interrupt Mask Register */
#define ZYNQ7000_XADC_MSTS_OFFSET     0x10C /* XADC Miscellaneous Status Register */
#define ZYNQ7000_XADC_CMDFIFO_OFFSET  0x110 /* XADC Command FIFO Register */
#define ZYNQ7000_XADC_RDFIFO_OFFSET   0x114 /* XADC Data FIFO Register */
#define ZYNQ7000_XADC_MCTL_OFFSET     0x118 /* XADC Miscellaneous Control Register */

/* XADC Config Register definitions */
#define ZYNQ7000_XADC_CFG_ENABLE_MASK  0x80000000 /* Enable access from the PS */
#define ZYNQ7000_XADC_CFG_CFIFOTH_MASK 0x00F00000 /* Command FIFO Threshold mask */
#define ZYNQ7000_XADC_CFG_DFIFOTH_MASK 0x000F0000 /* Data FIFO Threshold mask */
#define ZYNQ7000_XADC_CFG_WEDGE_MASK   0x00002000 /* Write Edge Mask */
#define ZYNQ7000_XADC_CFG_REDGE_MASK   0x00001000 /* Read Edge Mask */
#define ZYNQ7000_XADC_CFG_TCKRATE_MASK 0x00000300 /* Clock freq control */
#define ZYNQ7000_XADC_CFG_IGAP_MASK    0x0000001F /* Idle Gap between successive commands */

/* XADC Interrupt Status/Mask Register Bit definitions */
#define ZYNQ7000_XADC_INTX_ALL_MASK       0x000003FF /* Alarm Signals Mask  */
#define ZYNQ7000_XADC_INTX_CFIFO_LTH_MASK 0x00000200 /* CMD FIFO less than threshold */
#define ZYNQ7000_XADC_INTX_DFIFO_GTH_MASK 0x00000100 /* Data FIFO greater than threshold */
#define ZYNQ7000_XADC_INTX_OT_MASK        0x00000080 /* Over temperature Alarm Status */
#define ZYNQ7000_XADC_INTX_ALM_ALL_MASK   0x0000007F /* Alarm Signals Mask  */
#define ZYNQ7000_XADC_INTX_ALM6_MASK      0x00000040 /* Alarm 6 Mask  */
#define ZYNQ7000_XADC_INTX_ALM5_MASK      0x00000020 /* Alarm 5 Mask  */
#define ZYNQ7000_XADC_INTX_ALM4_MASK      0x00000010 /* Alarm 4 Mask  */
#define ZYNQ7000_XADC_INTX_ALM3_MASK      0x00000008 /* Alarm 3 Mask  */
#define ZYNQ7000_XADC_INTX_ALM2_MASK      0x00000004 /* Alarm 2 Mask  */
#define ZYNQ7000_XADC_INTX_ALM1_MASK      0x00000002 /* Alarm 1 Mask  */
#define ZYNQ7000_XADC_INTX_ALM0_MASK      0x00000001 /* Alarm 0 Mask  */

/* XADC Miscellaneous Register Bit definitions */
#define ZYNQ7000_XADC_MSTS_CFIFO_LVL_MASK 0x000F0000 /* Command FIFO Level mask */
#define ZYNQ7000_XADC_MSTS_DFIFO_LVL_MASK 0x0000F000 /* Data FIFO Level Mask  */
#define ZYNQ7000_XADC_MSTS_CFIFOF_MASK    0x00000800 /* Command FIFO Full Mask  */
#define ZYNQ7000_XADC_MSTS_CFIFOE_MASK    0x00000400 /* Command FIFO Empty Mask  */
#define ZYNQ7000_XADC_MSTS_DFIFOF_MASK    0x00000200 /* Data FIFO Full Mask  */
#define ZYNQ7000_XADC_MSTS_DFIFOE_MASK    0x00000100 /* Data FIFO Empty Mask  */
#define ZYNQ7000_XADC_MSTS_OT_MASK        0x00000080 /* Over Temperature Mask */
#define ZYNQ7000_XADC_MSTS_ALM_MASK       0x0000007F /* Alarms Mask  */

/* Reset Keys */
#define ZYNQ7000_XADC_MCTL_RESET_EN  0x10 /* Reset Enabled */
#define ZYNQ7000_XADC_MCTL_RESET_DIS 0x00 /* Reset Disabled */


#ifndef __ASSEMBLER__
typedef struct {
    zynq7000_xdcfg_ctrl_reg_t ctrl;
    zynq7000_xdcfg_lock_reg_t lock;
    zynq7000_xdcfg_cfg_reg_t cfg;
    zynq7000_xdcfg_int_sts_reg_t int_sts;
    zynq7000_xdcfg_int_mask_reg_t int_mask;
    zynq7000_xdcfg_status_reg_t status;
    zynq7000_xdcfg_dma_src_addr_reg_t src_addr;
    zynq7000_xdcfg_dma_dest_addr_reg_t dest_addr;
    zynq7000_xdcfg_dma_src_len_reg_t src_len;
    zynq7000_xdcfg_dma_dest_len_reg_t dest_len;
    uint8_t padding_1[4];
    zynq7000_xdcfg_multiboot_addr_reg_t multiboot;
    uint8_t padding_2[4];
    zynq7000_xdcfg_unlock_reg_t unlock;
    uint8_t padding_3[72];
    zynq7000_xdcfg_mctrl_reg_t mctrl;
    uint8_t padding_4[124];
    uint32_t xadc_cfg;
    uint32_t xadc_int_sts;
    uint32_t xadc_int_mask;
    uint32_t xadc_msts;
    uint32_t xadc_cmdfifo;
    uint32_t xadc_rdfifo;
    uint32_t xadc_mctl;
} __attribute__((packed)) zynq7000_devcfg_regs_t;

_Static_assert(sizeof(zynq7000_devcfg_regs_t) == ZYNQ7000_DEVCFG_SIZE, "Devcfg size mismatch");

ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, ctrl,          ZYNQ7000_XDCFG_CTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, lock,          ZYNQ7000_XDCFG_LOCK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, cfg,           ZYNQ7000_XDCFG_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, int_sts,       ZYNQ7000_XDCFG_INT_STS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, int_mask,      ZYNQ7000_XDCFG_INT_MASK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, status,        ZYNQ7000_XDCFG_STATUS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, src_addr,      ZYNQ7000_XDCFG_DMA_SRC_ADDR_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, dest_addr,     ZYNQ7000_XDCFG_DMA_DEST_ADDR_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, src_len,       ZYNQ7000_XDCFG_DMA_SRC_LEN_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, dest_len,      ZYNQ7000_XDCFG_DMA_DEST_LEN_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, multiboot,     ZYNQ7000_XDCFG_MULTIBOOT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, multiboot,     ZYNQ7000_XDCFG_MULTIBOOT_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, mctrl,         ZYNQ7000_XDCFG_MCTRL_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_cfg,      ZYNQ7000_XADC_CFG_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_int_sts,  ZYNQ7000_XADC_INT_STS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_int_mask, ZYNQ7000_XADC_INT_MASK_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_msts,     ZYNQ7000_XADC_MSTS_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_cmdfifo,  ZYNQ7000_XADC_CMDFIFO_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_rdfifo,   ZYNQ7000_XADC_RDFIFO_OFFSET);
ZYNQ7000_STATIC_ASSERT_OFFSET_OF(zynq7000_devcfg_regs_t, xadc_mctl,     ZYNQ7000_XADC_MCTL_OFFSET);
#endif /* __ASSEMBLER__ */

/* ----------------------------------------------------------------------------
 * Interrupts
 * ----------------------------------------------------------------------------
 */

#define ZYNQ7000_IRQ_GLOBAL_TIMER  27
#define ZYNQ7000_IRQ_PRIVATE_TIMER 29
#define ZYNQ7000_IRQ_XADC          39
#define ZYNQ7000_IRQ_WDT           41
#define ZYNQ7000_IRQ_TTC0_TIMER0   42
#define ZYNQ7000_IRQ_TTC0_TIMER1   43
#define ZYNQ7000_IRQ_TTC0_TIMER2   44
#define ZYNQ7000_IRQ_DMA_ABORT     45
#define ZYNQ7000_IRQ_DMA_CHAN0     46
#define ZYNQ7000_IRQ_DMA_CHAN1     47
#define ZYNQ7000_IRQ_DMA_CHAN2     48
#define ZYNQ7000_IRQ_DMA_CHAN3     49
#define ZYNQ7000_IRQ_GEM0          54
#define ZYNQ7000_IRQ_UART0         59
#define ZYNQ7000_IRQ_CAN0          60
#define ZYNQ7000_IRQ_TTC1_TIMER0   69
#define ZYNQ7000_IRQ_TTC1_TIMER1   70
#define ZYNQ7000_IRQ_TTC1_TIMER2   71
#define ZYNQ7000_IRQ_DMA_CHAN4     72
#define ZYNQ7000_IRQ_DMA_CHAN5     73
#define ZYNQ7000_IRQ_DMA_CHAN6     74
#define ZYNQ7000_IRQ_DMA_CHAN7     75
#define ZYNQ7000_IRQ_UART1         82
#define ZYNQ7000_IRQ_CAN1          83
#define ZYNQ7000_IRQ_I2C1          57
#define ZYNQ7000_IRQ_I2C2          80

#endif /* __ZYNQ_7000_H__ */
