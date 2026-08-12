/*
 * (c) 2023-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * Allwinner H3 (QuadCore Cortex-A7) system layout
 */

#ifndef __AW_H3_H__
#define __AW_H3_H__

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#endif /* __ASSEMBLER__ */

#define AW_H3_STATIC_ASSERT_OFFSET_OF(type, field, offset) \
    _Static_assert(offsetof(type, field) == (offset), "wrong field offset")

#define AW_H3_NUM_OF_CORES		4


/*
 * Memory map
 */
#define AW_H3_SRAM_A1_BASE      0x00000000
#define AW_H3_SRAM_A2_BASE      0x00044000
#define AW_H3_SRAM_C_BASE       0x00010000
#define AW_H3_DE_BASE           0x01000000
#define AW_H3_DE_INTR_BASE      0x01400000




/*
 * System Control
 */
#define AW_H3_SYS_CTRL_BASE     0x01C00000

#define VER_REG                 0x24 // Version Register
#define EMAC_EPHY_CLK_REG       0x30 // EMAC-EPHY Clock Register




/*
 * DMA
 */
#define AW_H3_DMA_BASE          0x01C02000

#define DMA_IRQ_EN_REG0         0x00 // DMA IRQ Enable Register0
#define DMA_IRQ_EN_REG1         0x04 // DMA IRQ Enable Register1
#define DMA_IRQ_PEND_REG0       0x10 // DMA IRQ Pending Register0
#define DMA_IRQ_PEND_REG1       0x14 // DMA IRQ Pending Register1
#define DMA_SEC_REG             0x20 // DMA Security Register
#define DMA_AUTO_GATE_REG       0x28 // DMA Auto Gating Register
#define DMA_STA_REG             0x30 // DMA Status Register
#define DMA_EN_REG(N)           (0x100+N*0x40) // DMA Channel Enable Register (N=0~11)
#define DMA_PAU_REG(N)          (0x100+N*0x40+0x4) // DMA Channel Pause Register (N=0~11)
#define DMA_DESC_ADDR_REG(N)    (0x100+N*0x40+0x8) // DMA Channel DMA Channel Start Address Register (N=0~11)
#define DMA_CFG_REG(N)          (0x100+N*0x40+0xC) // DMA Channel Configuration Register (N=0~11)
#define DMA_CUR_SRC_REG(N)      (0x100+N*0x40+0x10) // DMA Channel Current Source Register (N=0~11)
#define DMA_CUR_DEST_REG(N)     (0x100+N*0x40+0x14) // DMA Channel Current Destination Register (N=0~11)
#define DMA_BCNT_LEFT_REG(N)    (0x100+N*0x40+0x18) // DMA Channel Byte Counter Left Register (N=0~11)
#define DMA_PARA_REG(N)         (0x100+N*0x40+0x1C) // DMA Channel Parameter Register (N=0~11)
#define DMA_FDESC_ADDR_REG(N)   (0x100+N*0x40+0x2C) // DMA Formar Descriptor Address Register (N=0~11)
#define DMA_PKG_NUM_REG(N)      (0x100+N*0x40+0x30) // DMA Package Number Register (N=0~11)

/////////////////////////////////////////////////////////////////////////////
#define DMA_IRQ_EN_MASK					(0x07)
#define DMA_IRQ_EN0_DMA0_HALF_IRQ_EN	(1 << 0)
#define DMA_IRQ_EN0_DMA0_PKG_IRQ_EN		(1 << 1)
#define DMA_IRQ_EN0_DMA0_QUEUE_IRQ_EN	(1 << 2)
#define DMA_IRQ_EN0_DMA0_SHIFT			(0)
#define DMA_IRQ_EN0_DMA1_HALF_IRQ_EN	(1 << 4)
#define DMA_IRQ_EN0_DMA1_PKG_IRQ_EN		(1 << 5)
#define DMA_IRQ_EN0_DMA1_QUEUE_IRQ_EN	(1 << 6)
#define DMA_IRQ_EN0_DMA1_SHIFT			(4)
#define DMA_IRQ_EN0_DMA2_HALF_IRQ_EN	(1 << 8)
#define DMA_IRQ_EN0_DMA2_PKG_IRQ_EN		(1 << 9)
#define DMA_IRQ_EN0_DMA2_QUEUE_IRQ_EN	(1 << 10)
#define DMA_IRQ_EN0_DMA2_SHIFT			(8)
#define DMA_IRQ_EN0_DMA3_HALF_IRQ_EN	(1 << 12)
#define DMA_IRQ_EN0_DMA3_PKG_IRQ_EN		(1 << 13)
#define DMA_IRQ_EN0_DMA3_QUEUE_IRQ_EN	(1 << 14)
#define DMA_IRQ_EN0_DMA3_SHIFT			(12)

#define DMA_IRQ_PEND_MASK				(0x07)
#define DMA_IRQ_PEND0_DMA0_HALF_IRQ_EN	(1 << 0)
#define DMA_IRQ_PEND0_DMA0_PKG_IRQ_EN	(1 << 1)
#define DMA_IRQ_PEND0_DMA0_QUEUE_IRQ_EN	(1 << 2)
#define DMA_IRQ_PEND0_DMA0_SHIFT		(0)
#define DMA_IRQ_PEND0_DMA1_HALF_IRQ_EN	(1 << 4)
#define DMA_IRQ_PEND0_DMA1_PKG_IRQ_EN	(1 << 5)
#define DMA_IRQ_PEND0_DMA1_QUEUE_IRQ_EN	(1 << 6)
#define DMA_IRQ_PEND0_DMA1_SHIFT		(4)
#define DMA_IRQ_PEND0_DMA2_HALF_IRQ_EN	(1 << 8)
#define DMA_IRQ_PEND0_DMA2_PKG_IRQ_EN	(1 << 9)
#define DMA_IRQ_PEND0_DMA2_QUEUE_IRQ_EN	(1 << 10)
#define DMA_IRQ_PEND0_DMA2_SHIFT		(8)
#define DMA_IRQ_PEND0_DMA3_HALF_IRQ_EN	(1 << 12)
#define DMA_IRQ_PEND0_DMA3_PKG_IRQ_EN	(1 << 13)
#define DMA_IRQ_PEND0_DMA3_QUEUE_IRQ_EN	(1 << 14)
#define DMA_IRQ_PEND0_DMA3_SHIFT		(12)

#define DMA_LLI_LAST_ITEM				0xfffff800
#define DMA_NORMAL_WAIT					8

#define DMA_CHAN_ENABLE_START			(1 << 0)
#define DMA_CHAN_ENABLE_STOP			0

#define DMA_CHAN_MAX_DRQ				0x1f
#define DMA_CHAN_CFG_SRC_DRQ(x)			((x) & DMA_CHAN_MAX_DRQ)
#define DMA_CHAN_CFG_SRC_IO_MODE		(1 << 5)
#define DMA_CHAN_CFG_SRC_LINEAR_MODE	(0 << 5)
#define DMA_CHAN_CFG_SRC_BURST(x)		(((x) & 0x3) << 6)
#define DMA_CHAN_CFG_SRC_WIDTH(x)		(((x) & 0x3) << 9)

#define DMA_CHAN_CFG_DST_DRQ(x)			(DMA_CHAN_CFG_SRC_DRQ(x) << 16)
#define DMA_CHAN_CFG_DST_IO_MODE		(DMA_CHAN_CFG_SRC_IO_MODE << 16)
#define DMA_CHAN_CFG_DST_LINEAR_MODE	(DMA_CHAN_CFG_SRC_LINEAR_MODE << 16)
#define DMA_CHAN_CFG_DST_BURST(x)		(DMA_CHAN_CFG_SRC_BURST(x) << 16)
#define DMA_CHAN_CFG_DST_WIDTH(x)		(DMA_CHAN_CFG_SRC_WIDTH(x) << 16)
/////////////////////////////////////////////////////////////////////////////




/*
 * NAND Flash
 */
#define AW_H3_NFDC_BASE         0x01C03000

#define NDFC_CTL                0x00 // NDFC Configure and Control Register
#define NDFC_ST                 0x04 // NDFC Status Information Register
#define NDFC_INT                0x08 // NDFC Interrupt Control Register
#define NDFC_TIMING_CTL         0x0C // NDFC Timing Control Register
#define NDFC_TIMING_CFG         0x10 // NDFC Timing Configure Register
#define NDFC_ADDR_LOW           0x14 // NDFC Low Word Address Register
#define NDFC_ADDR_HIGH          0x18 // NDFC High Word Address Register
#define NDFC_BLOCK_NUM          0x1C // NDFC Data Block Number Register
#define NDFC_CNT                0x20 // NDFC Data Counter for data transfer Register
#define NDFC_CMD                0x24 // Set up NDFC commands Register
#define NDFC_RCMD_SET           0x28 // Read Command Set Register for vendors NAND memory
#define NDFC_WCMD_SET           0x2C // Write Command Set Register for vendors NAND memory
#define NDFC_ECC_CTL            0x34 // ECC Configure and Control Register
#define NDFC_ECC_ST             0x38 // ECC Status and Operation information Register
#define NDFC_EFR                0x3C // Enhanced Feature Register
#define NDFC_ERR_CNT0           0x40 // Corrected Error Bit Counter Register 0
#define NDFC_ERR_CNT1           0x44 // Corrected Error Bit Counter Register 1
#define NDFC_USER_DATAn(N)      (0x50+4*n) // User Data Field Register n (n from 0 to 15)
#define NDFC_EFNAND_STA         0x90 // EFNAND Status Register
#define NDFC_SPARE_AREA         0xA0 // Spare Area Configure Register
#define NDFC_PAT_ID             0xA4 // Pattern ID Register
#define NDFC_RDATA_STA_CTL      0xA8 // Read Data Status Control Register
#define NDFC_RDATA_STA_0        0xAC // Read Data Status Register 0
#define NDFC_RDATA_STA_1        0xB0 // Read Data Status Register 1
#define NDFC_MDMA_ADDR          0xC0 // MBUS DMA Address Register
#define NDFC_MDMA_CNT           0xC4 // MBUS DMA Data Counter Register
#define NDFC_IO_DATA            0x300 // Data Input/ Output Port Address Register
#define RAM0_BASE               0x400 // 1024 Bytes RAM0 base
#define RAM1_BASE               0x800 // 1024 Bytes RAM1 base




