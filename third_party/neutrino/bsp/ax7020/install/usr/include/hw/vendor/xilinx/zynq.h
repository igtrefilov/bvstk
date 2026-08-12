/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __HW_VENDOR_XILINX_ZYNQ_H__
#define __HW_VENDOR_XILINX_ZYNQ_H__

#ifndef __ASSEMBLER__
#include <stdint.h>
#endif /* __ASSEMBLER__ */

/* ----------------------------------------------------------------------------
 * Devices
 * ----------------------------------------------------------------------------
 */

#define XZYNQ_HWI_UART "uart"
#define XZYNQ_HWI_WDT  "wdt"
#define XZYNQ_HWI_DMA  "dma"

/* ----------------------------------------------------------------------------
 * UART
 * ----------------------------------------------------------------------------
 */

#define XZYNQ_XUARTPS_UART_SIZE 0x1000


/* UART registers */

#define XZYNQ_XUARTPS_CR_REG      0x00 /* UART Control register */
#define XZYNQ_XUARTPS_MR_REG      0x04 /* UART Mode register */
#define XZYNQ_XUARTPS_IER_REG     0x08 /* Interrupt Enable register */
#define XZYNQ_XUARTPS_IDR_REG     0x0C /* Interrupt Disable register */
#define XZYNQ_XUARTPS_IMR_REG     0x10 /* Interrupt Mask register */
#define XZYNQ_XUARTPS_ISR_REG     0x14 /* Channel Interrupt Status register*/
#define XZYNQ_XUARTPS_BAUDGEN_REG 0x18 /* Baud Rate Generator register */
#define XZYNQ_XUARTPS_RXTOUT_REG  0x1C /* Receiver Timeout register */
#define XZYNQ_XUARTPS_RXWM_REG    0x20 /* Receiver FIFO Trigger Level register */
#define XZYNQ_XUARTPS_MODEMCR_REG 0x24 /* Modem Control register */
#define XZYNQ_XUARTPS_MODEMSR_REG 0x28 /* Modem Status register */
#define XZYNQ_XUARTPS_SR_REG      0x2C /* Channel Status register */
#define XZYNQ_XUARTPS_FIFO_REG    0x30 /* Transmit and Receive FIFO register */
#define XZYNQ_XUARTPS_BAUDDIV_REG 0x34 /* Baud Rate Divider Register */
#define XZYNQ_XUARTPS_FLOWDEL_REG 0x38 /* Flow Control Delay register */
#define XZYNQ_XUARTPS_TXWM_REG    0x44 /* Transmitter FIFO Trigger Level register */


/* UART Control register */

#define XZYNQ_XUARTPS_CR_STOPBRK     0x00000100 /* Stop transmission of break */
#define XZYNQ_XUARTPS_CR_STARTBRK    0x00000080 /* Set break */
#define XZYNQ_XUARTPS_CR_TORST       0x00000040 /* RX timeout counter restart */
#define XZYNQ_XUARTPS_CR_TX_DIS      0x00000020 /* TX disabled */
#define XZYNQ_XUARTPS_CR_TX_EN       0x00000010 /* TX enabled */
#define XZYNQ_XUARTPS_CR_RX_DIS      0x00000008 /* RX disabled */
#define XZYNQ_XUARTPS_CR_RX_EN       0x00000004 /* RX enabled */
#define XZYNQ_XUARTPS_CR_EN_DIS_MASK 0x0000003C /* Enable/disable Mask */
#define XZYNQ_XUARTPS_CR_TXRST       0x00000002 /* TX logic reset */
#define XZYNQ_XUARTPS_CR_RXRST       0x00000001 /* RX logic reset */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rxrst    : 1,
            txrst    : 1,
            rx_en    : 1,
            rx_dis   : 1,
            tx_en    : 1,
            tx_dis   : 1,
            torst    : 1,
            startbrk : 1,
            stopbrk  : 1,
            reserved : 23;
    } __attribute__((packed)) bits;
} xzynq_xuartps_cr_reg_t;
#endif /* __ASSEMBLER__ */


/* UART Mode register */

#define XZYNQ_XUARTPS_MR_CHMODE_R_LOOP    0x00000300 /* Remote loopback mode */
#define XZYNQ_XUARTPS_MR_CHMODE_L_LOOP    0x00000200 /* Local loopback mode */
#define XZYNQ_XUARTPS_MR_CHMODE_ECHO      0x00000100 /* Auto echo mode */
#define XZYNQ_XUARTPS_MR_CHMODE_NORM      0x00000000 /* Normal mode */
#define XZYNQ_XUARTPS_MR_CHMODE_SHIFT     8          /* Mode shift */
#define XZYNQ_XUARTPS_MR_CHMODE_MASK      0x00000300 /* Mode mask */
#define XZYNQ_XUARTPS_MR_STOPMODE_2_BIT   0x00000080 /* 2 stop bits */
#define XZYNQ_XUARTPS_MR_STOPMODE_1_5_BIT 0x00000040 /* 1.5 stop bits */
#define XZYNQ_XUARTPS_MR_STOPMODE_1_BIT   0x00000000 /* 1 stop bit */
#define XZYNQ_XUARTPS_MR_STOPMODE_SHIFT   6          /* Stop bits setting shift */
#define XZYNQ_XUARTPS_MR_STOPMODE_MASK    0x000000C0 /* Stop bits setting mask */
#define XZYNQ_XUARTPS_MR_PARITY_NONE      0x00000020 /* No parity mode */
#define XZYNQ_XUARTPS_MR_PARITY_MARK      0x00000018 /* Mark parity mode */
#define XZYNQ_XUARTPS_MR_PARITY_SPACE     0x00000010 /* Space parity mode */
#define XZYNQ_XUARTPS_MR_PARITY_ODD       0x00000008 /* Odd parity mode */
#define XZYNQ_XUARTPS_MR_PARITY_EVEN      0x00000000 /* Even parity mode */
#define XZYNQ_XUARTPS_MR_PARITY_SHIFT     3          /* Parity setting shift */
#define XZYNQ_XUARTPS_MR_PARITY_MASK      0x00000038 /* Parity mask */
#define XZYNQ_XUARTPS_MR_CHARLEN_6_BIT    0x00000006 /* 6 bits data */
#define XZYNQ_XUARTPS_MR_CHARLEN_7_BIT    0x00000004 /* 7 bits data */
#define XZYNQ_XUARTPS_MR_CHARLEN_8_BIT    0x00000000 /* 8 bits data */
#define XZYNQ_XUARTPS_MR_CHARLEN_SHIFT    1          /* Data length setting shift */
#define XZYNQ_XUARTPS_MR_CHARLEN_MASK     0x00000006 /* Data length mask */
#define XZYNQ_XUARTPS_MR_CLKSEL           0x00000001 /* Input clock selection */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clksel      : 1,
            chrl        : 2,
            par         : 3,
            nbstop      : 2,
            chmode      : 2,
            reserved_rw : 2, /* do not modify */
            reserved    : 20;
    } __attribute__((packed)) bits;
} xzynq_xuartps_mr_reg_t;
#endif /* __ASSEMBLER__ */


