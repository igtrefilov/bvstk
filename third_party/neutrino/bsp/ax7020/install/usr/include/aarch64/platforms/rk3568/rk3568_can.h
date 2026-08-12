/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __RK3568_CAN_H__
#define __RK3568_CAN_H__

/* Operational base */
#define RK3568_CAN0_REG_BASE 0xFE570000
#define RK3568_CAN1_REG_BASE 0xFE580000
#define RK3568_CAN2_REG_BASE 0xFE590000

/* IRQ */
#define RK3568_CAN0_SYSINTR 33
#define RK3568_CAN1_SYSINTR 34
#define RK3568_CAN2_SYSINTR 35

#define RK3568_CAN_REG_SIZE 0x00010000

/* RK3568 CAN register offsets */

#define RK3568_CAN_MODE 0x0000 /* CAN controller working mode configure register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			work_mode      : 1,
			sleep_mode     : 1,
			self_test      : 1,
			slient_mode    : 1,
			lback_mode     : 1,
			rxstx_mode     : 1,
			txorder_mode   : 1,
			rxsort_mode    : 1,
			cover_mode     : 1,
			ovld_mode      : 1,
			auto_retx_mode : 1,
			auto_bus_on    : 1,
			space_rx_mode  : 1,
			brsd           : 1,
			dpee           : 1,
			can_fd         : 1,
			reserved       : 16;
	} __attribute__ ((packed)) bits;
} rk3568_can_mode_t;

#define RK3568_CAN_CMD 0x0004 /* CAN command register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tx0_req        : 1,
			tx1_req        : 1,
			reserved       : 30;
	} __attribute__((packed)) bits;
} rk3568_can_cmd_t;

#define RK3568_CAN_STATE 0x0008 /* CAN state register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_buffer_full      : 1,
			tx_buffer_full      : 1,
			rx_period           : 1,
			tx_period           : 1,
			error_warning_state : 1,
			bus_off_state       : 1,
			sleep_state         : 1,
			reserved            : 25;
	} __attribute__((packed)) bits;
} rk3568_can_state_t;

#define RK3568_CAN_INT 0x000C /* Interrupt state register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_finish_int                  : 1,
			tx_finish_int                  : 1,
			error_warning_int              : 1,
			overload_int                   : 1,
			passive_error_int              : 1,
			tx_arbit_fail_int              : 1,
			error_int                      : 1,
			rx_fifo_full_int               : 1,
			rx_fifo_overflow_int           : 1,
			bus_off_int                    : 1,
			bus_off_recovery_int           : 1,
			timestamp_counter_overflow_int : 1,
			tx_event_fifo_overflow_int     : 1,
			tx_event_fifo_full_int         : 1,
			wakeup_int                     : 1,
			reserved                       : 17;
	} __attribute__((packed)) bits;
} rk3568_can_int_t;

#define RK3568_CAN_INT_MASK 0x0010 /* Interrupt enable registers */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_finish_int_mask                  : 1,
			tx_finish_int_mask                  : 1,
			error_warning_int_mask              : 1,
			rx_buffer_overflow_int_mask         : 1,
			passive_error_int_mask              : 1,
			tx_arbit_fail_int_mask              : 1,
			error_int_mask                      : 1,
			rx_fifo_full_int_mask               : 1,
			rx_fifo_overflow_int_mask           : 1,
			bus_off_int_mask                    : 1,
			bus_off_recovery_int_mask           : 1,
			timestamp_counter_overflow_int_mask : 1,
			tx_event_fifo_overflow_int_mask     : 1,
			tx_event_fifo_full_int_mask         : 1,
			wakeup_int_mask                     : 1,
			reserved                            : 17;
	} __attribute__((packed)) bits;
} rk3568_can_int_mask_t;

#define RK3568_CAN_DMA_CTRL 0x0014 /* Dma mode control */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			dma_tx_mode : 1,
			dma_rx_mode : 1,
			reserved    : 30;
	} __attribute__((packed)) bits;
} rk3568_can_dma_ctrl_t;

#define RK3568_CAN_BITTIMING 0x0018 /* Bit timing configure register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tseg1       : 4,
			tseg2       : 3,
			reserved0   : 1,
			brp         : 6,
			sjw         : 2,
			sample_mode : 1,
			reserved1   : 15;
	} __attribute__((packed)) bits;
} rk3568_can_bittiming_t;

#define RK3568_CAN_ARBITFAIL 0x0028 /* Arbit fail code register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			arbit_fail_code : 7,
			reserved        : 25;
	} __attribute__((packed)) bits;
} rk3568_can_arbitfail_t;

#define RK3568_CAN_ERROR_CODE 0x002c /* Error code register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_error_position : 16,
			tx_error_position : 9,
			error_direction   : 1,
			error_type        : 3,
			error_phase       : 1,
			reserved          : 2;
	} __attribute__((packed)) bits;
} rk3568_can_error_code_t;

#define RK3568_CAN_RXERRORCNT 0x0034 /* Receive error counter */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_err_cnt : 8,
			reserved   : 24;
	} __attribute__((packed)) bits;
} rk3568_can_rxerrorcnt_t;

#define RK3568_CAN_TXERRORCNT 0x0038 /* Transmit error counter */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tx_err_cnt : 9,
			reserved   : 23;
	} __attribute__((packed)) bits;
} rk3568_can_txerrorcnt_t;

#define RK3568_CAN_IDCODE 0x003C /* CAN controller's identifier */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			id_code  : 29,
			reserved : 3;
	} __attribute__((packed)) bits;
} rk3568_can_idcode_t;