/*
 *
 */
#define AW_H3_TS_BASE           		0x01C06000
#define AW_H3_Key_Memory_Space_BASE     0x01C0B000




/*
 * LCD
 */
#define AW_H3_LCD0_BASE         0x01C0C000
#define AW_H3_LCD1_BASE         0x01C0D000




/*
 *
 */
#define AW_H3_VE_BASE           0x01C0E000




/*
 * SD/MMC
 */
#define AW_H3_SD0_BASE          0x01C0F000
#define AW_H3_SD1_BASE          0x01C10000
#define AW_H3_SD2_BASE          0x01C11000
#define AW_H3_SD_SIZE           0x1000
#define SD_GCTL     	0x00 // Control register
#define SD_CKCR     	0x04 // Clock Control register
#define SD_TMOR     	0x08 // Time out register
#define SD_BWDR     	0x0C // Bus Width register
#define SD_BKSR     	0x10 // Block size register
#define SD_BYCR     	0x14 // Byte count register
#define SD_CMDR     	0x18 // Command register
#define SD_CAGR     	0x1C // Command argument register
#define SD_RESP0    	0x20 // Response 0 register
#define SD_RESP1    	0x24 // Response 1 register
#define SD_RESP2    	0x28 // Response 2 register
#define SD_RESP3    	0x2C // Response 3 register
#define SD_IMKR     	0x30 // Interrupt mask register
#define SD_MISR     	0x34 // Masked interrupt status register
#define SD_RISR     	0x38 // Raw interrupt status register
#define SD_STAR     	0x3C // Status register
#define SD_FWLR     	0x40 // FIFO Water Level register
#define SD_FUNS     	0x44 // FIFO Function Select register
#define SD_A12A     	0x58 // Auto command 12 argument
#define SD_NTSR     	0x5C // SD NewTiming Set Register
#define SD_SDBG     	0x60 // SD NewTiming Set Debug Register
#define SD_HWRST    	0x78 // Hardware Reset Register
#define SD_DMAC     	0x80 // BUS Mode Control
#define SD_DLBA     	0x84 // Descriptor List Base Address
#define SD_IDST     	0x88 // DMAC Status
#define SD_IDIE     	0x8C // DMAC Interrupt Enable
#define SD_THLDC    	0x100 // ard Threshold Control register
#define SD_DSBD     	0x10C // eMMC4.41 DDR Start Bit Detection Control
#define SD_RES_CRC      0x110 // CRC status from card/eMMC in write operation
#define SD_DATA7_CRC    0x114 // CRC Data7 from card/eMMC
#define SD_DATA6_CRC    0x118 // CRC Data7 from card/eMMC
#define SD_DATA5_CRC    0x11C // CRC Data7 from card/eMMC
#define SD_DATA4_CRC    0x120 // CRC Data7 from card/eMMC
#define SD_DATA3_CRC    0x124 // CRC Data7 from card/eMMC
#define SD_DATA2_CRC    0x128 // CRC Data7 from card/eMMC
#define SD_DATA1_CRC    0x12C // CRC Data7 from card/eMMC
#define SD_DATA0_CRC    0x130 // CRC Data7 from card/eMMC
#define SD_CRC_STA      0x134 // Response CRC from card/eMMC
#define SD_FIFO         0x200 // Read/Write FIFO




/*
 *
 */
#define AW_H3_SID_BASE          0x01C14000




/*
 * Crypto Engine
 */
#define AW_H3_Crypto_Eng_BASE   0x01C15000
#define AW_H3_Crypto_S_BASE     0x01C15800


#define CE_TDQ                  0x00 // Task Descriptioin Address
#define CE_CTR                  0x04 // Gating Control Register
#define CE_ICR                  0x08 // Interrupt Control Register
#define CE_ISR                  0x0c // Interrupt Status Register
#define CE_TLR                  0x10 // task Loader Register
#define CE_ESR                  0x18 // Task Error type Register
#define CE_CSSGR                0x1c // Current Source Scatter Group Register
#define CE_CDSGR                0x20 // Current Destination Scatter Group Register
#define CE_CSAR                 0x24 // Current Source Address Register
#define CE_CDAR                 0x28 // Current Destination Address Register
#define CE_TPR                  0x2c // Throughput Register




/*
 * Message Box
 */
#define AW_H3_MSG_BOX_BASE      	0x01C17000

#define MSGBOX_CTRL_REG0        	0x0000 // Message Queue Attribute Control Register 0
#define MSGBOX_CTRL_REG1        	0x0004 // Message Queue Attribute Control Register 1
#define MSGBOXU_IRQ_EN_REG(N)      (0x0040+N*0x20) // IRQ Enable For User N(N=0,1)
#define MSGBOXU_IRQ_STATUS_REG(N)  (0x0050+N*0x20) // IRQ Status For User N(N=0,1)
#define MSGBOXM_FIFO_STATUS_REG(N) (0x0100+N*0x4) // FIFO Status For Message Queue N(N = 0~7)
#define MSGBOXM_MSG_STATUS_REG(N)  (0x0140+N*0x4) // Message Status For Message Queue N(N=0~7)
#define MSGBOXM_MSG_REG(N)         (0x0180+N*0x4) // Message Register For Message Queue N(N=0~7)




/*
 * SpinLock
 */
#define AW_H3_SPINLOCK_BASE     	0x01C18000

#define SPINLOCK_SYSTATUS_REG  		0x0000 // Spinlock System Status Register
#define SPINLOCK_STATUS_REG     	0x0010 // Spinlock Status Register
#define SPINLOCK_LOCK_REGN(N)   	(0x100+N*0x4) // Spinlock Register N (N=0~31)




/*
 * USB OTG
 */
#define AW_H3_USB_OTG_DEV_BASE  	0x01C19000
#define AW_H3_USB_OTG_EHCI_BASE     0x01C1A000




/*
 * USB Host Controller
 */
#define AW_H3_USB_HCI1_BASE     0x01C1B000
#define AW_H3_USB_HCI2_BASE     0x01C1C000
#define AW_H3_USB_HCI3_BASE     0x01C1D000




/*
 * Secure Memory Controller
 */
#define AW_H3_SMC_BASE          0x01C1E000

#define SMC_CONFIG_REG          0x0 // SMC Configuration Register
#define SMC_ACTION_REG          0x4 // SMC Action Register
#define SMC_LD_RANGE_REG        0x8 // SMC Lock Down Range Register
#define SMC_LD_SELECT_REG       0xC // SMC Lock Down Select Register
#define SMC_INT_STATUS_REG      0x10 // SMC Interrupt Status Register
#define MC_INT_CLEAR_REG        0x14 // SMC Interrupt Clear Register
#define SMC_MST_BYP_REG         0x18 // SMC Master Bypass Register
#define SMC_MST_SEC_REG         0x1C // SMC Master Secure Register
#define SMC_FAIL_ADDR_REG       0x20 // SMC Fail Address Register
#define SMC_FAIL_CTRL_REG       0x28 // SMC Fail Control Register
#define SMC_FAIL_ID_REG         0x2C // SMC Fail ID Register
#define SMC_SPECU_CTRL_REG      0x30 // SMC Speculation Control Register
#define SMC_SEC_INV_EN_REG      0x34 // SMC Security Inversion Enable register
#define SMC_MST_ATTRI_REG       0x48 // SMC Master Attribute Register
#define DRM_MASTER_EN_REG       0x50 // DRM Master Enable Register
#define DRM_ILLACCE_REG         0x58 // DRM Illegal Access Register
#define DRM_STATADDR_REG        0x60 // DRM Start Address Register
#define DRM_ENDADDR_REG         0x68 // DRM End Address Register
#define SMC_REGION_SETUP_LO_REG(N)     (0x100+N*0x10) // Region Setup Low Register N (N=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)
#define SMC_REGION_SETUP_HI_REG(N)     (0x104+N*0x10) // Region Setup High Register N (N=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)
#define SMC_REGION_ATTR_REG(N)         (0x108+N*0x10) // Region Attribute Register N (N=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)




/*
 * CCU
 */
#define AW_H3_CCU_BASE          0x01C20000
#define AW_H3_CCU_SIZE          0x400

