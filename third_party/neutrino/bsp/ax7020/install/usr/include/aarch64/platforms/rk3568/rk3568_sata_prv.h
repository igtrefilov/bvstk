/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
*/


#ifndef __RK3568_SATA_PRV_H__
#define __RK3568_SATA_PRV_H__

#include <rk3568/rk3568_common.h>

#define RK3568_SATA_MULTI_PHY0_BASE                                      0xfe820000
#define RK3568_SATA_MULTI_PHY1_BASE                                      0xfe830000
#define RK3568_SATA_MULTI_PHY2_BASE                                      0xfe840000

#define RK3568_SATA_MULTI_PHY_SIZE                                       0x00010000

#define RK3568_SATA_PIPE_PHY_GRF_SIZE                                    0x00010000

#define RK3568_SATA_PIPE_GRF_SIZE                                        0x00010000

#define RK3568_SATA_PMU_GRF_GPIO0A_OFFSET                                0x0004
#define RK3568_SATA_PMU_GRF_GPIO0A_BIT                                   12
#define RK3568_SATA_PMU_GRF_GPIO0A_VALUE                                 0

#define RK3568_SATA_PIPE_GRF_PHY_SPDMODE_1_5GB                           0x0
#define RK3568_SATA_PIPE_GRF_PHY_SPDMODE_3GB                             0x1
#define RK3568_SATA_PIPE_GRF_PHY_SPDMODE_6GB                             0x2

#define RK3568_SATA_MULTI_PHY_REG_007_OFFSET                             0x0018
#define RK3568_SATA_MULTI_PHY_REG_008_OFFSET                             0x001c
#define RK3568_SATA_MULTI_PHY_REG_015_OFFSET                             0x0038
#define RK3568_SATA_MULTI_PHY_REG_016_OFFSET                             0x003c

#define RK3568_SATA_MULTI_PHY_REG_015_SSC_MASK                           0x3
#define RK3568_SATA_MULTI_PHY_REG_016_SSC_MASK                           0x3FC

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET                        0x0000
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET                        0x0004
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET                        0x0008
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET                        0x000c

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_VALUE                         0x0119
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_VALUE                         0x0040
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_VALUE                         0x80c3
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_VALUE                         0x4407

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_24M                           0x0
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_25M                           0x1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_100M                          0x2

#define RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET                            0x0000

#define RK3568_SATA_MULTI_PHY_SSC_EN      			                     1
#define RK3568_SATA_MULTI_PHY_TX_RTERM      		                     8
#define RK3568_SATA_MULTI_PHY_RX_RTERM      		                     8
#define RK3568_SATA_MULTI_PHY_EN_ADPT      			                     1

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PIPE_DATABUSWIDTH             1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PHY_MODE                      2
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PIPE_RATE                     1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_MAC_PCLKREQ_N                 1

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_PIPE_PIPE_TXDEEMPH            1

#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_DATABUSWIDTH         1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_PHYMODE              1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_EBUF                 1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_BYPASS_CODEC         1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_BYPASS_CODEC         1
#define RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_TXCOMPLIANCE_I       1

#define	RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXSWING_I            1
#define	RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXDEEMPH_I           1
#define	RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXMARGIN_I           1
#define	RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_RXELECIDLE_SEL                1
#define	RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_PIPE_SEL                      2

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    rx_rterm                       : 4,
                    tx_rterm                       : 4,
                    reserved                       : 24;
    } __attribute__((packed)) bits;
} rk3568_sata_multi_phy_reg_007_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    tx_swing_com                   : 4,
                    ssc_en                         : 1,
                    near_end_lpk_enable            : 1,
                    tx_swing                       : 1,
                    byps_cdr_en                    : 1,
                    reserved                       : 24;
    } __attribute__((packed)) bits;
} rk3568_sata_multi_phy_reg_008_t;


