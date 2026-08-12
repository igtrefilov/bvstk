/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
*/


#ifndef __RK3568_SATA_PUB_H__
#define __RK3568_SATA_PUB_H__

#define RK3568_SATA_CAP_OFFSET                      0x0000
#define RK3568_SATA_GHC_OFFSET                      0x0004
#define RK3568_SATA_CMD_OFFSET                      0x0118

#define RK3568_SATA_CMD                             0xe0000

#define MHz 1000000

typedef union {
    uint32_t         raw;
    struct {
        uint32_t     np                             : 5,
                     sxs                            : 1,
                     ems                            : 1,
                     cccs                           : 1,
                     ncs                            : 5,
                     psc                            : 1,
                     ssc                            : 1,
                     pmd                            : 1,
                     fbss                           : 1,
                     spm                            : 1,
                     sam                            : 1,
                     reserved_19                    : 1,
                     iss                            : 4,
                     sclo                           : 1,
                     sal                            : 1,
                     salp                           : 1,
                     sss                            : 1,
                     smsps                          : 1,
                     ssntf                          : 1,
                     sn_cq                          : 1,
                     s64a                           : 1;
    } __attribute__((packed)) bits;
} rk3568_sata_cap_t;

typedef union {
    uint32_t         raw;
    struct {
        uint32_t     hr                             : 1,
                     ie                             : 1,
                     reserved_2_30                  : 29,
                     ae                             : 1;
    } __attribute__((packed)) bits;
} rk3568_sata_ghc_t;

typedef union {
    uint32_t         raw;
    struct {
        uint32_t     st                             : 1,
                     sud                            : 1,
                     pod                            : 1,
                     clo                            : 1,
                     fre                            : 1,
                     reserved                       : 3,
                     ccs                            : 5,
                     mpss                           : 1,
                     fr                             : 1,
                     cr                             : 1,
                     cps                            : 1,
                     pma                            : 1,
                     hpcp                           : 1,
                     mpsp                           : 1,
                     cpd                            : 1,
                     esp                            : 1,
                     fbscp                          : 1,
                     apste                          : 1,
                     atapi                          : 1,
                     dlae                           : 1,
                     alpe                           : 1,
                     asp                            : 1,
                     icc                            : 4;
    } __attribute__((packed)) bits;
} rk3568_sata_cmd_t;

static inline
void rk3568_sata_cap_set_sss(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cap_t reg;
	reg.raw = in32(addr + RK3568_SATA_CAP_OFFSET);
	reg.bits.sss = value;

	out32(addr + RK3568_SATA_CAP_OFFSET, reg.raw);
}

static inline
uint32_t rk3568_sata_cap_read_iss(const uintptr_t addr, const uint32_t value)
{
    rk3568_sata_cap_t reg;
    reg.raw = in32(addr + RK3568_SATA_CAP_OFFSET);

    return reg.bits.iss;
}

static inline
void rk3568_sata_ghc_set_hr(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_ghc_t reg;
	reg.raw = in32(addr + RK3568_SATA_GHC_OFFSET);
	reg.bits.hr = value;

	out32(addr + RK3568_SATA_GHC_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_set_pma(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cmd_t reg;
	reg.raw = in32(addr + RK3568_SATA_CMD_OFFSET);
	reg.bits.pma = value;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_set_hpcp(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cmd_t reg;
	reg.raw = in32(addr + RK3568_SATA_CMD_OFFSET);
	reg.bits.hpcp = value;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_set_mpsp(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cmd_t reg;
	reg.raw = in32(addr + RK3568_SATA_CMD_OFFSET);
	reg.bits.mpsp = value;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_init(const uintptr_t addr)
{
	rk3568_sata_cmd_t reg;
	reg.raw = RK3568_SATA_CMD;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_set_st(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cmd_t reg;
	reg.raw = in32(addr + RK3568_SATA_CMD_OFFSET);
	reg.bits.st = value;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

static inline
void rk3568_sata_cmd_set_sud(const uintptr_t addr, const uint32_t value)
{
	rk3568_sata_cmd_t reg;
	reg.raw = in32(addr + RK3568_SATA_CMD_OFFSET);
	reg.bits.sud = value;

	out32(addr + RK3568_SATA_CMD_OFFSET, reg.raw);
}

#endif /* __RK3568_SATA_PUB_H__ */