/* Interrupt registers */

#define XZYNQ_XUARTPS_IXR_TXOVR   0x00001000 /* TX Overrun error interrupt */
#define XZYNQ_XUARTPS_IXR_TXNFULL 0x00000800 /* TX FIFO near full interrupt */
#define XZYNQ_XUARTPS_IXR_TRIG    0x00000400 /* TX FIFO trigger interrupt */
#define XZYNQ_XUARTPS_IXR_DMS     0x00000200 /* Modem status change interrupt */
#define XZYNQ_XUARTPS_IXR_TOUT    0x00000100 /* Timeout error interrupt */
#define XZYNQ_XUARTPS_IXR_PARITY  0x00000080 /* Parity error interrupt */
#define XZYNQ_XUARTPS_IXR_FRAMING 0x00000040 /* Framing error interrupt */
#define XZYNQ_XUARTPS_IXR_RXOVR   0x00000020 /* RX Overrun error interrupt */
#define XZYNQ_XUARTPS_IXR_TXFULL  0x00000010 /* TX FIFO full interrupt */
#define XZYNQ_XUARTPS_IXR_TXEMPTY 0x00000008 /* TX FIFO empty interrupt */
#define XZYNQ_XUARTPS_IXR_RXFULL  0x00000004 /* RX FIFO full interrupt */
#define XZYNQ_XUARTPS_IXR_RXEMPTY 0x00000002 /* RX FIFO empty interrupt */
#define XZYNQ_XUARTPS_IXR_RTRIG   0x00000001 /* RX FIFO trigger interrupt */
#define XZYNQ_XUARTPS_IXR_MASK    0x00001FFF /* Valid bit mask */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ixr_rxovr   : 1,
            ixr_rxempty : 1,
            ixr_rxfull  : 1,
            ixr_txempty : 1,
            ixr_txfull  : 1,
            ixr_over    : 1,
            ixr_framing : 1,
            ixr_parity  : 1,
            ixr_tout    : 1,
            ixr_dms     : 1,
            ttrig       : 1,
            tnful       : 1,
            tovr        : 1,
            reserved    : 19;
    } __attribute__((packed)) bits;
} xzynq_xuartps_ir_reg_t;

typedef xzynq_xuartps_ir_reg_t xzynq_xuartps_ier_reg_t;
typedef xzynq_xuartps_ir_reg_t xzynq_xuartps_idr_reg_t;
typedef xzynq_xuartps_ir_reg_t xzynq_xuartps_imr_reg_t;
typedef xzynq_xuartps_ir_reg_t xzynq_xuartps_isr_reg_t;
#endif /* __ASSEMBLER__ */


/* Baud Rate Generator register */

#define XZYNQ_XUARTPS_BAUDGEN_DISABLE 0x00000000 /* Disable clock */
#define XZYNQ_XUARTPS_BAUDGEN_MASK    0x0000FFFF /* Valid bits mask */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            cd       : 16,
            reserved : 16;
    } __attribute__((packed)) bits;
} xzynq_xuartps_baudgen_reg_t;
#endif /* __ASSEMBLER__ */


/* Receiver Timeout register */

#define XZYNQ_XUARTPS_RXTOUT_DISABLE 0x00000000 /* Disable time out */
#define XZYNQ_XUARTPS_RXTOUT_MASK    0x000000FF /* Valid bits mask */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rto      : 8,
            reserved : 24;
    } __attribute__((packed)) bits;
} xzynq_xuartps_rxtout_reg_t;
#endif /* __ASSEMBLER__ */


/* Receiver FIFO Trigger Level register */

#define XZYNQ_XUARTPS_RXWM_DISABLE 0x00000000 /* Disable RX trigger interrupt */
#define XZYNQ_XUARTPS_RXWM_MASK    0x0000003F /* Valid bits mask */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rtrig    : 6,
            reserved : 26;
    } __attribute__((packed)) bits;
} xzynq_xuartps_rxwm_reg_t;
#endif /* __ASSEMBLER__ */


/* Modem Control register */

