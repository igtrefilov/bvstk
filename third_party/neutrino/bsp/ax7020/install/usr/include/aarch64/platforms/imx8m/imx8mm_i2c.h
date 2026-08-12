/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __IMX8MM_I2C_H__
#define __IMX8MM_I2C_H__

#define IMX8MM_I2C_IOMUX_BASE             0x30330000
#define IMX8MM_I2C_CCGR_BASE              0x30380000

#define IMX8MM_I2C_CCGR_I2C3_OFFSET       0x4190
#define IMX8MM_I2C_CCGR_I2C4_OFFSET       0x41a0

#define IMX8MM_I2C_CCGR_I2C3_EN_VAL       0x3
#define IMX8MM_I2C_CCGR_I2C4_EN_VAL       0x3

#define IMX8MM_I2C_PULL_UP_ENABLE         0x1c2

#define IMX8MM_I2C_MUX_I2C1_SCL           0x214
#define IMX8MM_I2C_MUX_I2C1_SDA           0x218
#define IMX8MM_I2C_MUX_I2C2_SCL           0x21c
#define IMX8MM_I2C_MUX_I2C2_SDA           0x220
#define IMX8MM_I2C_MUX_I2C3_SCL           0x224
#define IMX8MM_I2C_MUX_I2C3_SDA           0x228
#define IMX8MM_I2C_MUX_I2C4_SCL           0x22c
#define IMX8MM_I2C_MUX_I2C4_SDA           0x230

#define IMX8MM_I2C_MUX_MODE_ALT_0         0x0

#define IMX8MM_I2C_PAD_I2C1_SCL           0x47c
#define IMX8MM_I2C_PAD_I2C1_SDA           0x480
#define IMX8MM_I2C_PAD_I2C2_SCL           0x484
#define IMX8MM_I2C_PAD_I2C2_SDA           0x488
#define IMX8MM_I2C_PAD_I2C3_SCL           0x48c
#define IMX8MM_I2C_PAD_I2C3_SDA           0x490
#define IMX8MM_I2C_PAD_I2C4_SCL           0x494
#define IMX8MM_I2C_PAD_I2C4_SDA           0x498

#define IMX8MM_ENET_INPUT_ENET1           0x4c0
#define IMX8MM_I2C_DAISY_I2C1_SDA         0x2

#define IMX8MM_USDHC_INPUT_USDHC3_CD      0x544
#define IMX8MM_I2C_DAISY_I2C2_SCL         0x1
#define IMX8MM_USDHC_INPUT_USDHC3_WP      0x548
#define IMX8MM_I2C_DAISY_I2C2_SDA         0x1

#define IMX8MM_PCIE_INPUT_PCIE1_CLK       0x524
#define IMX8MM_I2C_DAISY_I2C4_SCL         0x0

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    mux_mode              :3,
				    reserved1             :1,
				    sion                  :1,
				    reserved2             :27;
	} __attribute__((packed)) bits;
} imx8mm_mux_ctl_pad_i2c_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    reserved1             :1,
				    dse                   :2,
				    fsel                  :2,
				    ode                   :1,
				    pue                   :1,
				    hys                   :1,
				    pe                    :1,
				    reserved3             :23;
	} __attribute__((packed)) bits;
} imx8mm_pad_ctl_i2c_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mm_i2c1_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mm_i2c2_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :3,
				    reserved              :29;
	} __attribute__((packed)) bits;
} imx8mm_i2c3_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :3,
				    reserved              :29;
	} __attribute__((packed)) bits;
} imx8mm_i2c4_scl_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mm_enet1_mdio_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mm_pcie1_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    daisy                 :2,
				    reserved              :30;
	} __attribute__((packed)) bits;
} imx8mm_usdhc3_select_input_t;

typedef union {
	uint32_t 	    raw;
	struct {
		uint32_t    domain0               :2,
					reserved0             :2,
					domain1               :2,
					reserved1             :2,
					domain2               :2,
					reserved2             :2,
					domain3               :2,
					reserved3             :18;
	} __attribute__((packed)) bits;
} imx8mm_ccgr_t;

static inline
void imx8mm_ccgr_set_domain0(const uint32_t offset, const uint32_t value)
{
	imx8mm_ccgr_t reg;
	reg.raw = in32(IMX8MM_I2C_CCGR_BASE + offset);
	reg.bits.domain0 = value;

	out32(IMX8MM_I2C_CCGR_BASE + offset, reg.raw);
}

static inline
void imx8mm_usdhc3_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_usdhc3_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_pcie1_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_pcie1_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_enet1_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_enet1_mdio_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c_mux_enable_sion(const uint32_t offset)
{
	imx8mm_mux_ctl_pad_i2c_t reg;
	reg.raw = in32(IMX8MM_I2C_IOMUX_BASE + offset);
	reg.bits.sion = 1;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c_mux_set_mux_mode(const uint32_t offset, const uint32_t value)
{
	imx8mm_mux_ctl_pad_i2c_t reg;
	reg.raw = in32(IMX8MM_I2C_IOMUX_BASE + offset);
	reg.bits.mux_mode = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c_pad_enable_pull_up(const uint32_t offset)
{
	imx8mm_pad_ctl_i2c_t reg;
	reg.raw = IMX8MM_I2C_PULL_UP_ENABLE;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c1_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_i2c1_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c2_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_i2c2_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c3_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_i2c3_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

static inline
void imx8mm_i2c4_set_daisy(const uint32_t offset, const uint32_t value)
{
	imx8mm_i2c4_scl_select_input_t reg = {0};
	reg.bits.daisy = value;

	out32(IMX8MM_I2C_IOMUX_BASE + offset, reg.raw);
}

#endif /* __IMX8MM_I2C_H__ */
