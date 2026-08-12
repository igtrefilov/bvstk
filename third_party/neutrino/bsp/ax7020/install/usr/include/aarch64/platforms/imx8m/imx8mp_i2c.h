/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __IMX8MP_I2C_H__
#define __IMX8MP_I2C_H__

#define IMX8MP_I2C_IOMUX_BASE           0x30330000

#define IMX8MP_I2C_PULL_UP_ENABLE       0x1c2

#define IMX8MP_I2C_MUX_I2C1_SCL         0x200
#define IMX8MP_I2C_MUX_I2C1_SDA         0x204
#define IMX8MP_I2C_MUX_I2C2_SCL         0x208
#define IMX8MP_I2C_MUX_I2C2_SDA         0x20c
#define IMX8MP_I2C_MUX_I2C3_SCL         0x210
#define IMX8MP_I2C_MUX_I2C3_SDA         0x214
#define IMX8MP_I2C_MUX_I2C4_SCL         0x218
#define IMX8MP_I2C_MUX_I2C4_SDA         0x21c

#define IMX8MP_I2C_MUX_MODE_ALT_0       0x0

#define IMX8MP_I2C_PAD_I2C1_SCL         0x460
#define IMX8MP_I2C_PAD_I2C1_SDA         0x464
#define IMX8MP_I2C_PAD_I2C2_SCL         0x468
#define IMX8MP_I2C_PAD_I2C2_SDA         0x46c
#define IMX8MP_I2C_PAD_I2C3_SCL         0x470
#define IMX8MP_I2C_PAD_I2C3_SDA         0x474
#define IMX8MP_I2C_PAD_I2C4_SCL         0x478
#define IMX8MP_I2C_PAD_I2C4_SDA         0x47c

#define IMX8MP_I2C_INPUT_I2C1_SCL       0x5a4
#define IMX8MP_I2C_INPUT_I2C1_SDA       0x5a8
#define IMX8MP_I2C_INPUT_I2C2_SCL       0x5ac
#define IMX8MP_I2C_INPUT_I2C2_SDA       0x5b0
#define IMX8MP_I2C_INPUT_I2C3_SCL       0x5b4
#define IMX8MP_I2C_INPUT_I2C3_SDA       0x5b8
#define IMX8MP_I2C_INPUT_I2C4_SCL       0x5bc
#define IMX8MP_I2C_INPUT_I2C4_SDA       0x5c0

#define IMX8MP_I2C_DAISY_I2C1_SCL       0x2
#define IMX8MP_I2C_DAISY_I2C1_SDA       0x2
#define IMX8MP_I2C_DAISY_I2C2_SCL       0x2
#define IMX8MP_I2C_DAISY_I2C2_SDA       0x2
#define IMX8MP_I2C_DAISY_I2C3_SCL       0x4
#define IMX8MP_I2C_DAISY_I2C3_SDA       0x4
#define IMX8MP_I2C_DAISY_I2C4_SCL       0x5
#define IMX8MP_I2C_DAISY_I2C4_SDA       0x5

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    mux_mode              :3,
				    reserved1             :1,
				    sion                  :1,
				    reserved2             :27;
	} __attribute__((packed)) bits;
} imx8mp_mux_ctl_pad_i2c_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    reserved1             :1,
				    dse                   :2,
				    reserved2             :1,
				    fsel                  :1,
				    ode                   :1,
				    pue                   :1,
				    hys                   :1,
				    pe                    :1,
				    reserved3             :23;
	} __attribute__((packed)) bits;
} imx8mp_pad_ctl_i2c_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mp_i2c1_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mp_i2c2_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :3,
				    reserved              :29;
	} __attribute__((packed)) bits;
} imx8mp_i2c3_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :3,
				    reserved              :29;
	} __attribute__((packed)) bits;
} imx8mp_i2c4_scl_select_input_t;

static inline
void imx8mp_i2c_mux_enable_sion(const uint32_t offset)
{
	imx8mp_mux_ctl_pad_i2c_t reg;
	reg.raw = in32(IMX8MP_I2C_IOMUX_BASE + offset);
	reg.bits.sion = 1;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c_mux_set_mux_mode(const uint32_t offset, const uint32_t value)
{
	imx8mp_mux_ctl_pad_i2c_t reg;
	reg.raw = in32(IMX8MP_I2C_IOMUX_BASE + offset);
	reg.bits.mux_mode = value;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c_pad_enable_pull_up(const uint32_t offset)
{
	imx8mp_pad_ctl_i2c_t reg;
	reg.raw = IMX8MP_I2C_PULL_UP_ENABLE;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c1_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mp_i2c1_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c2_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mp_i2c2_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c3_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mp_i2c3_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mp_i2c4_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mp_i2c4_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MP_I2C_IOMUX_BASE + offset, reg.raw);
}

#endif /* __IMX8MP_I2C_H__ */