#define XZYNQ_XUARTPS_MODEMCR_FCM 0x00000020 /* Flow control mode */
#define XZYNQ_XUARTPS_MODEMCR_RTS 0x00000002 /* Request to send */
#define XZYNQ_XUARTPS_MODEMCR_DTR 0x00000001 /* Data terminal ready */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dtr        : 1,
            rts        : 1,
            reserved_1 : 3,
            fcm        : 1,
            reserved_2 : 26;
    } __attribute__((packed)) bits;
} xzynq_xuartps_modemcr_reg_t;
#endif /* __ASSEMBLER__ */


/* Modem Status register */

#define XZYNQ_XUARTPS_MODEMSR_FCMS 0x00000100 /* Flow control mode (FCMS) */
#define XZYNQ_XUARTPS_MODEMSR_DCD  0x00000080 /* Complement of DCD input */
#define XZYNQ_XUARTPS_MODEMSR_RI   0x00000040 /* Complement of RI input */
#define XZYNQ_XUARTPS_MODEMSR_DSR  0x00000020 /* Complement of DSR input */
#define XZYNQ_XUARTPS_MODEMSR_CTS  0x00000010 /* Complement of CTS input */
#define XZYNQ_XUARTPS_MODEMSR_DCDX 0x00000008 /* Delta DCD indicator */
#define XZYNQ_XUARTPS_MODEMSR_RIX  0x00000004 /* Change of RI */
#define XZYNQ_XUARTPS_MODEMSR_DSRX 0x00000002 /* Change of DSR */
#define XZYNQ_XUARTPS_MODEMSR_CTSX 0x00000001 /* Change of CTS */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            medemsr_ctsx : 1,
            medemsr_dsrx : 1,
            medemsr_rix  : 1,
            medemsr_dcdx : 1,
            cts          : 1,
            dsr          : 1,
            ri           : 1,
            dcd          : 1,
            fcms         : 1,
            reserved     : 23;
    } __attribute__((packed)) bits;
} xzynq_xuartps_modemsr_reg_t;
#endif /* __ASSEMBLER__ */


/* Channel Status register */

#define XZYNQ_XUARTPS_SR_FLOWDEL 0x00001000 /* RX FIFO fill over flow delay */
#define XZYNQ_XUARTPS_SR_TACTIVE 0x00000800 /* TX active */
#define XZYNQ_XUARTPS_SR_RACTIVE 0x00000400 /* RX active */
#define XZYNQ_XUARTPS_SR_TOUT    0x00000100 /* RX timeout */
#define XZYNQ_XUARTPS_SR_TXFULL  0x00000010 /* TX FIFO full */
#define XZYNQ_XUARTPS_SR_TXEMPTY 0x00000008 /* TX FIFO empty */
#define XZYNQ_XUARTPS_SR_RXFULL  0x00000004 /* RX FIFO full */
#define XZYNQ_XUARTPS_SR_RXEMPTY 0x00000002 /* RX FIFO empty */
#define XZYNQ_XUARTPS_SR_RXOVR   0x00000001 /* RX FIFO fill over trigger */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rxovr       : 1,
            rxempty     : 1,
            rxfull      : 1,
            txempty     : 1,
            txfull      : 1,
            reserved_ro : 5, /* do not modify */
            ractive     : 1,
            tactive     : 1,
            flowdel     : 1,
            ttrig       : 1,
            tnful       : 1,
            reserved    : 17;
    } __attribute__((packed)) bits;
} xzynq_xuartps_sr_reg_t;
#endif /* __ASSEMBLER__ */


/* Transmit and Receive FIFO register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            fifo     : 8,
            reserved : 24;
    } __attribute__((packed)) bits;
} xzynq_xuartps_fifo_reg_t;
#endif /* __ASSEMBLER__ */


/* Baud Rate Divider Register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            bdiv     : 8,
            reserved : 24;
    } __attribute__((packed)) bits;
} xzynq_xuartps_bauddiv_reg_t;
#endif /* __ASSEMBLER__ */


/* Flow Control Delay register */

#define XZYNQ_XUARTPS_FLOWDEL_MASK 0x0000003F /* Valid bit mask */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            fdel     : 6,
            reserved : 26;
    } __attribute__((packed)) bits;
} xzynq_xuartps_flowdel_reg_t;
#endif /* __ASSEMBLER__ */


/* Transmitter FIFO Trigger Level register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ttrig    : 6,
            reserved : 26;
    } __attribute__((packed)) bits;
} xzynq_xuartps_txwm_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * CAN
 * ----------------------------------------------------------------------------
 */

#define XZYNQ_CAN_REG_SIZE 0x84