#define PLL_CPUX_CTRL_REG       0x0000 // PLL_CPUX Control Register
#define PLL_AUDIO_CTRL_REG      0x0008 // PLL_AUDIO Control Register
#define PLL_VIDEO_CTRL_REG      0x0010 // PLL_VIDEO Control Register
#define PLL_VE_CTRL_REG         0x0018 // PLL_VE Control Register
#define PLL_DDR_CTRL_REG        0x0020 // PLL_DDR Control Register
#define PLL_PERIPH0_CTRL_REG    0x0028 // PLL_PERIPH0 Control Register
#define LL_GPU_CTRL_REG         0x0038 // PLL_GPU Control Register
#define PLL_PERIPH1_CTRL_REG    0x0044 // PLL_PERIPH1_CTRL_REG
#define PLL_DE_CTRL_REG         0x0048 // PLL DE Control Register
#define CPUX_AXI_CFG_REG        0x0050 // CPUX/AXI Configuration register
#define AHB1_APB1_CFG_REG       0x0054 // APB2 _CFG_REG0x0058APB2 Configuration Register
#define APB2_CFG_REG            0x0058 // APB2 Configuration Register
#define AHB2_CFG_REG            0x005C // AHB2 Configuration Register
#define BUS_CLK_GATING_REG0     0x0060 // Bus Clock Gating Register 0
#define BUS_CLK_GATING_REG1     0x0064 // Bus Clock Gating Register 1
#define BUS_CLK_GATING_REG2     0x0068 // Bus Clock Gating Register 2
#define BUS_CLK_GATING_REG3     0x006C // Bus Clock Gating Register 3
#define BUS_CLK_GATING_REG4     0x0070 // Bus Clock Gating Register 4
#define THS_CLK_REG             0x0074 // THS Clock Register
#define NAND_CLK_REG            0x0080 // NAND Clock Register
#define SDMMC0_CLK_REG          0x0088 // SDMMC0 Clock Register
#define SDMMC1_CLK_REG          0x008C // SDMMC1 Clock Register
#define SDMMC2_CLK_REG          0x0090 // SDMMC2 Clock Register
#define CE_CLK_REG              0x009C // CE Clock Register
#define SPI0_CLK_REG            0x00A0 // SPI0 Clock Register
#define SPI1_CLK_REG            0x00A4 // SPI1 Clock Register
#define I2S_PCM0_CLK_REG        0x00B0 // I2S/PCM0 Clock Register
#define I2S_PCM1_CLK_REG        0x00B4 // I2S/PCM1 Clock Register
#define I2S_PCM2_CLK_REG        0x00B8 // I2S/PCM2 Clock Register
#define OWA_CLK_REG             0x00C0 // OWA Clock Register
#define USBPHY_CFG_REG          0x00CC // USBPHY Configuration Register
#define DRAM_CFG_REG            0x00F4 // DRAM Configuration Register
#define MBUS_RST_REG            0x00FC // MBUS Reset Register
#define DRAM_CLK_GATING_REG     0x0100 // DRAM Clock Gating Register
#define TCON0_CLK_REG           0x0118 // TCON0 Clock Register
#define TVE_CLK_REG             0x0120 // TVE Clock Register
#define DEINTERLACE_CLK_REG     0x0124 // DEINTERLACE Clock Register
#define CSI_MISC_CLK_REG        0x0130 // CSI_MISC Clock Register
#define CSI_CLK_REG             0x0134 // CSI Clock Register
#define VE_CLK_REG              0x013C // VE Clock Register
#define AC_DIG_CLK_REG          0x0140 // AC Digital Clock Register
#define AVS_CLK_REG             0x0144 // AVS Clock Register
#define HDMI_CLK_REG            0x0150 // HDMI Clock Register
#define HDMI_SLOW_CLK_REG       0x0154 // HDMI Slow Clock Register
#define MBUS_CLK_REG            0x015C // MBUS Clock Register
#define GPU_CLK_REG             0x01A0 // GPU Clock Register
#define PLL_STABLE_TIME_REG0    0x0200 // PLL Stable Time Register 0
#define PLL_STABLE_TIME_REG1    0x0204 // PLL Stable Time Register 1
#define PLL_CPUX_BIAS_REG       0x0220 // PLL_CPUX Bias Register
#define PLL_AUDIO_BIAS_REG      0x0224 // PLL_AUDIO Bias Register
#define PLL_VIDEO_BIAS_REG      0x0228 // PLL_VIDEO Bias Register
#define PLL_VE_BIAS_REG         0x022C // PLL_VE Bias Register
#define PLL_DDR_BIAS_REG        0x0230 // PLL_PERIPH0_BIAS_REG0x0234PLL_GPU_BIAS_REG0x023Cnt
#define PLL_PERIPH1_BIAS_REG    0x0244 // PLL_PERIPH1 Bias Register
#define PLL_DE_BIAS_REG         0x0248 // PLL_DE Bias Register
#define PLL_CPUX_TUN_REG        0x0250 // PLL_CPUX Tuning Register
#define PLL_DDR_TUN_REG         0x0260 // PLL_DDR Tuning Register
#define PLL_CPUX_PAT_CTRL_REG   0x0280 // PLL_CPUX Pattern Control Register
#define PLL_AUDIO_PAT_CTRL_REG0 0x0284 // PLL_AUDIO Pattern Control Register
#define PLL_VIDEO_PAT_CTRL_REG0 0x0288 //
#define PLL_VE_PAT_CTRL_REG0    0x028C //
#define PLL_DDR_PAT_CTRL_REG0   0x0290 // PLL_DDR Pattern Control Register
#define PLL_GPU_PAT_CTRL_REG    0x029C // PLL_GPU Pattern Control Register
#define PLL_PERIPH1_PAT_CTRL_REG1   0x02A4 // PLL_PERIPH1 Pattern Control Register
#define PLL_DE_PAT_CTRL_REG     0x02A8 // PLL_DE Pattern Control Register
#define BUS_SOFT_RST_REG0       0x02C0 // Bus Software Reset Register 0
#define BUS_SOFT_RST_REG1       0x02C4 // Bus Software Reset Register 1
#define BUS_SOFT_RST_REG2       0x02C8 // Bus Software Reset Register 2
#define BUS_SOFT_RST_REG3       0x02D0 // Bus Software Reset Register 3
#define BUS_SOFT_RST_REG4       0x02D8 // Bus Software Reset Register 4
#define CCU_SEC_SWITCH_REG      0x02F0 // CCU Security Switch Register
#define PS_CTRL_REG             0x0300 // PS Control Register
#define PS_CNT_REG              0x0304 // PS Counter Register

/////////////////////////////////////////////////////////////////////////////
#define CCU_BUS_CLK_GATING0_DMA				(1U << 6)
#define CCU_BUS_CLK_GATING0_HSTMR			(1U << 19)
#define CCU_BUS_CLK_GATING0_SPI0			(1U << 20)
#define CCU_BUS_CLK_GATING0_SPI1			(1U << 21)
#define CCU_BUS_CLK_GATING0_USB_OTG			(1U << 23)
#define CCU_BUS_CLK_GATING0_USB_OTG_EHCI0	(1U << 24)
#define CCU_BUS_CLK_GATING0_USB_EHCI1		(1U << 25)
#define CCU_BUS_CLK_GATING0_USB_EHCI2		(1U << 26)
#define CCU_BUS_CLK_GATING0_USB_EHCI3		(1U << 27)
#define CCU_BUS_CLK_GATING0_USB_OTG_OHCI0	(1U << 28)
#define CCU_BUS_CLK_GATING0_USB_OHCI1		(1U << 29)
#define CCU_BUS_CLK_GATING0_USB_OHCI2		(1U << 30)
#define CCU_BUS_CLK_GATING0_USB_OHCI3		(1U << 31)

#define CCU_BUS_CLK_GATING1_TCON0			(1U << 3)
#define CCU_BUS_CLK_GATING1_TCON1			(1U << 4)
#define CCU_BUS_CLK_GATING1_HDMI			(1U << 11)
#define CCU_BUS_CLK_GATING1_DE				(1U << 12)
#define CCU_BUS_CLK_GATING1_SPINLOCK		(1U << 22)

#define CCU_BUS_CLK_GATING2_AC_DIG			(1U << 0)
#define CCU_BUS_CLK_GATING2_THS				(1U << 8)

#define CCU_BUS_CLK_GATING3_TWI0			(1U << 0)
#define CCU_BUS_CLK_GATING3_TWI1			(1U << 1)
#define CCU_BUS_CLK_GATING3_UART0			(1U << 16)
#define CCU_BUS_CLK_GATING3_UART1			(1U << 17)
#define CCU_BUS_CLK_GATING3_UART2			(1U << 18)
#define CCU_BUS_CLK_GATING3_UART3			(1U << 19)

#define CCU_BUS_SOFT_RESET0_DMA				(1U << 6)
#define CCU_BUS_SOFT_RESET0_HSTMR			(1U << 19)
#define CCU_BUS_SOFT_RESET0_SPI0			(1U << 20)
#define CCU_BUS_SOFT_RESET0_SPI1			(1U << 21)
#define CCU_BUS_SOFT_RESET0_USB_OTG			(1U << 23)
#define CCU_BUS_SOFT_RESET0_USB_OTG_EHCI0	(1U << 24)
#define CCU_BUS_SOFT_RESET0_USB_EHCI1		(1U << 25)
#define CCU_BUS_SOFT_RESET0_USB_EHCI2		(1U << 26)
#define CCU_BUS_SOFT_RESET0_USB_EHCI3		(1U << 27)
#define CCU_BUS_SOFT_RESET0_USB_OTG_OHCI0	(1U << 28)
#define CCU_BUS_SOFT_RESET0_USB_OHCI1		(1U << 29)
#define CCU_BUS_SOFT_RESET0_USB_OHCI2		(1U << 30)
#define CCU_BUS_SOFT_RESET0_USB_OHCI3		(1U << 31)

#define CCU_BUS_SOFT_RESET1_TCON0			(1U << 3)
#define CCU_BUS_SOFT_RESET1_TCON1			(1U << 4)
#define CCU_BUS_SOFT_RESET1_HDMI2			(1U << 10)
#define CCU_BUS_SOFT_RESET1_HDMI			(1U << 11)
#define CCU_BUS_SOFT_RESET1_DE				(1U << 12)
#define CCU_BUS_SOFT_RESET1_SPINLOCK		(1U << 22)

#define CCU_BUS_SOFT_RESET3_AC				(1U << 0)
#define CCU_BUS_SOFT_RESET3_THS				(1U << 8)

#define CCU_BUS_SOFT_RESET4_TWI0			(1U << 0)
#define CCU_BUS_SOFT_RESET4_TWI1			(1U << 1)
#define CCU_BUS_SOFT_RESET4_UART0			(1U << 16)
#define CCU_BUS_SOFT_RESET4_UART1			(1U << 17)
#define CCU_BUS_SOFT_RESET4_UART2			(1U << 18)
#define CCU_BUS_SOFT_RESET4_UART3			(1U << 19)

#define CCU_PERIPH0_CLOCK_HZ				600000000		///< 600MHz
#define CCU_PLL_CPUX_MIN_CLOCK_HZ			200000000UL		///< 200MHz
#define CCU_PLL_CPUX_MAX_CLOCK_HZ 			2600000000UL	///< 2.6GHz

///< 4.3.5.19 THS Clock Register (THS_CLK_REG)
#define CCU_THS_CLK_SCLK_GATING				(1U << 31)

#define CCU_THS_CLK_SRC_OSC24M				0
#define CCU_THS_CLK_SRC_SHIFT				(24)
#define CCU_THS_CLK_SRC_MASK				(0xFU << 24)

#define CCU_THS_CLK_DIV_RATIO_1				(0)	///< /1
#define CCU_THS_CLK_DIV_RATIO_2				(1)	///< /2
#define CCU_THS_CLK_DIV_RATIO_4				(2)	///< /4
#define CCU_THS_CLK_DIV_RATIO_6				(3)	///< /6
#define CCU_THS_CLK_DIV_RATIO_SHIFT			(0)
#define CCU_THS_CLK_DIV_RATIO_MASK			(0xFU << 0)
/////////////////////////////////////////////////////////////////////////////




