/*
 *
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __RPI4B_I2C_H__
#define __RPI4B_I2C_H__

#include <stdint.h>

#define BCM2711_BASE_I2C0		UINT64_C(0xFE205000)
#define BCM2711_BASE_I2C1		UINT64_C(0xFE804000)
#define BCM2711_BASE_I2C3		UINT64_C(0xFE205600)
#define BCM2711_BASE_I2C4		UINT64_C(0xFE205800)
#define BCM2711_BASE_I2C5		UINT64_C(0xFE205A80)
#define BCM2711_BASE_I2C6		UINT64_C(0xFE205C00)

#define BCM2711_IRQ_I2C         UINT8_C(117 + 32)

#define BCM2711_I2C_FIFO_SIZE_BYTE   UINT8_C(16)

#define BCM2711_I2C_OFFSET_REG_CONTROL UINT8_C(0x00)
#define BCM2711_I2C_OFFSET_REG_STATUS  UINT8_C(0x04)
#define BCM2711_I2C_OFFSET_REG_DLEN    UINT8_C(0x08)
#define BCM2711_I2C_OFFSET_REG_ADDRESS UINT8_C(0x0C)
#define BCM2711_I2C_OFFSET_REG_FIFO    UINT8_C(0x10)
#define BCM2711_I2C_OFFSET_REG_DIV     UINT8_C(0x14)
#define BCM2711_I2C_OFFSET_REG_DEL     UINT8_C(0x18)
#define BCM2711_I2C_OFFSET_REG_CLKT    UINT8_C(0x1C)

typedef union {
	uint32_t raw;
	struct {
		uint32_t
			read       : 1,
			reserved_0 : 3,
			clear      : 2,
			reserved_1 : 1,
			st         : 1,
			intd       : 1,
			intt       : 1,
			intr       : 1,
            reserved_2 : 4,
            i2c_en     : 1,
            reserved_3 : 16;
	} __attribute__ ((packed)) bits;
} rpi4b_i2c_control_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            t_a        : 1,
            done       : 1,
            tx_w       : 1,
            rx_r       : 1,
            tx_d       : 1,
            rx_d       : 1,
            tx_e       : 1,
            rx_f       : 1,
            err        : 1,
            clkt       : 1,
            reserved_0 : 22;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_status_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            d_len      : 15,
            reserved_0 : 17;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_data_length_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            addr       : 7,
            reserved_0 : 25;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_addr_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            data       : 8,
            reserved_0 : 24;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_fifo_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            c_div      : 16,
            reserved_0 : 16;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_div_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            re_dl : 16,
            fe_dl : 16;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_delay_reg_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t
            t_out      : 16,
            reserved_0 : 16;
    } __attribute__ ((packed)) bits;
} rpi4b_i2c_clkt_reg_t;

#endif /* __RPI4B_I2C_H__ */