#define XZYNQ_XCANPS_SRR_OFFSET        0x00
#define XZYNQ_XCANPS_MSR_OFFSET        0x04
#define XZYNQ_XCANPS_BRPR_OFFSET       0x08
#define XZYNQ_XCANPS_BTR_OFFSET        0x0C
#define XZYNQ_XCANPS_ECR_OFFSET        0x10
#define XZYNQ_XCANPS_ESR_OFFSET        0x14
#define XZYNQ_XCANPS_SR_OFFSET         0x18
#define XZYNQ_XCANPS_ISR_OFFSET        0x1C
#define XZYNQ_XCANPS_IER_OFFSET        0x20
#define XZYNQ_XCANPS_ICR_OFFSET        0x24
#define XZYNQ_XCANPS_TCR_OFFSET        0x28
#define XZYNQ_XCANPS_WIR_OFFSET        0x2C
#define XZYNQ_XCANPS_TXFIFO_ID_OFFSET  0x30
#define XZYNQ_XCANPS_TXFIFO_DLC_OFFSET 0x34
#define XZYNQ_XCANPS_TXFIFO_DW1_OFFSET 0x38
#define XZYNQ_XCANPS_TXFIFO_DW2_OFFSET 0x3C
#define XZYNQ_XCANPS_TXHPB_ID_OFFSET   0x40
#define XZYNQ_XCANPS_TXHPB_DLC_OFFSET  0x44
#define XZYNQ_XCANPS_TXHPB_DW1_OFFSET  0x48
#define XZYNQ_XCANPS_TXHPB_DW2_OFFSET  0x4C
#define XZYNQ_XCANPS_RXFIFO_ID_OFFSET  0x50
#define XZYNQ_XCANPS_RXFIFO_DLC_OFFSET 0x54
#define XZYNQ_XCANPS_RXFIFO_DW1_OFFSET 0x58
#define XZYNQ_XCANPS_RXFIFO_DW2_OFFSET 0x5C
#define XZYNQ_XCANPS_AFR_OFFSET        0x60
#define XZYNQ_XCANPS_AFMR1_OFFSET      0x64
#define XZYNQ_XCANPS_AFIR1_OFFSET      0x68
#define XZYNQ_XCANPS_AFMR2_OFFSET      0x6C
#define XZYNQ_XCANPS_AFIR2_OFFSET      0x70
#define XZYNQ_XCANPS_AFMR3_OFFSET      0x74
#define XZYNQ_XCANPS_AFIR3_OFFSET      0x78
#define XZYNQ_XCANPS_AFMR4_OFFSET      0x7C
#define XZYNQ_XCANPS_AFIR4_OFFSET      0x80

/* Software Reset register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            srst     : 1,
            cen      : 1,
            reserved : 30;
    } __attribute__((packed)) bits;
} xzynq_xcanps_srr_reg_t;
#endif /* __ASSEMBLER__ */


/* Mode Select register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            sleep    : 1,
            lback    : 1,
            snoop    : 1,
            reserved : 29;
    } __attribute__((packed)) bits;
} xzynq_xcanps_msr_reg_t;
#endif /* __ASSEMBLER__ */


/* Baud Rate Prescaler register */

#define XZYNQ_CAN_BRPR_MAXVAL 255

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            brp      : 8,
            reserved : 24;
    } __attribute__((packed)) bits;
} xzynq_xcanps_brpr_reg_t;
#endif /* __ASSEMBLER__ */


/* Bit Timing register */

/* Maxval timing values */
#define XZYNQ_CAN_BTR_TS1_MAXVAL 15
#define XZYNQ_CAN_BTR_TS2_MAXVAL 7
#define XZYNQ_CAN_BTR_SJW_MAXVAL 3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ts1      : 4,
            ts2      : 3,
            sjw      : 2,
            reserved : 23;
    } __attribute__((packed)) bits;
} xzynq_xcanps_btr_reg_t;
#endif /* __ASSEMBLER__ */


/* Error Counter register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            tec      : 8,
            rec      : 8,
            reserved : 16;
    } __attribute__((packed)) bits;
} xzynq_xcanps_ecr_reg_t;
#endif /* __ASSEMBLER__ */


/* Error Status register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            crcer    : 1,
            fmer     : 1,
            ster     : 1,
            berr     : 1,
            acker    : 1,
            reserved : 27;
    } __attribute__((packed)) bits;
} xzynq_xcanps_esr_reg_t;
#endif /* __ASSEMBLER__ */


/* Status register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            config   : 1,
            lback    : 1,
            sleep    : 1,
            normal   : 1,
            bidle    : 1,
            bbsy     : 1,
            errwrn   : 1,
            estat    : 2,
            txbfll   : 1,
            txfll    : 1,
            acfbsy   : 1,
            snoop    : 1,
            reserved : 19;
    } __attribute__((packed)) bits;
} xzynq_xcanps_sr_reg_t;
#endif /* __ASSEMBLER__ */


/* Interrupt registers */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ixr_arblst   : 1,
            ixr_txok     : 1,
            ixr_txfll    : 1,
            ixr_txbfll   : 1,
            ixr_rxok     : 1,
            ixr_rxuflw   : 1,
            ixr_rxoflw   : 1,
            ixr_rxnemp   : 1,
            ixr_error    : 1,
            ixr_bsoff    : 1,
            ixr_slp      : 1,
            ixr_wkup     : 1,
            ixr_rxfwmfll : 1,
            ixr_txfwmemp : 1,
            ixr_txfemp   : 1,
            reserved     : 17;
    } __attribute__((packed)) bits;
} xzynq_xcanps_ixr_reg_t;

typedef xzynq_xcanps_ixr_reg_t xzynq_xcanps_isr_reg_t;
typedef xzynq_xcanps_ixr_reg_t xzynq_xcanps_ier_reg_t;
typedef xzynq_xcanps_ixr_reg_t xzynq_xcanps_icr_reg_t;
#endif /* __ASSEMBLER__ */


/* Timestamp Control register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            cts      : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} xzynq_xcanps_tcr_reg_t;
#endif /* __ASSEMBLER__ */


/* Watermark Interrupt register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            fw       : 8,
            ew       : 8,
            reserved : 16;
    } __attribute__((packed)) bits;
} xzynq_xcanps_wir_reg_t;
#endif /* __ASSEMBLER__ */


/* Fifo message and high priority buffer identifier registers */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            idr_rtr : 1,
            idr_id2 : 18,
            idr_ide : 1,
            idr_srr : 1,
            idr_id1 : 11;
    } __attribute__((packed)) bits;
} xzynq_xcanps_buf_id_reg_t;