/*
 * TIMER
 */
#define AW_H3_TIMER_BASE        0x01C20C00

#define AW_H3_TIMER_SIZE        0xa0

#define AW_H3_TIMER0_IRG        50
#define AW_H3_TIMER1_IRG        51

#define TMR_IRQ_EN_REG          0x00 // Timer IRQ Enable Register
#define TMR_IRQ_STA_REG         0x04 // Timer Status Register
#define TMR0_CTRL_REG           0x10 // Timer 0 Control Register
#define TMR0_INTV_VALUE_REG     0x14 // Timer 0 Interval Value Register
#define TMR0_CUR_VALUE_REG      0x18 // Timer 0 Current Value Register
#define TMR1_CTRL_REG           0x20 // Timer 1 Control Register
#define TMR1_INTV_VALUE_REG     0x24 // Timer 1 Interval Value Register
#define TMR1_CUR_VALUE_REG      0x28 // Timer 1 Current Value Register
#define AVS_CNT_CTL_REG         0x80 /// AVS Control Register
#define AVS_CNT0_REG            0x84 // AVS Counter 0 Register
#define AVS_CNT1_REG            0x88 // AVS Counter 1 Register
#define AVS_CNT_DIV_REG         0x8C // AVS Divisor Register
#define WDOG0_IRQ_EN_REG        0xA0 // Watchdog 0 IRQ Enable Register
#define WDOG0_IRQ_STA_REG       0xA4 // Watchdog 0 Status Register
#define WDOG0_CTRL_REG          0xB0 // Watchdog 0 Control Register
#define WDOG0_CFG_REG           0xB4 // Watchdog 0 Configuration Register
#define WDOG0_MODE_REG          0xB8 // Watchdog 0 Mode Register

/////////////////////////////////////////////////////////////////////////////
// Timer IRQ Enable Register
#define TIMER_IRQ_EN_TMR0			(1U << 0)
#define TIMER_IRQ_EN_TMR1			(1U << 1)

// Timer IRQ Status Register
#define TIMER_IRQ_PEND_TMR0			(1U << 0)	// Set will clear
#define TIMER_IRQ_PEND_TMR1			(1U << 1)	// Set will clear

// Timer Control Register
#define TIMER_CTRL_EN_START			(1U << 0)	// 0:Stop/Pause 1:Start
#define TIMER_CTRL_RELOAD			(1U << 1)
#define TIMER_CTRL_CLK_SRC_OSC24M	(1U << 2)	// Bit 3:2
#define TIMER_CTRL_CLK_PRES_1		(0U << 4)	// Bit 6:4
#define TIMER_CTRL_CLK_PRES_2		(1U << 4)
#define TIMER_CTRL_CLK_PRES_4		(2U << 4)
#define TIMER_CTRL_CLK_PRES_8		(3U << 4)
#define TIMER_CTRL_CLK_PRES_16		(4U << 4)
#define TIMER_CTRL_CLK_PRES_32		(5U << 4)
#define TIMER_CTRL_CLK_PRES_64		(6U << 4)
#define TIMER_CTRL_CLK_PRES_128		(7U << 4)
#define TIMER_CTRL_SINGLE_MODE		(1U << 7)	// When interval reached , the timer will disable automatically

#define TIMER_INTV_1SECOND			 0xB71B00	// 12.000.000 ticks with TIMER_CTRL_CLK_PRES_2 and TIMER_CTRL_CLK_SRC_OSC24M
#define TIMER_INT_VAL_1MS	         0x2EE0	// 24MHz / 2 / 12000 = 1KHz, period 1ms
#define TIMER_INT_VAL_1MKS	         0xC	// 24MHz / 2 / 12 = 1MHz, period 1us
#define TIMER_SINGLE_MODE            0x94   // Select Single mode,24MHz clock source,2 pre-scale

#define AVS_CNT0_EN			        (1 << 0)
#define AVS_CNT1_EN			        (1 << 1)
#define AVS_CNT0_D_SHIFT	        0
#define AVS_CNT1_D_SHIFT	        16
/////////////////////////////////////////////////////////////////////////////




/*
 * Port Controller (PIO) (CPU-PORT)
 */
#define AW_H3_PIO_BASE      0x01C20800
#define AW_H3_PIO_SIZE      0x260
#define AW_H3_PIO_PORT_SIZE 0x24

#define AW_H3_PIO_Pn_CFG0(N)  (N*0x24+0x00) // PIO Port n Configure Register 0 (n from 0 to 6)
#define AW_H3_PIO_Pn_CFG1(N)  (N*0x24+0x04) // PIO Port n Configure Register 1 (n from 0 to 6)
#define AW_H3_PIO_Pn_CFG2(N)  (N*0x24+0x08) // PIO Port n Configure Register 2 (n from 0 to 6)
#define AW_H3_PIO_Pn_CFG3(N)  (N*0x24+0x0C) // PIO Port n Configure Register 3 (n from 0 to 6)
#define AW_H3_PIO_Pn_DAT(N)   (N*0x24+0x10) // PIO Port n Data Register (n from 0 to 6)
#define AW_H3_PIO_Pn_DRV0(N)  (N*0x24+0x14) // PIO Port n Multi-Driving Register 0 (n from 0 to 6)
#define AW_H3_PIO_Pn_DRV1(N)  (N*0x24+0x18) // PIO Port n Multi-Driving Register 1 (n from 0 to 6)
#define AW_H3_PIO_Pn_PUL0(N)  (N*0x24+0x1C) // PIO Port n Pull Register 0 (n from 0 to 6)
#define AW_H3_PIO_Pn_PUL1(N)  (N*0x24+0x20) // PIO Port n Pull Register 1 (n from 0 to 6)
#define AW_H3_PIO_PA_INT_CFG0 0x200+0*0x20+0x00 // PIO Interrrupt Configure Register 0
#define AW_H3_PIO_PA_INT_CFG1 0x200+0*0x20+0x04 // PIO Interrrupt Configure Register 1
#define AW_H3_PIO_PA_INT_CFG2 0x200+0*0x20+0x08 // PIO Interrrupt Configure Register 2
#define AW_H3_PIO_PA_INT_CFG3 0x200+0*0x20+0x0C // PIO Interrrupt Configure Register 3
#define AW_H3_PIO_PA_INT_CTL  0x200+0*0x20+0x10 // PIO Interrupt Control Register
#define AW_H3_PIO_PA_INT_STA  0x200+0*0x20+0x14 // PIO Interrupt Status Register
#define AW_H3_PIO_PA_INT_DEB  0x200+0*0x20+0x18 // PIO Interrupt Debounce Register
#define AW_H3_PIO_PG_INT_CFG0 0x200+1*0x20+0x00 // PIO Interrrupt Configure Register 0
#define AW_H3_PIO_PG_INT_CFG1 0x200+1*0x20+0x04 // PIO Interrrupt Configure Register 1
#define AW_H3_PIO_PG_INT_CFG2 0x200+1*0x20+0x08 // PIO Interrrupt Configure Register 2
#define AW_H3_PIO_PG_INT_CFG3 0x200+1*0x20+0x0C // PIO Interrrupt Configure Register 3
#define AW_H3_PIO_PG_INT_CTL  0x200+1*0x20+0x10 // PIO Interrupt Control Register
#define AW_H3_PIO_PG_INT_STA  0x200+1*0x20+0x14 // PIO Interrupt Status Register
#define AW_H3_PIO_PG_INT_DEB  0x200+1*0x20+0x18 // PIO Interrupt Debounce Register

#define AW_H3_PIO_PORT_A_MAX_PIN 21
#define AW_H3_PIO_PORT_C_MAX_PIN 18
#define AW_H3_PIO_PORT_D_MAX_PIN 17
#define AW_H3_PIO_PORT_E_MAX_PIN 15
#define AW_H3_PIO_PORT_F_MAX_PIN 6
#define AW_H3_PIO_PORT_G_MAX_PIN 13

#ifndef __ASSEMBLER__
#define PIO_CFG_BIT(n) select_##n : 3, reserved_##n : 1

#define PIO_CFG_0  PIO_CFG_BIT(0),  PIO_CFG_BIT(1),  PIO_CFG_BIT(2),  PIO_CFG_BIT(3), \
                   PIO_CFG_BIT(4),  PIO_CFG_BIT(5),  PIO_CFG_BIT(6),  PIO_CFG_BIT(7)

#define PIO_CFG_1  PIO_CFG_BIT(8),  PIO_CFG_BIT(9),  PIO_CFG_BIT(10), PIO_CFG_BIT(11), \
                   PIO_CFG_BIT(12), PIO_CFG_BIT(13), PIO_CFG_BIT(14), PIO_CFG_BIT(15)

#define PIO_CFG_2  PIO_CFG_BIT(16), PIO_CFG_BIT(17), PIO_CFG_BIT(18), PIO_CFG_BIT(19), \
                   PIO_CFG_BIT(20), PIO_CFG_BIT(21), PIO_CFG_BIT(22), PIO_CFG_BIT(23)

#define PIO_CFG_3  PIO_CFG_BIT(24), PIO_CFG_BIT(25), PIO_CFG_BIT(26), PIO_CFG_BIT(27), \
                   PIO_CFG_BIT(28), PIO_CFG_BIT(29), PIO_CFG_BIT(30), PIO_CFG_BIT(31)

#define PIO_CFG_STRUCT8(port, num_cfg)         \
    typedef union {                            \
        uint32_t raw;                          \
        struct {                               \
            uint32_t PIO_CFG_##num_cfg;        \
        } __attribute__((packed)) bits;        \
    } aw_h3_pio_p##port##_cfg##num_cfg##_reg_t

#define PIO_CFG0_STRUCT7(port)                  \
    typedef union {                             \
        uint32_t raw;                           \
        struct {                                \
            uint32_t                            \
                PIO_CFG_BIT(0), PIO_CFG_BIT(1), \
                PIO_CFG_BIT(2), PIO_CFG_BIT(3), \
                PIO_CFG_BIT(4), PIO_CFG_BIT(5), \
                PIO_CFG_BIT(6), reserved : 4;   \
        } __attribute__((packed)) bits;         \
    } aw_h3_pio_p##port##_cfg0_reg_t