typedef union {
    uint32_t        raw;
    struct {
        uint32_t    en_adpt                        : 1,
                    rx_tseq                        : 1,
                    test_sel                       : 4,
                    ssc_cnt                        : 2,
                    reserved                       : 24;
    } __attribute__((packed)) bits;
} rk3568_sata_multi_phy_reg_015_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    ssc_cnt                        : 8,
                    reserved                       : 24;
    } __attribute__((packed)) bits;
} rk3568_sata_multi_phy_reg_016_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    pipe_databuswidth              : 2,
                    pipe_phymode                   : 2,
                    pipe_rate                      : 2,
                    pipe_powerdown                 : 2,
                    pipe_mac_pclkreq_n             : 1,
                    pipe_l1sub_entreq              : 1,
                    pipe_ebuffmode                 : 1,
                    pipe_bypass_codec              : 1,
                    pipe_rxterm                    : 1,
                    pipe_rxelecidle_disable        : 1,
                    pipe_rxstandby                 : 1,
                    reserved                       : 1,
                    write_enable                   : 16;
    } __attribute__((packed)) bits;
} rk3568_sata_pipe_phy_grf_pipe_con0_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    pipe_l0_txdrx_loopback         : 1,
                    pipe_txeidle                   : 1,
                    pipe_l0_txoneszeros            : 1,
                    pipe_txcommonmode_disable      : 1,
                    pipe_txcompliance              : 1,
                    pipe_txswing                   : 1,
                    pipe_txdeemph                  : 2,
                    pipe_txmargin                  : 3,
                    pipe_txpattern_sata            : 2,
                    phy_clk_sel                    : 2,
                    reserved                       : 1,
                    write_enable                   : 16;
    } __attribute__((packed)) bits;
} rk3568_sata_pipe_phy_grf_pipe_con1_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    sel_pipe_databuswidth          : 1,
                    sel_pipe_phymode               : 1,
                    sel_pipe_rate                  : 1,
                    sel_pipe_mac_pclreq_n          : 1,
                    sel_pipe_powerdown             : 1,
                    sel_pipe_l1sub_entreq_i        : 1,
                    sel_pipe_ebuf                  : 1,
                    sel_pipe_bypass_codec          : 1,
                    sel_pipe_rxterm                : 1,
                    sel_pipe_rxelecide_disable     : 1,
                    sel_pipe_rxstandby             : 1,
                    sel_pipe_txdrx_loopback        : 1,
                    sel_pipe_txelecidle            : 1,
                    sel_pipe_txoneszeros           : 1,
                    sel_pipe_txcommonmode_disable  : 1,
                    sel_pipe_txcompliance_i        : 1,
                    write_enable                   : 16;
    } __attribute__((packed)) bits;
} rk3568_sata_pipe_phy_grf_pipe_con2_t;

typedef union {
    uint32_t        raw;
    struct {
        uint32_t    sel_pipe_txswing_i             : 1,
                    sel_pipe_txdeemph_i            : 1,
                    sel_pipe_txmargin_i            : 1,
                    sel_pipe_txpattern_sata_i      : 1,
                    reserved_4                     : 4,
                    phy_clk_ref_src_i              : 2,
                    rxelecidle_sel                 : 1,
                    from_pcie_io                   : 1,
                    reserved_12                    : 1,
                    pipe_sel                       : 2,
                    qsgm_mode                      : 1,
                    write_enable                   : 16;
    } __attribute__((packed)) bits;
} rk3568_sata_pipe_phy_grf_pipe_con3_t;

typedef union {
    uint32_t         raw;
    struct {
        uint32_t    pcie20_link_rst_grt            : 1,
                    pcie30x1_link_rst_grt          : 1,
                    pcie30x2_link_rst_grt          : 1,
                    reserved                       : 1,
                    sata0_phy_spdmode              : 2,
                    sata0_phy_rx_err               : 1,
                    sata0_pclkreq_n                : 1,
                    sata1_phy_spdmode              : 2,
                    sata1_phy_rx_err               : 1,
                    sata1_pclkreq_n                : 1,
                    sata2_phy_spdmode              : 2,
                    sata2_phy_rx_err               : 1,
                    sata2_pclkreq_n                : 1,
                    write_enable                   : 16;
    } __attribute__((packed)) bits;
} rk3568_sata_pipe_grf_pipe_con0_t;

static inline
void rk3568_sata_multi_phy_set_ssc_en(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_multi_phy_reg_008_t reg;
	reg.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_008_OFFSET);
	reg.bits.ssc_en = value;

	out32(addr + RK3568_SATA_MULTI_PHY_REG_008_OFFSET, reg.raw);
}

static inline
void rk3568_sata_multi_phy_set_tx_rterm(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_multi_phy_reg_007_t reg;
	reg.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_007_OFFSET);
	reg.bits.tx_rterm = value;

	out32(addr + RK3568_SATA_MULTI_PHY_REG_007_OFFSET, reg.raw);
}

static inline
void rk3568_sata_multi_phy_set_rx_rterm(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_multi_phy_reg_007_t reg;
	reg.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_007_OFFSET);
	reg.bits.rx_rterm = value;

	out32(addr + RK3568_SATA_MULTI_PHY_REG_007_OFFSET, reg.raw);
}

static inline
void rk3568_sata_multi_phy_set_en_adpt(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_multi_phy_reg_015_t reg;
	reg.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_015_OFFSET);
	reg.bits.en_adpt = value;

	out32(addr + RK3568_SATA_MULTI_PHY_REG_015_OFFSET, reg.raw);
}

static inline
void rk3568_sata_multi_phy_set_ssc_cnt(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_multi_phy_reg_015_t reg15;
	reg15.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_015_OFFSET);
	reg15.bits.ssc_cnt = value & RK3568_SATA_MULTI_PHY_REG_015_SSC_MASK;

	rk3568_sata_multi_phy_reg_016_t reg16;
	reg16.raw = in32(addr + RK3568_SATA_MULTI_PHY_REG_016_OFFSET);
	reg16.bits.ssc_cnt = value & RK3568_SATA_MULTI_PHY_REG_016_SSC_MASK;

	out32(addr + RK3568_SATA_MULTI_PHY_REG_015_OFFSET, reg15.raw);
	out32(addr + RK3568_SATA_MULTI_PHY_REG_016_OFFSET, reg16.raw);
}