typedef xzynq_xcanps_buf_id_reg_t xzynq_xcanps_txfifo_id_reg_t;
typedef xzynq_xcanps_buf_id_reg_t xzynq_xcanps_rxfifo_id_reg_t;

typedef xzynq_xcanps_buf_id_reg_t xzynq_xcanps_txhpb_id_reg_t;
#endif /* __ASSEMBLER__ */


/* Transmit message fifo and high priority buffer data length code register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            reserved : 28,
            dlcr_dlc : 4;
    } __attribute__((packed)) bits;
} xzynq_xcanps_buf_dlc_reg_t;

typedef xzynq_xcanps_buf_dlc_reg_t xzynq_xcanps_txfifo_dlc_reg_t;
typedef xzynq_xcanps_buf_dlc_reg_t xzynq_xcanps_txhpb_dlc_reg_t;
#endif /* __ASSEMBLER__ */


/* Message fifo and high priority buffer data word 1 register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dw1r_db3 : 8,
            dw1r_db2 : 8,
            dw1r_db1 : 8,
            dw1r_db0 : 8;
    } __attribute__((packed)) bits;
} xzynq_xcanps_buf_dw1_reg_t;

typedef xzynq_xcanps_buf_dw1_reg_t xzynq_xcanps_txfifo_dw1_reg_t;
typedef xzynq_xcanps_buf_dw1_reg_t xzynq_xcanps_rxfifo_dw1_reg_t;

typedef xzynq_xcanps_buf_dw1_reg_t xzynq_xcanps_txhpb_dw1_reg_t;
#endif /* __ASSEMBLER__ */


/* Message fifo data word 2 register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            dw2r_db7 : 8,
            dw2r_db6 : 8,
            dw2r_db5 : 8,
            dw2r_db4 : 8;
    } __attribute__((packed)) bits;
} xzynq_xcanps_buf2_dw2_reg_t;

typedef xzynq_xcanps_buf2_dw2_reg_t xzynq_xcanps_txfifo_dw2_reg_t;
typedef xzynq_xcanps_buf2_dw2_reg_t xzynq_xcanps_rxfifo_dw2_reg_t;

typedef xzynq_xcanps_buf2_dw2_reg_t xzynq_xcanps_txhpb_dw2_reg_t;
#endif /* __ASSEMBLER__ */


/* Receive message fifo data length code register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rxt      : 16,
            reserved : 12,
            dlcr_dlc : 4;
    } __attribute__((packed)) bits;
} xzynq_xcanps_rxfifo_dlc_reg_t;
#endif /* __ASSEMBLER__ */


/* Acceptance Filter register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            uaf1     : 1,
            uaf2     : 1,
            uaf3     : 1,
            uaf4     : 1,
            dlcr_dlc : 28;
    } __attribute__((packed)) bits;
} xzynq_xcanps_afr_reg_t;
#endif /* __ASSEMBLER__ */


/* Acceptance Filter Mask and ID registers */

#define XZYNQ_CAN_EXTENDED_ID_WIDTH 18
#define XZYNQ_CAN_STANDARD_ID_WIDTH 11
#define XZYNQ_CAN_FULL_ID_WIDTH     (XZYNQ_CAN_EXTENDED_ID_WIDTH + \
                                 XZYNQ_CAN_STANDARD_ID_WIDTH)

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            artr : 1,
            aidl : 18,
            aide : 1,
            asrr : 1,
            aidh : 11;
    } __attribute__((packed)) bits;
} xzynq_xcanps_afmir_reg_t;

typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afmr1_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afmr2_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afmr3_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afmr4_reg_t;

typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afir1_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afir2_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afir3_reg_t;
typedef xzynq_xcanps_afmir_reg_t xzynq_xcanps_afir4_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * I2C
 * ----------------------------------------------------------------------------
 */

#define XZYNQ_XIICPS_MAX_SPEED 400000 /* 400 kB/s */

#define XZYNQ_XIICPS_FIFO_DEPTH 16 /* FIFO size in bytes */

#define XZYNQ_XIICPS_DIV_A_COEF 22 /* Divisor A multiplier coefficient */
#define XZYNQ_XIICPS_DIV_A_MIN  1  /* Minimum value of clock divisor A */
#define XZYNQ_XIICPS_DIV_A_MAX  4  /* Maximum value of clock divisor A */
#define XZYNQ_XIICPS_DIV_B_MIN  1  /* Minimum value of clock divisor B */
#define XZYNQ_XIICPS_DIV_B_MAX  64 /* Maximum value of clock divisor B */


/* I2C registers offsets */

#define XZYNQ_XIICPS_CR_OFFSET         0x00 /* Control register */
#define XZYNQ_XIICPS_SR_OFFSET         0x04 /* Status register */
#define XZYNQ_XIICPS_ADDR_OFFSET       0x08 /* IIC Address register */
#define XZYNQ_XIICPS_DATA_OFFSET       0x0C /* IIC Data register */
#define XZYNQ_XIICPS_ISR_OFFSET        0x10 /* IIC Interrupt Status register */
#define XZYNQ_XIICPS_TRANS_SIZE_OFFSET 0x14 /* Transfer Size register */
#define XZYNQ_XIICPS_SLV_PAUSE_OFFSET  0x18 /* Slave Monitor Pause register */
#define XZYNQ_XIICPS_TIME_OUT_OFFSET   0x1C /* Time Out register */
#define XZYNQ_XIICPS_IMR_OFFSET        0x20 /* Interrupt Mask register */
#define XZYNQ_XIICPS_IER_OFFSET        0x24 /* Interrupt Enable register */
#define XZYNQ_XIICPS_IDR_OFFSET        0x28 /* Interrupt Disable register */