#define PIO_CFG1_STRUCT4(port)                    \
    typedef union {                               \
        uint32_t raw;                             \
        struct {                                  \
            uint32_t                              \
                PIO_CFG_BIT(8), PIO_CFG_BIT(9),   \
                PIO_CFG_BIT(10), PIO_CFG_BIT(11), \
                reserved : 16;                    \
        } __attribute__((packed)) bits;           \
    } aw_h3_pio_p##port##_cfg1_reg_t

#define PIO_CFG1_STRUCT6(port)                    \
    typedef union {                               \
        uint32_t raw;                             \
        struct {                                  \
            uint32_t                              \
                PIO_CFG_BIT(8), PIO_CFG_BIT(9),   \
                PIO_CFG_BIT(10), PIO_CFG_BIT(11), \
                PIO_CFG_BIT(12), PIO_CFG_BIT(13), \
                reserved : 8;                     \
        } __attribute__((packed)) bits;           \
    } aw_h3_pio_p##port##_cfg1_reg_t

#define PIO_CFG2_STRUCT1(port)                  \
    typedef union {                             \
        uint32_t raw;                           \
        struct {                                \
            uint32_t                            \
                PIO_CFG_BIT(16), reserved : 28; \
        } __attribute__((packed)) bits;         \
    } aw_h3_pio_p##port##_cfg2_reg_t

#define PIO_CFG2_STRUCT2(port)                    \
    typedef union {                               \
        uint32_t raw;                             \
        struct {                                  \
            uint32_t                              \
                PIO_CFG_BIT(16), PIO_CFG_BIT(17), \
                reserved : 24;                    \
        } __attribute__((packed)) bits;           \
    } aw_h3_pio_p##port##_cfg2_reg_t

#define PIO_CFG2_STRUCT7(port)                    \
    typedef union {                               \
        uint32_t raw;                             \
        struct {                                  \
            uint32_t                              \
                PIO_CFG_BIT(16), PIO_CFG_BIT(17), \
                PIO_CFG_BIT(18), PIO_CFG_BIT(19), \
                PIO_CFG_BIT(20), PIO_CFG_BIT(21), \
                PIO_CFG_BIT(22), reserved : 4;    \
        } __attribute__((packed)) bits;           \
    } aw_h3_pio_p##port##_cfg2_reg_t

#define PIO_DEF_STRUCT_RESERVED(port, num, type) \
    typedef union {                              \
        uint32_t raw;                            \
        struct {                                 \
            uint32_t                             \
                reserved :32;                    \
        } __attribute__((packed)) bits;          \
    } aw_h3_pio_p##port##_##type##num##_reg_t;

/* Ports A Configure Registers 0-3 (aw_h3_pio_pa_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(a, 0);
PIO_CFG_STRUCT8(a, 1);
PIO_CFG2_STRUCT7(a);
PIO_DEF_STRUCT_RESERVED(a, 3, cfg);

/* Ports C Configure Registers 0-3 (aw_h3_pio_pc_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(c, 0);
PIO_CFG_STRUCT8(c, 1);
PIO_CFG2_STRUCT1(c);
PIO_DEF_STRUCT_RESERVED(c, 3, cfg);

/* Ports D Configure Registers 0-3 (aw_h3_pio_pd_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(d, 0);
PIO_CFG_STRUCT8(d, 1);
PIO_CFG2_STRUCT2(d);
PIO_DEF_STRUCT_RESERVED(d, 3, cfg);

/* Ports E Configure Registers 0-3 (aw_h3_pio_pe_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(e, 0);
PIO_CFG_STRUCT8(e, 1);
PIO_DEF_STRUCT_RESERVED(e, 2, cfg);
PIO_DEF_STRUCT_RESERVED(e, 3, cfg);

/* Ports F Configure Registers 0-3 (aw_h3_pio_pf_cfg<0..3>_reg_t) */
PIO_CFG0_STRUCT7(f);
PIO_DEF_STRUCT_RESERVED(f, 1, cfg);
PIO_DEF_STRUCT_RESERVED(f, 2, cfg);
PIO_DEF_STRUCT_RESERVED(f, 3, cfg);

/* Ports G Configure Registers 0-3 (aw_h3_pio_pg_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(g, 0);
PIO_CFG1_STRUCT6(g);
PIO_DEF_STRUCT_RESERVED(g, 2, cfg);
PIO_DEF_STRUCT_RESERVED(g, 3, cfg);

/* Ports L Configure Registers 0-3 (aw_h3_pio_pl_cfg<0..3>_reg_t) */
PIO_CFG_STRUCT8(l, 0);
PIO_CFG1_STRUCT4(l);
PIO_DEF_STRUCT_RESERVED(l, 2, cfg);
PIO_DEF_STRUCT_RESERVED(l, 3, cfg);

#define PIO_STRUCT_0_FIELD7(type)  \
    type##0  : 2, type##1  : 2,    \
    type##2  : 2, type##3  : 2,    \
    type##4  : 2, type##5  : 2,    \
    type##6  : 2, reserved : 18

#define PIO_STRUCT_0_FIELD12(type) \
    type##0  : 2, type##1  : 2,    \
    type##2  : 2, type##3  : 2,    \
    type##4  : 2, type##5  : 2,    \
    type##6  : 2, type##7  : 2,    \
    type##8  : 2, type##9  : 2,    \
    type##10 : 2, type##11 : 2,    \
    reserved : 8

#define PIO_STRUCT_0_FIELD14(type) \
    type##0  : 2, type##1  : 2,    \
    type##2  : 2, type##3  : 2,    \
    type##4  : 2, type##5  : 2,    \
    type##6  : 2, type##7  : 2,    \
    type##8  : 2, type##9  : 2,    \
    type##10 : 2, type##11 : 2,    \
    type##12 : 2, type##13 : 2,    \
    reserved : 4

#define PIO_STRUCT_0_FIELD16(type) \
    type##0  : 2, type##1  : 2,    \
    type##2  : 2, type##3  : 2,    \
    type##4  : 2, type##5  : 2,    \
    type##6  : 2, type##7  : 2,    \
    type##8  : 2, type##9  : 2,    \
    type##10 : 2, type##11 : 2,    \
    type##12 : 2, type##13 : 2,    \
    type##14 : 2, type##15 : 2

#define PIO_STRUCT_1_FIELD2(type) \
    type##16 : 2, type##17 : 2,   \
    reserved : 28

#define PIO_STRUCT_1_FIELD3(type) \
    type##16 : 2, type##17 : 2,   \
    type##18 : 2, reserved : 26

#define PIO_STRUCT_1_FIELD6(type) \
    type##16 : 2, type##17 : 2,   \
    type##18 : 2, type##19 : 2,   \
    type##20 : 2, type##21 : 2,   \
    reserved : 20

#define PIO_STRUCT_1_FIELD12(type) \
    type##16 : 2, type##17 : 2,    \
    type##18 : 2, type##19 : 2,    \
    type##20 : 2, type##21 : 2,    \
    type##22 : 2, type##23 : 2,    \
    type##24 : 2, type##25 : 2,    \
    type##26 : 2, type##27 : 2,    \
    reserved : 8

#define PIO_DEF_STRUCT(port, num_cfg, type, pins)         \
    typedef union {                                       \
        uint32_t raw;                                     \
        struct {                                          \
            uint32_t                                      \
                PIO_STRUCT_##num_cfg##_FIELD##pins(type); \
        } __attribute__((packed)) bits;                   \
    } aw_h3_pio_p##port##_##type##num_cfg##_reg_t

/* Ports A Multi-Driving Registers 0-1 (aw_h3_pio_pa_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(a, 0, drv, 16);
PIO_DEF_STRUCT(a, 1, drv, 6);

/* Ports C Multi-Driving Registers 0-1 (aw_h3_pio_pc_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(c, 0, drv, 16);
PIO_DEF_STRUCT(c, 1, drv, 3);

/* Ports D Multi-Driving Registers 0-1 (aw_h3_pio_pd_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(d, 0, drv, 16);
PIO_DEF_STRUCT(d, 1, drv, 2);

/* Ports E Multi-Driving Registers 0-1 (aw_h3_pio_pe_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(e, 0, drv, 16);
PIO_DEF_STRUCT_RESERVED(e, 1, drv);

/* Ports F Multi-Driving Registers 0-1 (aw_h3_pio_pf_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(f, 0, drv, 7);
PIO_DEF_STRUCT_RESERVED(f, 1, drv);

/* Ports G Multi-Driving Registers 0-1 (aw_h3_pio_pg_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(g, 0, drv, 14);
PIO_DEF_STRUCT_RESERVED(g, 1, drv);

/* Ports L Multi-Driving Registers 0-1 (aw_h3_pio_pl_drv<0..1>_reg_t) */
PIO_DEF_STRUCT(l, 0, drv, 12);
PIO_DEF_STRUCT_RESERVED(l, 1, drv);


/* Ports A Pull Registers 0-1 (aw_h3_pio_pa_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(a, 0, pull, 16);
PIO_DEF_STRUCT(a, 1, pull, 6);

/* Ports C Pull Registers 0-1 (aw_h3_pio_pc_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(c, 0, pull, 16);
PIO_DEF_STRUCT(c, 1, pull, 3);

/* Ports D Pull Registers 0-1 (aw_h3_pio_pd_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(d, 0, pull, 16);
PIO_DEF_STRUCT(d, 1, pull, 2);

/* Ports E Pull Registers 0-1 (aw_h3_pio_pe_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(e, 0, pull, 16);
PIO_DEF_STRUCT_RESERVED(e, 1, pull);

/* Ports F Pull Registers 0-1 (aw_h3_pio_pf_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(f, 0, pull, 7);
PIO_DEF_STRUCT_RESERVED(f, 1, pull);

/* Ports G Pull Registers 0-1 (aw_h3_pio_pg_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(g, 0, pull, 14);
PIO_DEF_STRUCT_RESERVED(g, 1, pull);

/* Ports L Pull Registers 0-1 (aw_h3_pio_pl_pull<0..1>_reg_t) */
PIO_DEF_STRUCT(l, 0, pull, 12);
PIO_DEF_STRUCT_RESERVED(l, 1, pull);