#define RK3568_CAN_IDMASK 0x0040 /* Identification code bit mask register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			id_mask  : 29,
			reserved : 3;
	} __attribute__((packed)) bits;
} rk3568_can_idmask_t;

#define RK3568_CAN_TXFRAMEINFO                                                 \
  0x0050 /* TX frame information configuration register */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			txdata_length  : 4,
			reserved0      : 2,
			tx_rtr         : 1,
			txframe_format : 1,
			reserved1      : 24;
	} __attribute__((packed)) bits;
} rk3568_can_txframeinfo_t;

#define RK3568_CAN_TXID 0x0054 /* CAN controller transmit ID */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tx_id          : 29,
			reserved       : 3;
	} __attribute__((packed)) bits;
} rk3568_can_txid_t;

#define RK3568_CAN_TXDATA0 0x0058 /* CAN controller transmit DATA0 */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tx_data0 : 32;
	} __attribute__((packed)) bits;
} rk3568_can_txdata0_t;

#define RK3568_CAN_TXDATA1 0x005C /* CAN controller transmit DATA1 */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			tx_data1 : 32;
	} __attribute__((packed)) bits;
} rk3568_can_txdata1_t;

#define RK3568_CAN_RXFRAMEINFO                                                 \
  0x0060 /* RX frame information register. This register needs to be           \
			read(clear) before receiving the next frame */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rxdata_length  : 4,
			reserved0      : 2,
			rx_rtr         : 1,
			rxframe_format : 1,
			reserved1      : 24;
	} __attribute__((packed)) bits;
} rk3568_can_rxframeinfo_t;

#define RK3568_CAN_RXID                                                        \
  0x0064 /* CAN controller receive ID. This register needs to be read(clear)   \
			before receiving the next frame. */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_id          : 29,
			reserved       : 3;
	} __attribute__((packed)) bits;
} rk3568_can_rxid_t;

#define RK3568_CAN_RXDATA0                                                     \
  0x0068 /* Receive Data RegO. This register needs to be read(clear) before    \
			receiving the next frame */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_data0 : 32;
	} __attribute__((packed)) bits;
} rk3568_can_rxdata0_t;

#define RK3568_CAN_RXDATA1                                                     \
  0x006C /* Receive Data Reg1. This register needs to be read(clear) before    \
			receiving the next frame */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_data1 : 32;
	} __attribute__((packed)) bits;
} rk3568_can_rxdata1_t;

#define RK3568_CAN_RTL_VERSION 0x0070 /* CAN RTL version */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			version : 32;
	} __attribute__((packed)) bits;
} rk3568_can_rtl_version_t;

#define RK3568_CAN_TIMESTAMP_CTRL 0x010c /* CAN RTL version */

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			time_base_counter_enable   : 1,
			time_base_counter_prescale : 6,
			reserved                   : 25;
	} __attribute__((packed)) bits;
} rk3568_can_timestamp_ctrl_t;

#define RK3568_CAN_TIMESTAMP 0x0110

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			time_base_counter : 32;
	} __attribute__((packed)) bits;
} rk3568_can_timestamp_t;

#define RK3568_CAN_TXEVENT_FIFO_CTRL 0x0114

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			txe_fifo_enable    : 1,
			txe_fifo_watermark : 4,
			txe_fifo_cnt       : 4,
			reserved           : 23;
	} __attribute__((packed)) bits;
} rk3568_can_txevent_fifo_ctrl_t;

#define RK3568_CAN_RX_FIFO_CTRL 0x0118

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			rx_fifo_enable         : 1,
			rx_fifo_full_watermark : 3,
			rx_fifo_cnt            : 3,
			reserved               : 25;
	} __attribute__((packed)) bits;
} rk3568_can_rx_fifo_ctrl_t;

#define RK3568_CAN_AFR_CTRL 0x011C
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			uaf1     : 1,
			uaf2     : 1,
			uaf3     : 1,
			uaf4     : 1,
			uaf5     : 1,
			reserved : 27;
	} __attribute__((packed)) bits;
} rk3568_can_afr_ctrl_t;

#define RK3568_CAN_IDCODE0 0x0120
#define RK3568_CAN_IDMASK0 0x0124

typedef rk3568_can_idcode_t rk3568_can_idcode0_t;
typedef rk3568_can_idmask_t rk3568_can_idmask0_t;

#define RK3568_CAN_IDCODE1 0x0128
#define RK3568_CAN_IDMASK1 0x012c

typedef rk3568_can_idcode_t rk3568_can_idcode1_t;
typedef rk3568_can_idmask_t rk3568_can_idmask1_t;

#define RK3568_CAN_IDCODE2 0x0130
#define RK3568_CAN_IDMASK2 0x0134

typedef rk3568_can_idcode_t rk3568_can_idcode2_t;
typedef rk3568_can_idmask_t rk3568_can_idmask2_t;

#define RK3568_CAN_IDCODE3 0x0138
#define RK3568_CAN_IDMASK3 0x013c

typedef rk3568_can_idcode_t rk3568_can_idcode3_t;
typedef rk3568_can_idmask_t rk3568_can_idmask3_t;

#define RK3568_CAN_IDCODE4 0x0140
#define RK3568_CAN_IDMASK4 0x0144

typedef rk3568_can_idcode_t rk3568_can_idcode4_t;
typedef rk3568_can_idmask_t rk3568_can_idmask4_t;

#define RK3568_CAN_RX_FIFO_RDATA 0x0400

#define RK3568_CAN_TXE_FIFO_RDATA 0x0500

#endif /* __RK3568_CAN_H__ */