/* Control register */

#define XZYNQ_XIICPS_CR_DIV_A       0xC000 /* Clock divisor A */
#define XZYNQ_XIICPS_CR_DIV_A_SHIFT 14     /* Clock divisor A shift */
#define XZYNQ_XIICPS_CR_DIV_B       0x3F00 /* Clock divisor B */
#define XZYNQ_XIICPS_CR_DIV_B_SHIFT 8      /* Clock divisor B shift */
#define XZYNQ_XIICPS_CR_CLR_FIFO    0x0040 /* Clear FIFO, auto clears */
#define XZYNQ_XIICPS_CR_SLVMON      0x0020 /* Slave monitor mode */
#define XZYNQ_XIICPS_CR_HOLD        0x0010 /* Hold bus */
#define XZYNQ_XIICPS_CR_ACKEN       0x0008 /* Transmit ACK */
#define XZYNQ_XIICPS_CR_NEA         0x0004 /* Addressing mode */
#define XZYNQ_XIICPS_CR_MS          0x0002 /* Overall interface mode */
#define XZYNQ_XIICPS_CR_RD_WR       0x0001 /* Direction of transfer */

#ifndef __ASSEMBLER__
typedef union {
    uint16_t raw;
    struct {
        uint16_t
            rd_wr    : 1,
            ms       : 1,
            nea      : 1,
            acken    : 1,
            hold     : 1,
            slvmon   : 1,
            clr_fifo : 1,
            reserved : 1,
            div_b    : 6,
            div_a    : 2;
    } __attribute__((packed)) bits;
} xzynq_xiicps_cr_reg_t;
#endif /* __ASSEMBLER__ */


/* Status register */

#define XZYNQ_XIICPS_SR_BA    0x0100 /* Bus Active */
#define XZYNQ_XIICPS_SR_RXOVF 0x0080 /* Receiver Overflow */
#define XZYNQ_XIICPS_SR_TXDV  0x0040 /* Transmit Data Valid */
#define XZYNQ_XIICPS_SR_RXDV  0x0020 /* Receiver Data Valid */
#define XZYNQ_XIICPS_SR_RXRW  0x0008 /* Receive read/write */

#ifndef __ASSEMBLER__
typedef union {
    uint16_t raw;
    struct {
        uint16_t
            reserved_1 : 3,
            rxrw       : 1,
            reserved_2 : 1,
            rxdv       : 1,
            txdv       : 1,
            rxovf      : 1,
            ba         : 1,
            reserved_3 : 7;
    } __attribute__((packed)) bits;
} xzynq_xiicps_sr_reg_t;
#endif /* __ASSEMBLER__ */


/* Address register */

#define XZYNQ_XIICPS_ADDR 0x03FF /* IIC address */

#ifndef __ASSEMBLER__
typedef union {
    uint16_t raw;
    struct {
        uint16_t
            addr     : 10,
            reserved : 6;
    } __attribute__((packed)) bits;
} xzynq_xiicps_addr_reg_t;
#endif /* __ASSEMBLER__ */


/* Data register */

#define XZYNQ_XIICPS_DATA 0x00FF /* Data */

#ifndef __ASSEMBLER__
typedef union {
    uint16_t raw;
    struct {
        uint16_t
            data     : 8,
            reserved : 8;
    } __attribute__((packed)) bits;
} xzynq_xiicps_data_reg_t;
#endif /* __ASSEMBLER__ */


/* Interrupt registers */

#define XZYNQ_XIICPS_IXR_ALL_INTR_MASK 0x02FF /* All interrupts mask */
#define XZYNQ_XIICPS_IXR_ARB_LOST      0x0200 /* Arbitration lost interrupt */
#define XZYNQ_XIICPS_IXR_RX_UNF        0x0080 /* FIFO receive underflow interrupt */
#define XZYNQ_XIICPS_IXR_TX_OVR        0x0040 /* FIFO transmit overflow interrupt */
#define XZYNQ_XIICPS_IXR_RX_OVR        0x0020 /* Receive overflow interrupt */
#define XZYNQ_XIICPS_IXR_SLV_RDY       0x0010 /* Monitored slave ready interrupt */
#define XZYNQ_XIICPS_IXR_TO            0x0008 /* Transfer time out interrupt */
#define XZYNQ_XIICPS_IXR_NACK          0x0004 /* NACK interrupt */
#define XZYNQ_XIICPS_IXR_DATA          0x0002 /* Data interrupt */
#define XZYNQ_XIICPS_IXR_COMP          0x0001 /* Transfer complete interrupt */

#ifndef __ASSEMBLER__
typedef union {
    uint16_t raw;
    struct {
        uint16_t
            ixr_comp     : 1,
            ixr_data     : 1,
            ixr_nack     : 1,
            ixr_to       : 1,
            ixr_slv_rdy  : 1,
            ixr_rx_ovr   : 1,
            ixr_tx_ovr   : 1,
            ixr_rx_unf   : 1,
            reserved_1   : 1,
            ixr_arb_lost : 1,
            reserved_2   : 6;
    } __attribute__((packed)) bits;
} xzynq_xiicps_ir_reg_t;

typedef xzynq_xiicps_ir_reg_t xzynq_xiicps_isr_reg_t;
typedef xzynq_xiicps_ir_reg_t xzynq_xiicps_imr_reg_t;
typedef xzynq_xiicps_ir_reg_t xzynq_xiicps_ier_reg_t;
typedef xzynq_xiicps_ir_reg_t xzynq_xiicps_idr_reg_t;
#endif /* __ASSEMBLER__ */


/* Transfer Size register */