#define PIO_DATA_STRUCT(port, pins)     \
    typedef union {                     \
        uint32_t raw;                   \
        struct {                        \
            uint32_t                    \
                dat : pins,             \
                reserved : (32 - pins); \
        } __attribute__((packed)) bits; \
    } aw_h3_pio_p##port##_dat_reg_t

/* Ports A-L Data Register (aw_h3_pio_p<a..l>_dat_reg_t) */
PIO_DATA_STRUCT(a, 22);
PIO_DATA_STRUCT(c, 19);
PIO_DATA_STRUCT(d, 18);
PIO_DATA_STRUCT(e, 16);
PIO_DATA_STRUCT(f, 7);
PIO_DATA_STRUCT(g, 14);
PIO_DATA_STRUCT(l, 12);

#define ADD_PIO_PORT(port)                           \
    aw_h3_pio_p##port##_cfg0_reg_t p##port##_cfg0;   \
    aw_h3_pio_p##port##_cfg1_reg_t p##port##_cfg1;   \
    aw_h3_pio_p##port##_cfg2_reg_t p##port##_cfg2;   \
    aw_h3_pio_p##port##_cfg3_reg_t p##port##_cfg3;   \
    aw_h3_pio_p##port##_dat_reg_t  p##port##_dat;    \
    aw_h3_pio_p##port##_drv0_reg_t p##port##_drv0;   \
    aw_h3_pio_p##port##_drv1_reg_t p##port##_drv1;   \
    aw_h3_pio_p##port##_pull0_reg_t p##port##_pull0; \
    aw_h3_pio_p##port##_pull1_reg_t p##port##_pull1

typedef struct {
    ADD_PIO_PORT(a);
    uint32_t port_b_reserved[9];
    ADD_PIO_PORT(c);
    ADD_PIO_PORT(d);
    ADD_PIO_PORT(e);
    ADD_PIO_PORT(f);
    ADD_PIO_PORT(g);
    uint8_t  padding_0[296];
    uint32_t pa_int_cfg0;
    uint32_t pa_int_cfg1;
    uint32_t pa_int_cfg2;
    uint32_t pa_int_cfg3;
    uint32_t pa_int_ctl;
    uint32_t pa_int_sta;
    uint32_t pa_int_deb;
    uint8_t  padding_1[4];
    uint32_t pg_int_cfg0;
    uint32_t pg_int_cfg1;
    uint32_t pg_int_cfg2;
    uint32_t pg_int_cfg3;
    uint32_t pg_int_ctl;
    uint32_t pg_int_sta;
    uint32_t pg_int_deb;
} __attribute__ ((packed)) aw_h3_pio_regs_t;