static inline
void rk3568_sata_multi_phy_init(const uintptr_t addr)
{
	rk3568_sata_multi_phy_set_ssc_en(addr,
		RK3568_SATA_MULTI_PHY_SSC_EN);

	rk3568_sata_multi_phy_set_tx_rterm(addr,
		RK3568_SATA_MULTI_PHY_TX_RTERM);

	rk3568_sata_multi_phy_set_rx_rterm(addr,
		RK3568_SATA_MULTI_PHY_RX_RTERM);

	rk3568_sata_multi_phy_set_en_adpt(addr,
		RK3568_SATA_MULTI_PHY_EN_ADPT);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_set_pipe_databuswidth(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET);
	reg.bits.pipe_databuswidth = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_set_pipe_phymode(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET);
	reg.bits.pipe_phymode = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_set_pipe_rate(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET);
	reg.bits.pipe_rate = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_set_pipe_mac_pclkreq_n(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET);
	reg.bits.pipe_mac_pclkreq_n = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_clear(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_pipe_con0_t reg;
	reg.raw = 0;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con0_init(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_con0_clear(addr);

	rk3568_sata_pipe_phy_grf_con0_set_pipe_databuswidth(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PIPE_DATABUSWIDTH);

	rk3568_sata_pipe_phy_grf_con0_set_pipe_phymode(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PHY_MODE);

	rk3568_sata_pipe_phy_grf_con0_set_pipe_rate(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_PIPE_RATE);

	rk3568_sata_pipe_phy_grf_con0_set_pipe_mac_pclkreq_n(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON0_MAC_PCLKREQ_N);
}

static inline
void rk3568_sata_pipe_phy_grf_con1_set_pipe_txdeemph(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con1_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET);
	reg.bits.pipe_txdeemph = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con1_set_phy_clk_sel(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con1_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET);
	reg.bits.phy_clk_sel = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con1_clear(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_pipe_con1_t reg;
	reg.raw = 0;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con1_init(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_con1_clear(addr);

	rk3568_sata_pipe_phy_grf_con1_set_pipe_txdeemph(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON1_PIPE_PIPE_TXDEEMPH);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_databuswidth(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET);
	reg.bits.sel_pipe_databuswidth = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_phymode(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET);
	reg.bits.sel_pipe_phymode = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_ebuf(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET);
	reg.bits.sel_pipe_ebuf = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_bypass_codec(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET);
	reg.bits.sel_pipe_bypass_codec = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_txcompliance_i(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET);
	reg.bits.sel_pipe_txcompliance_i = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_clear(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_pipe_con2_t reg;
	reg.raw = 0;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con2_init(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_con2_clear(addr);

	rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_databuswidth(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_DATABUSWIDTH);

	rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_phymode(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_PHYMODE);

	rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_ebuf(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_EBUF);

	rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_bypass_codec(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_BYPASS_CODEC);

	rk3568_sata_pipe_phy_grf_con2_set_sel_pipe_txcompliance_i(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON2_SEL_PIPE_TXCOMPLIANCE_I);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txswing_i(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET);
	reg.bits.sel_pipe_txswing_i = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txdeemph_i(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET);
	reg.bits.sel_pipe_txdeemph_i = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txmargin_i(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET);
	reg.bits.sel_pipe_txmargin_i = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_set_rxelecidle_sel(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET);
	reg.bits.rxelecidle_sel = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_set_pipe_sel(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET);
	reg.bits.pipe_sel = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_clear(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_pipe_con3_t reg;
	reg.raw = 0;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_phy_grf_con3_init(const uintptr_t addr)
{
	rk3568_sata_pipe_phy_grf_con3_clear(addr);

	rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txswing_i(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXSWING_I);

	rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txdeemph_i(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXDEEMPH_I);

	rk3568_sata_pipe_phy_grf_con3_set_sel_pipe_txmargin_i(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_SEL_PIPE_TXMARGIN_I);

	rk3568_sata_pipe_phy_grf_con3_set_rxelecidle_sel(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_RXELECIDLE_SEL);

	rk3568_sata_pipe_phy_grf_con3_set_pipe_sel(addr,
		RK3568_SATA_PIPE_PHY_GRF_PIPE_CON3_PIPE_SEL);
}

static inline
void rk3568_sata_pipe_grf_con0_set_sata0_phy_spdmode(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET);
	reg.bits.sata0_phy_spdmode = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_grf_con0_set_sata1_phy_spdmode(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET);
	reg.bits.sata1_phy_spdmode = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET, reg.raw);
}

static inline
void rk3568_sata_pipe_grf_con0_set_sata2_phy_spdmode(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_pipe_grf_pipe_con0_t reg;
	reg.raw = in32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET);
	reg.bits.sata2_phy_spdmode = value;
	reg.bits.write_enable = 0xffff;

	out32(addr + RK3568_SATA_PIPE_GRF_PIPE_CON0_OFFSET, reg.raw);
}

#endif /* __RK3568_SATA_PRV_H__ */