#define XZYNQ_XIICPS_TRANS_SIZE 0xFF /* Transfer size */

#ifndef __ASSEMBLER__
typedef union {
    uint8_t raw;
    struct {
        uint8_t
            size : 8;
    } __attribute__((packed)) bits;
} xzynq_xiicps_trans_size_reg_t;
#endif /* __ASSEMBLER__ */


/* Slave Monitor Pause register */

#define XZYNQ_XIICPS_SLV_PAUSE 0x0F /* Pause interval */

#ifndef __ASSEMBLER__
typedef union {
    uint8_t raw;
    struct {
        uint8_t
            pause    : 4,
            reserved : 4;
    } __attribute__((packed)) bits;
} xzynq_xiicps_slv_pause_reg_t;
#endif /* __ASSEMBLER__ */


/* Time Out register */

#define XZYNQ_XIICPS_TIME_OUT 0xFF /* Time Out */

#ifndef __ASSEMBLER__
typedef union {
    uint8_t raw;
    struct {
        uint8_t
            timeout : 8;
    } __attribute__((packed)) bits;
} xzynq_xiicps_time_out_reg_t;
#endif /* __ASSEMBLER__ */


/* ----------------------------------------------------------------------------
 * SPI
 * ----------------------------------------------------------------------------
 */

#define XZYNQ_XSPIPS_FIFO_DEPTH 128

#define XZYNQ_XSPIPS_DATA_FRAME_WIDTH 8 /* in bits */


/* SPI registers offsets */

#define XZYNQ_XSPIPS_CR_OFFSET     0x00 /* Configuration register */
#define XZYNQ_XSPIPS_ISR_OFFSET    0x04 /* Interrupt Status register */
#define XZYNQ_XSPIPS_IER_OFFSET    0x08 /* Interrupt Enable register */
#define XZYNQ_XSPIPS_IDR_OFFSET    0x0C /* Interrupt Disable register */
#define XZYNQ_XSPIPS_IMR_OFFSET    0x10 /* Interrupt Mask register */
#define XZYNQ_XSPIPS_EN_OFFSET     0x14 /* Controller Enable register */
#define XZYNQ_XSPIPS_DLY_OFFSET    0x18 /* Delay Control register */
#define XZYNQ_XSPIPS_TXD_OFFSET    0x1C /* Transmit Data register */
#define XZYNQ_XSPIPS_RXD_OFFSET    0x20 /* Receive Data register */
#define XZYNQ_XSPIPS_SICR_OFFSET   0x24 /* Slave Idle Count register */
#define XZYNQ_XSPIPS_TXWR_OFFSET   0x28 /* Tx FIFO threshold */
#define XZYNQ_XSPIPS_RXWR_OFFSET   0x2C /* Rx FIFO threshold */
#define XZYNQ_XSPIPS_MOD_ID_OFFSET 0xFC /* Module ID register */


/* Configuration register */

#define XZYNQ_XSPIPS_CR_MODE_SEL_SLAVE  0
#define XZYNQ_XSPIPS_CR_MODE_SEL_MASTER 1

#define XZYNQ_XSPIPS_CR_CS_SELECT_NONE 0xF
#define XZYNQ_XSPIPS_CR_CS_SELECT_0    0x0
#define XZYNQ_XSPIPS_CR_CS_SELECT_1    0x1
#define XZYNQ_XSPIPS_CR_CS_SELECT_2    0x3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            mode_sel        : 1,
            clk_pol         : 1,
            clk_ph          : 1,
            baud_rate_div   : 3,
            reserved_1      : 2,
            ref_clk         : 1,
            peri_sel        : 1,
            cs              : 4,
            manual_cs       : 1,
            man_start_en    : 1,
            man_start_com   : 1,
            modefail_gen_en : 1,
            reserved_2      : 14;
    } __attribute__((packed)) bits;
} xzynq_xspips_cr_reg_t;
#endif /* __ASSEMBLER__ */


/* Interrupt resgisters */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            ixr_rxovr    : 1,
            ixr_modf     : 1,
            ixr_txow     : 1,
            ixr_txfull   : 1,
            ixr_rxnempty : 1,
            ixr_rxfull   : 1,
            ixr_txuf     : 1,
            reserved     : 25;
    } __attribute__((packed)) bits;
} xzynq_xspips_ir_reg_t;

typedef xzynq_xspips_ir_reg_t xzynq_xspips_isr_reg_t;
typedef xzynq_xspips_ir_reg_t xzynq_xspips_ier_reg_t;
typedef xzynq_xspips_ir_reg_t xzynq_xspips_idr_reg_t;
typedef xzynq_xspips_ir_reg_t xzynq_xspips_imr_reg_t;
#endif /* __ASSEMBLER__ */


/* Controller Enable resgister */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            enable   : 1,
            reserved : 31;
    } __attribute__((packed)) bits;
} xzynq_xspips_en_reg_t;
#endif /* __ASSEMBLER__ */


/* Delay Control register */

#define XZYNQ_XSPIPS_DLY_D_INT_MAX   255
#define XZYNQ_XSPIPS_DLY_D_AFTER_MAX 255
#define XZYNQ_XSPIPS_DLY_D_BTWN_MAX  255
#define XZYNQ_XSPIPS_DLY_D_NSS_MAX   255

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            d_int   : 8,
            d_after : 8,
            d_btwn  : 8,
            d_nss   : 8;
    } __attribute__((packed)) bits;
} xzynq_xspips_dly_reg_t;
#endif /* __ASSEMBLER__ */


/* Transmit Data register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            data   : 8,
            unused : 24;
    } __attribute__((packed)) bits;
} xzynq_xspips_txd_reg_t;
#endif /* __ASSEMBLER__ */