_Static_assert(sizeof(aw_h3_pio_regs_t) == AW_H3_PIO_SIZE, "pio size mismatch");

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_cfg0,  AW_H3_PIO_Pn_CFG0(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_cfg1,  AW_H3_PIO_Pn_CFG1(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_cfg2,  AW_H3_PIO_Pn_CFG2(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_cfg3,  AW_H3_PIO_Pn_CFG3(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_dat,   AW_H3_PIO_Pn_DAT(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_drv0,  AW_H3_PIO_Pn_DRV0(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_drv1,  AW_H3_PIO_Pn_DRV1(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_pull0, AW_H3_PIO_Pn_PUL0(0));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pa_pull1, AW_H3_PIO_Pn_PUL1(0));

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_cfg0,  AW_H3_PIO_Pn_CFG0(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_cfg1,  AW_H3_PIO_Pn_CFG1(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_cfg2,  AW_H3_PIO_Pn_CFG2(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_cfg3,  AW_H3_PIO_Pn_CFG3(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_dat,   AW_H3_PIO_Pn_DAT(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_drv0,  AW_H3_PIO_Pn_DRV0(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_drv1,  AW_H3_PIO_Pn_DRV1(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_pull0, AW_H3_PIO_Pn_PUL0(2));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pc_pull1, AW_H3_PIO_Pn_PUL1(2));

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_cfg0,  AW_H3_PIO_Pn_CFG0(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_cfg1,  AW_H3_PIO_Pn_CFG1(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_cfg2,  AW_H3_PIO_Pn_CFG2(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_cfg3,  AW_H3_PIO_Pn_CFG3(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_dat,   AW_H3_PIO_Pn_DAT(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_drv0,  AW_H3_PIO_Pn_DRV0(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_drv1,  AW_H3_PIO_Pn_DRV1(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_pull0, AW_H3_PIO_Pn_PUL0(3));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pd_pull1, AW_H3_PIO_Pn_PUL1(3));

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_cfg0,  AW_H3_PIO_Pn_CFG0(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_cfg1,  AW_H3_PIO_Pn_CFG1(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_cfg2,  AW_H3_PIO_Pn_CFG2(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_cfg3,  AW_H3_PIO_Pn_CFG3(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_dat,   AW_H3_PIO_Pn_DAT(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_drv0,  AW_H3_PIO_Pn_DRV0(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_drv1,  AW_H3_PIO_Pn_DRV1(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_pull0, AW_H3_PIO_Pn_PUL0(4));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pe_pull1, AW_H3_PIO_Pn_PUL1(4));

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_cfg0,  AW_H3_PIO_Pn_CFG0(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_cfg1,  AW_H3_PIO_Pn_CFG1(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_cfg2,  AW_H3_PIO_Pn_CFG2(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_cfg3,  AW_H3_PIO_Pn_CFG3(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_dat,   AW_H3_PIO_Pn_DAT(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_drv0,  AW_H3_PIO_Pn_DRV0(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_drv1,  AW_H3_PIO_Pn_DRV1(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_pull0, AW_H3_PIO_Pn_PUL0(5));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pf_pull1, AW_H3_PIO_Pn_PUL1(5));

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_cfg0,  AW_H3_PIO_Pn_CFG0(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_cfg1,  AW_H3_PIO_Pn_CFG1(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_cfg2,  AW_H3_PIO_Pn_CFG2(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_cfg3,  AW_H3_PIO_Pn_CFG3(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_dat,   AW_H3_PIO_Pn_DAT(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_drv0,  AW_H3_PIO_Pn_DRV0(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_drv1,  AW_H3_PIO_Pn_DRV1(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_pull0, AW_H3_PIO_Pn_PUL0(6));
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio_regs_t, pg_pull1, AW_H3_PIO_Pn_PUL1(6));
#endif /* __ASSEMBLER__ */

/*
 * Port Controller (PIO) (CPUs-PORT)
 */
#define AW_H3_PIO_BASE2 0x01F02C00
#define AW_H3_PIO_SIZE2 0x21C

#define AW_H3_PIO_PL_CFG0     0*0x24+0x00 // PIO Port L Configure Register 0
#define AW_H3_PIO_PL_CFG1     0*0x24+0x04 // PIO Port L Configure Register 1
#define AW_H3_PIO_PL_CFG2     0*0x24+0x08 // PIO Port L Configure Register 2
#define AW_H3_PIO_PL_CFG3     0*0x24+0x0C // PIO Port L Configure Register 3
#define AW_H3_PIO_PL_DAT      0*0x24+0x10 // PIO Port L Data Register
#define AW_H3_PIO_PL_DRV0     0*0x24+0x14 // PIO Port L Multi-Driving Register 0
#define AW_H3_PIO_PL_DRV1     0*0x24+0x18 // PIO Port L Multi-Driving Register 1
#define AW_H3_PIO_PL_PUL0     0*0x24+0x1C // PIO Port L Pull Register 0
#define AW_H3_PIO_PL_PUL1     0*0x24+0x20 // PIO Port L Pull Register 1
#define AW_H3_PIO_PL_INT_CFG0 0x200+0*0x20+0x00 // PIO Port L Interrrupt Configure Register 0
#define AW_H3_PIO_PL_INT_CFG1 0x200+0*0x20+0x04 // PIO Port L Interrrupt Configure Register 1
#define AW_H3_PIO_PL_INT_CFG2 0x200+0*0x20+0x08 // PIO Port L Interrrupt Configure Register 2
#define AW_H3_PIO_PL_INT_CFG3 0x200+0*0x20+0x0C // PIO Port L Interrrupt Configure Register 3
#define AW_H3_PIO_PL_INT_CTL  0x200+0*0x20+0x10 // PIO Port L Interrupt Control Register
#define AW_H3_PIO_PL_INT_STA  0x200+0*0x20+0x14 // PIO Port L Interrupt Status Register
#define AW_H3_PIO_PL_INT_DEB  0x200+0*0x20+0x18 // PIO Port L Interrupt Debounce Register

#define AW_H3_PIO_PORT_L_MAX_PIN 11

#ifndef __ASSEMBLER__
typedef struct {
    ADD_PIO_PORT(l);
    uint8_t  padding[476];
    uint32_t pl_int_cfg0;
    uint32_t pl_int_cfg1;
    uint32_t pl_int_cfg2;
    uint32_t pl_int_cfg3;
    uint32_t pl_int_ctl;
    uint32_t pl_int_sta;
    uint32_t pl_int_deb;
} __attribute__ ((packed)) aw_h3_pio2_regs_t;

_Static_assert(sizeof(aw_h3_pio2_regs_t) == AW_H3_PIO_SIZE2, "pio2 size mismatch");

AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_cfg0,  AW_H3_PIO_PL_CFG0);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_cfg1,  AW_H3_PIO_PL_CFG1);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_cfg2,  AW_H3_PIO_PL_CFG2);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_cfg3,  AW_H3_PIO_PL_CFG3);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_dat,   AW_H3_PIO_PL_DAT);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_drv0,  AW_H3_PIO_PL_DRV0);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_drv1,  AW_H3_PIO_PL_DRV1);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_pull0, AW_H3_PIO_PL_PUL0);
AW_H3_STATIC_ASSERT_OFFSET_OF(aw_h3_pio2_regs_t, pl_pull1, AW_H3_PIO_PL_PUL1);
#endif /* __ASSEMBLER__ */


/*
 * One Wire Audio
 */
#define AW_H3_OWA_BASE          0x01C21000




/*
 * PWM
 */
#define AW_H3_PWM_BASE          0x01C21400

#define PWM_CH_CTRL             0x00 // PWM Control Register
#define PWM_CH0_PERIOD          0x04 // PWM Channel 0 Period Register




/*
 * Key ADC
 */
#define AW_H3_KEYADC_BASE        0x01C21800

#define KEY_ADC_CTR              0x00 // KEY_ADC Control Register
#define KEY_ADC_INTC             0x04 // KEY_ADC Interrupt Control Register
#define KEY_ADC_INTS             0x08 // KEY_ADC Interrupt Status Register
#define KEY_ADC_DATA             0x0C // KEY_ADC Data Register




/*
 * Inter-Integrated Sound
 */
#define AW_H3_I2S0_BASE         0x01C22000
#define AW_H3_I2S1_BASE         0x01C22400
#define AW_H3_I2S2_BASE         0x01C22800




/*
 * Audio Codec
 */
#define AW_H3_AC_BASE           0x01C22C00

//#define PAGE 266
// P.S. To much for now




/*
 * Secure Memory Task Contgroller
 */
#define AW_H3_SMTA_BASE         0x01C23400

#define SMTA_DECPORT0_STA_REG       0x4 // SMTA Decode Port0 Status Register
#define SMTA_DECPORT0_SET_REG       0x8 // SMTA Decode Port0 Set Register
#define SMTA_DECPORT0_CLR_REG       0xC // SMTA Decode Port0 Clear Register
#define SMTA_DECPORT1_STA_REG       0x10 // SMTA Decode Port1 Status Register
#define SMTA_DECPORT1_SET_REG       0x14 // SMTA Decode Port1 Set Register
#define SMTA_DECPORT1_CLR_REG       0x18 // SMTA Decode Port1 Clear Register
#define SMTA_DECPORT2_STA_REG       0x1C // SMTA Decode Port2 Status Register
#define SMTA_DECPORT2_SET_REG       0x20 // SMTA Decode Port2 Set Register
#define SMTA_DECPORT2_CLR_REG       0x24 // SMTA Decode Port2 Clear Register




/*
 * Thermal Sensor Controller
 */
#define AW_H3_THS_BASE          0x01C25000

#define THS_CTRL0       0x00 // THS Control Register0
#define THS_CTRL1       0x04 // THS Control Register1
//#define ADC_CDAT        0x14 // ADC calibration data Register
#define THS_CTRL2       0x40 // THS Control Register2
#define THS_INT_CTRL    0x44 // THS Interrupt Control Register
#define THS_STAT        0x48 // THS Status Register
#define THS_ALARM_CTRL  0x50 // Alarm threshold Control Register
#define THS_SHUTDOWN_CTRL   0x60 // Shutdown threshold Control Register
#define THS_FILTER      0x70 // Median filter Control Register
#define THS_CDATA       0X74 // Thermal Sensor Calibration Data
#define THS_DATA        0x80 // THS Data Register




/*
 * UART
 */
#define AW_H3_UART0_BASE        0x01C28000
#define AW_H3_UART1_BASE        0x01C28400
#define AW_H3_UART2_BASE        0x01C28800
#define AW_H3_UART3_BASE        0x01C28C00

#define AW_H3_UART0_IRQ         32
#define AW_H3_UART1_IRQ         33
#define AW_H3_UART2_IRQ         34
#define AW_H3_UART3_IRQ         35

#define AW_H3_UART_SIZE 0x400

#define UART_RBR        0x00 // UART Receive Buffer Register
#define UART_THR        0x00 // UART Transmit Holding Register
#define UART_DLL        0x00 // UART Divisor Latch Low Register
#define UART_DLH        0x04 // UART Divisor Latch High Register
#define UART_IER        0x04 // UART Interrupt Enable Register
#define UART_IIR        0x08 // UART Interrupt Identity Register
#define UART_FCR        0x08 // UART FIFO Control Register
#define UART_LCR        0x0C // UART Line Control Register
#define UART_MCR        0x10 // UART Modem Control Register
#define UART_LSR        0x14 // UART Line Status Register
#define UART_MSR        0x18 // UART Moddem Status Register
#define UART_SCH        0x1C // UART Scratch Register
#define UART_USR        0x7C // USRT Status Register
#define UART_TFL        0x80 // UART Transmit FIFO Level
#define UART_RFL        0x84 // UART_RFL
#define UART_HALT       0xA4 // UART Halt TX Register




/*
 * TWI
 */
#define AW_H3_TWI0_BASE         0x01C2AC00
#define AW_H3_TWI1_BASE         0x01C2B000
#define AW_H3_TWI2_BASE         0x01C2B400

#define TWI_ADDR        0x0000 // TWI Slave address
#define TWI_XADDR       0x0004 // TWI Extended slave address
#define TWI_DATA        0x0008 // TWI Data byte
#define TWI_CNTR        0x000C // TWI Control register
#define TWI_STAT        0x0010 // TWI Status register
#define TWI_CCR         0x0014 // TWI Clock control register
#define TWI_SRST        0x0018 // TWI Software reset
#define TWI_EFR         0x001C // TWI Enhance Feature register
#define TWI_LCR         0x0020 // TWI Line Control register




/*
 * Smart Card Reader
 */
#define AW_H3_SCR_BASE          0x01C2C400

#define SCR_CSR         0x000 // Smart Card Reader Control and Status Register
#define SCR_INTEN       0x004 // Smart Card Reader Interrupt Enable Register 1
#define SCR_INTST       0x008 // Smart Card Reader Interrupt Status Register 1
#define SCR_FCSR        0x00c // Smart Card Reader FIFO Control and Status Register
#define SCR_FCNT        0x010 // mart Card Reader RX and TX FIFO Counter Register
#define SCR_RPT         0x014 // Smart Card Reader RX and TX Repeat Register
#define SCR_DIV         0x018 // Smart Card Reader Clock and Baud Divisor Register
#define SCR_LTIM        0x01c // Smart Card Reader Line Time Register
#define SCR_CTIM        0x020 // Smart Card Reader Character Time Register
#define SCR_LCTLR       0x030 // Smart Card Reader Line Control Register
#define SCR_FIFO        0x100 // Smart Card Reader RX and TX FIFO Access Point




/*
 * EMAC
 */
#define AW_H3_EMAC_BASE         0x01C30000




/*
 * GPU
 */
#define AW_H3_GPU_BASE          0x01C40000




/*
 * High Speed Timer
 */
#define AW_H3_HSTMR_BASE        0x01C60000

#define HS_TMR_IRQ_EN_REG       0x00 // HS Timer IRQ Enable Register
#define HS_TMR_IRQ_STAS_REG     0x04 // HS Timer Status Register
#define HS_TMR_CTRL_REG         0x10 // HS Timer Control Register
#define HS_TMR_INTV_LO_REG      0x14 // HS Timer Interval Value Low Register
#define HS_TMR_INTV_HI_REG      0x18 // HS Timer Interval Value High Register
#define HS_TMR_CURNT_LO_REG     0x1C // HS Timer Current Value Low Register
#define HS_TMR_CURNT_HI_REG     0x20 // HS Timer Current Value High Register




/*
 * DRAM
 */
#define AW_H3_DRAMCOM_BASE      0x01C62000
#define AW_H3_DRAMCTL0_BASE     0x01C63000
#define AW_H3_DRAMPHY0_BASE     0x01C65000




/*
 * SPI
 */
#define AW_H3_SPI0_BASE         0x01C68000
#define AW_H3_SPI1_BASE         0x01C69000

#define SPI_GCR     0x04 // SPI Global Control Register
#define SPI_TCR     0x08 // SPI Transfer Control register
// 0x0c reserved
#define SPI_IER     0x10 // SPI Interrupt Control register
#define SPI_ISR     0x14 // SPI Interrupt Status register
#define PI_FCR      0x18 // SPI FIFO Control register
#define SPI_FSR     0x1C // SPI FIFO Status register
#define SPI_WCR     0x20 // SPI Wait Clock Counter register
#define SPI_CCR     0x24 // SPI Clock Rate Control register
// 0x28 reserved
// 0x2c reserved
#define SPI_MBC     0x30 // SPI Burst Counter register
#define SPI_MTC     0x34 // SPI Transmit Counter Register
#define SPI_BCC     0x38 // SPI Burst Control register
#define SPI_TXD     0x200 // SPI TX Data register
#define SPI_RXD     0x300 // SPI RX Data register




/*
 * CPUCFG
 */
#define AW_H3_CPUCFG_BASE	0x01F01C00

#define CPUS_RST_CTRL_REG	0x0000 // CPUS reset control register

#define CPU0_RST_CTRL		0x0040 // CPU0 reset control
#define CPU0_CTRL_REG		0x0044 // CPU0 control register
#define CPU0_STATUS_REG		0x0048 // CPU0 status register

#define CPU1_RST_CTRL		0x0080 // CPU1 reset control
#define CPU1_CTRL_REG		0x0084 // CPU1 control register
#define CPU1_STATUS_REG		0x0088 // CPU1 status register

#define CPU2_RST_CTRL		0x00C0 // CPU2 reset control
#define CPU2_CTRL_REG		0x00C4 // CPU2 control register
#define CPU2_STATUS_REG		0x00C8 // CPU2 status register

#define CPU3_RST_CTRL		0x0100 // CPU3 reset control
#define CPU3_CTRL_REG		0x0104 // CPU3 control register
#define CPU3_STATUS_REG		0x0108 // CPU3 status register

#define CPU_SYS_RST_REG		0x0140 // CPU System Reset Register
#define CPU_CLK_GATING_REG	0x0144 // CPU clock gating Register
#define GENER_CTRL_REG		0x0184 // General Control Register
#define SUP_STAN_FLAG_REG	0x01A0 // Super Standby Flag Register
#define CNT64_CTRL_REG		0x0280 // 64-bit Counter Control Register
#define CNT64_LOW_REG		0x0284 // 64-bit Counter Low Register
#define CNT64_HIGH_REG		0x0288 // 64-bit Counter High Register

#define CPUCFG_CX_CTRL_REG0(c)		            (0x10 * (c))
#define CPUCFG_CX_CTRL_REG0_L1_RST_DISABLE(n)	BIT(n)
#define CPUCFG_CX_CTRL_REG0_L1_RST_DISABLE_ALL	0xf
#define CPUCFG_CX_CTRL_REG0_L2_RST_DISABLE_A7	BIT(4)
#define CPUCFG_CX_CTRL_REG0_L2_RST_DISABLE_A15	BIT(0)

#define CPUCFG_CX_CTRL_REG1(c)		            (0x10 * (c) + 0x4)
#define CPUCFG_CX_CTRL_REG1_ACINACTM	        BIT(0)
#define CPUCFG_CX_STATUS(c)		                (0x30 + 0x4 * (c))
#define CPUCFG_CX_STATUS_STANDBYWFI(n)	        BIT(16 + (n))
#define CPUCFG_CX_STATUS_STANDBYWFIL2	        BIT(0)

#define CPUCFG_CX_RST_CTRL(c)		            (0x80 + 0x4 * (c))
#define CPUCFG_CX_RST_CTRL_DBG_SOC_RST	        BIT(24)
#define CPUCFG_CX_RST_CTRL_ETM_RST(n)	        BIT(20 + (n))
#define CPUCFG_CX_RST_CTRL_ETM_RST_ALL	        (0xf << 20)
#define CPUCFG_CX_RST_CTRL_DBG_RST(n)	        BIT(16 + (n))
#define CPUCFG_CX_RST_CTRL_DBG_RST_ALL	        (0xf << 16)
#define CPUCFG_CX_RST_CTRL_H_RST	            BIT(12)
#define CPUCFG_CX_RST_CTRL_L2_RST	            BIT(8)
#define CPUCFG_CX_RST_CTRL_CX_RST(n)	        BIT(4 + (n))
#define CPUCFG_CX_RST_CTRL_CORE_RST(n)	        BIT(n)
#define CPUCFG_CX_RST_CTRL_CORE_RST_ALL	        (0xf << 0)

#define CPUCFG_CPU_PWR_CLAMP_STATUS_REG(cpu)	((cpu) * 0x40 + 0x64)
#define CPUCFG_CPU_RST_CTRL_REG(cpu)			(((cpu) + 1) * 0x40)
#define CPUCFG_CPU_STATUS_REG(cpu)				(((cpu) + 1) * 0x40 + 0x08)
#define CPUCFG_CPU_CTRL_REG(cpu)				(((cpu) + 1) * 0x40 + 0x04)
#define CPUCFG_GEN_CTRL_REG			0x184
#define CPUCFG_PRIVATE0_REG			0x1a4
#define CPUCFG_PRIVATE1_REG			0x1a8
#define CPUCFG_DBG_CTL0_REG			0x1e0
#define CPUCFG_DBG_CTL1_REG			0x1e4




/*
 * PRCM
 */
#define AW_H3_PRCM_BASE 					0x01f01400

#define PRCM_CPU_PWROFF_REG					0x100
#define PRCM_CPU_PO_RST_CTRL_CORE_ALL		0xf
#define PRCM_CPU_PO_RST_CTRL_CORE(n)		BIT(n)
#define PRCM_PWROFF_GATING_REG(c)			(0x100 + 0x4 * (c))
#define PRCM_CPU_PO_RST_CTRL(c)				(0x4 + 0x4 * (c))
#define PRCM_CPU_PWR_CLAMP_REG(cpu)			(((cpu) * 4) + 0x140)

/* The power off register for clusters are different from a80 and a83t */
#define PRCM_PWROFF_GATING_REG_CLUSTER_SUN8I	BIT(0)
#define PRCM_PWROFF_GATING_REG_CLUSTER_SUN9I	BIT(4)
#define PRCM_PWROFF_GATING_REG_CORE(n)			BIT(n)
#define PRCM_PWR_SWITCH_REG(c, cpu)			    (0x140 + 0x10 * (c) + 0x4 * (cpu))
#define PRCM_CPU_SOFT_ENTRY_REG				    0x164

/* R_CPUCFG registers, specific to sun8i-a83t */
#define R_CPUCFG_CLUSTER_PO_RST_CTRL(c)		    (0x30 + (c) * 0x4)
#define R_CPUCFG_CLUSTER_PO_RST_CTRL_CORE(n)	BIT(n)
#define R_CPUCFG_CPU_SOFT_ENTRY_REG			    0x01a4

#define CPU0_SUPPORT_HOTPLUG_MAGIC0			    0xFA50392F
#define CPU0_SUPPORT_HOTPLUG_MAGIC1			    0x790DCA3A




/*
 *
 */
#define AW_H3_SCU_BASE          0x01C80000
//#define GIC_DIST                (AW_H3_SCU_BASE + 0x1000)
//#define GIC_CPUIF               (AW_H3_SCU_BASE + 0x2000)



/*
 * CSI
 */
#define AW_H3_CSI_BASE          0x01CB0000

#define CSI0_EN_REG             0X0000 // CSI Enable register
#define CSI0_IF_CFG_REG         0X0004 // CSI Interface Configuration Register
#define CSI0_CAP_REG            0X0008 // CSI Capture Register
#define CSI0_SYNC_CNT_REG       0X000C // CSI Synchronization Counter Register
#define CSI0_FIFO_THRS_REG      0X0010 // CSI FIFO Threshold Register
#define CSI0_PTN_LEN_REG        0X0030 // CSI Pattern Generation Length register
#define CSI0_PTN_ADDR_REG       0X0034 // CSI Pattern Generation Address register
#define CSI0_VER_REG            0X003C // CSI Version Register
#define CSI0_C0_CFG_REG         0X0044 // CSI Channel_0 configuration register
#define CSI0_C0_SCALE_REG       0X004C // CSI Channel_0 scale register
#define CSI0_C0_F0_BUFA_REG     0X0050 // CSI Channel_0 FIFO 0 output buffer-A address register
#define CSI0_C0_F1_BUFA_REG     0X0058 // CSI Channel_0 FIFO 1 output buffer-A address register
#define CSI0_C0_F2_BUFA_REG     0X0060 // CSI Channel_0 FIFO 2 output buffer-A address register
#define CSI0_C0_CAP_STA_REG     0X006C // CSI Channel_0 status register
#define CSI0_C0_INT_EN_REG      0X0070 // CSI Channel_0 interrupt enable register
#define CSI0_C0_INT_STA_REG     0X0074 // CSI Channel_0 interrupt status register
#define CSI0_C0_HSIZE_REG       0X0080 // CSI Channel_0 horizontal size register
#define CSI0_C0_VSIZE_REG       0X0084 // CSI Channel_0 vertical size register
#define CSI0_C0_BUF_LEN_REG     0X0088 // CSI Channel_0 line buffer length register
#define CSI0_C0_FLIP_SIZE_REG   0X008C // CSI Channel_0 flip size register
#define CSI0_C0_FRM_CLK_CNT_REG         0X0090 // CSI Channel_0 frame clock counter register
#define CSI0_C0_ACC_ITNL_CLK_CNT_REG    0X0094 // CSI Channel_0 accumulated and internal clock counter register
#define CSI0_C0_FIFO_STAT_REG           0X0098 // CSI Channel_0 FIFO Statistic Register
#define CSI0_C0_PCLK_STAT_REG           0X009C // CSI Channel_0 PCLK Statistic Register
#define CCI_CTRL                0x3000 // CCI control register
#define CCI_CFG                 0x3004 // CCI transmission config register
#define CCI_FMT                 0x3008 // CCI packet format register
#define CCI_BUS_CTRL            0x300C // CCI bus control register
#define CCI_INT_CTRL            0x3014 // CCI interrupt control register
#define CCI_LC_TRIG             0x3018 // CCI line counter register
#define CCI_FIFO_ACC            0x3100 // CCI FIFO access register
#define CCI_RSV_REG             0x3200 // CCI reserved register



/*
 *
 */
#define AW_H3_TVE_BASE          0x01E00000
#define AW_H3_HDMI_BASE         0x01EE0000




/*
 * Real Time Clock
 */
#define AW_H3_RTC_BASE          0x01F00000

#define LOSC_CTRL_REG           0x0 // Low Oscillator Control Register
#define LOSC_AUTO_SWT_STA_REG   0x4 // LOSC Auto Switch Status Register
#define INTOSC_CLK_PRESCAL_REG  0x8 // Internal OSC Clock Prescalar Register
#define RTC_YY_MM_DD_REG        0x10 // RTC Year-Month-Day Register
#define RTC_HH_MM_SS_REG        0x14 // RTC Hour-Minute-Second Register
#define ALARM0_COUNTER_REG      0x20 // Alarm 0 Counter Register
#define ALARM0_CUR_VLU_REG      0x24 // Alarm 0 Counter Current Value Register
#define ALARM0_ENABLE_REG       0x28 // Alarm 0 Enable Register
#define ALARM0_IRQ_EN           0x2C // Alarm 0 IRQ Enable Register
#define ALARM0_IRQ_STA_REG      0x30 // Alarm 0 IRQ Status Register
#define ALARM1_WK_HH_MM_SS      0x40 // Alarm 1 Week HMS Register
#define ALARM1_ENABLE_REG       0x44 // Alarm 1 Enable Register
#define ALARM1_IRQ_EN           0x48 // Alarm 1 IRQ Enable Register
#define ALARM1_IRQ_STA_REG      0x4C// Alarm 1 IRQ Status Register
#define ALARM_CONFIG_REG        0x50 // Alarm Config Register
#define LOSC_OUT_GATING_REG     0x60 // LOSC output gating register
#define GP_DATA_REG(N)          (0x100 + N*0x4) // General Purpose Register (N=0~7)
#define RTC_DEB_REG             0x170 // RTC Debug Register
#define GPL_HOLD_OUTPUT_REG     0x180 // GPL Hold Output Register
#define VDD_RTC_REG             0x190 // VDD RTC Regulate Register
#define IC_CHARA_REG            0x1F0 // IC Characteristic Register




#define AW_H3_CoreSight_Debug_BASE      0x3F500000
#define AW_H3_TSGEN_RO_BASE             0x3F50 6000
#define AW_H3_TSGEN_CTRL_BASE           0x3F507000
#define AW_H3_RAM_BASE                  0x40000000




/*
 * GIC Interrupt Source
 */
#define AW_H3_GIC_BASE              0x01C81000

#define AW_H3_GICD_BASE             0x01C81000
#define AW_H3_GICC_BASE             0x01C82000
// Page 206




#endif /* __AW_H3_H__ */