/* Receive Data register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            data   : 8,
            unused : 24;
    } __attribute__((packed)) bits;
} xzynq_xspips_rxd_reg_t;
#endif /* __ASSEMBLER__ */


/* Slave Idle Count register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            slave_idle_coun : 8,
            reserved        : 24;
    } __attribute__((packed)) bits;
} xzynq_xspips_sicr_reg_t;
#endif /* __ASSEMBLER__ */


/* Tx FIFO Threshold register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            threshold : 32;
    } __attribute__((packed)) bits;
} xzynq_xspips_txwr_reg_t;
#endif /* __ASSEMBLER__ */


/* Rx FIFO Threshold register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            threshold : 32;
    } __attribute__((packed)) bits;
} xzynq_xspips_rxwr_reg_t;
#endif /* __ASSEMBLER__ */


/* Module ID register */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            module_id : 25,
            reserved  : 7;
    } __attribute__((packed)) bits;
} xzynq_xspips_mod_id_reg_t;
#endif /* __ASSEMBLER__ */


/* -------------------------------------------------------------------------
 * Watchdog
 * -------------------------------------------------------------------------
 */

#define XZYNQ_WDT_SIZE 0xF

#define XZYNQ_WDT_ZMR_OFFSET     0x0 /* Zero Mode Register */
#define XZYNQ_WDT_CCR_OFFSET     0x4 /* Counter Control Register */
#define XZYNQ_WDT_RESTART_OFFSET 0x8 /* Restart Register */
#define XZYNQ_WDT_SR_OFFSET      0xC /* Status Register */

/* Zero Mode register */
#define XZYNQ_WDT_ZMR_ACCESS_KEY 0xABC /* Zero access key */

#define XZYNQ_WDT_ZMR_IRQLN_4  0
#define XZYNQ_WDT_ZMR_IRQLN_8  1
#define XZYNQ_WDT_ZMR_IRQLN_16 2
#define XZYNQ_WDT_ZMR_IRQLN_32 3

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            wden      : 1,
            rsten     : 1,
            irqen     : 1,
            reserved0 : 1, /* Should be zero */
            reserved1 : 3, /* Should be set to 0x4 */
            irqln     : 2,
            reserved2 : 3,
            zkey      : 12,
            reserved3 : 8;
    } __attribute__((packed)) bits;
} xzynq_wdt_zmr_reg_t;
#endif /* __ASSEMBLER__ */

/* Counter Control register */

#define XZYNQ_WDT_CCR_ACCESS_KEY 0x248 /* Counter access key */

/* Clock prescaler value and selection */
#define XZYNQ_WDT_CCR_PRESCALE_8           8
#define XZYNQ_WDT_CCR_PRESCALE_64          64
#define XZYNQ_WDT_CCR_PRESCALE_512         512
#define XZYNQ_WDT_CCR_PRESCALE_4096        4096
#define XZYNQ_WDT_CCR_PRESCALE_SELECT_8    0
#define XZYNQ_WDT_CCR_PRESCALE_SELECT_64   1
#define XZYNQ_WDT_CCR_PRESCALE_SELECT_512  2
#define XZYNQ_WDT_CCR_PRESCALE_SELECT_4096 3

#define XZYNQ_WDT_COUNTER_VALUE_DIVISOR 0x1000 /* Counter value divisor */
#define XZYNQ_WDT_COUNTER_MAX           0xFFF  /* Counter maximum value */
#define XZYNQ_WDT_COUNTER_TOTAL_MIN     0xFFF  /* Full counter minimum value */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            clksel    : 2,
            crv       : 12,
            ckey      : 12,
            reserved0 : 6;
    } __attribute__((packed)) bits;
} xzynq_wdt_ccr_reg_t;
#endif /* __ASSEMBLER__ */

/* Register RESTART */

#define XZYNQ_WDT_RST_RESTART_KEY 0x00001999 /* Restart key */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            rstkey    : 16,
            reserved0 : 16;
    } __attribute__((packed)) bits;
} xzynq_wdt_rst_reg_t;
#endif /* __ASSEMBLER__ */

/* Register STATUS */

#ifndef __ASSEMBLER__
typedef union {
    uint32_t raw;
    struct {
        uint32_t
            wdz       : 1,
            reserved0 : 31;
    } __attribute__((packed)) bits;
} xzynq_wdt_sr_reg_t;
#endif /* __ASSEMBLER__ */

/*
 * These masks are needed for startup, because it still uses in/out functions
 */
#define XZYNQ_WDT_ZMR_WDEN_MASK       0x00000001 /* Enable the WDT */
#define XZYNQ_WDT_ZMR_RSTEN_MASK      0x00000002 /* Enable the reset output */
#define XZYNQ_WDT_ZMR_IRQEN_MASK      0x00000004 /* Enable IRQ output */
#define XZYNQ_WDT_ZMR_RSTLEN_16       0x00000030 /* Reset pulse of 16 pclk cycles */
#define XZYNQ_WDT_ZMR_ZKEY_VAL        0x00ABC000 /* Access key, 0xABC << 12 */
#define XZYNQ_WDT_ZMR_ZKEY_SHIFT      12         /* Counter reset shift */
#define XZYNQ_WDT_REGISTER_ACCESS_KEY 0x00920000 /* Counter register access key */
#define XZYNQ_WDT_CCR_CRV_MASK        0x00003FFC /* Counter reset value */
#define XZYNQ_WDT_CCR_CKEY_SHIFT      14         /* Counter reset shift */

#endif /* __HW_VENDOR_XILINX_ZYNQ_H__ */
