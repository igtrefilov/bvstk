/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __ROCKCHIP_I2C_H__
#define __ROCKCHIP_I2C_H__

#define RK_BASE_ADDR_I2C0		0xFDD40000
#define RK_BASE_ADDR_I2C1		0xFE5A0000
#define RK_BASE_ADDR_I2C2		0xFE5B0000
#define RK_BASE_ADDR_I2C3		0xFE5C0000
#define RK_BASE_ADDR_I2C4		0xFE5D0000
#define RK_BASE_ADDR_I2C5		0xFE5E0000

#define RK_REGSIZE_I2C			0x00010000

/* Register Map */
#define RK_REG_CON				0x00 /* Control register */
	#define I2C_CON_EN				(1 << 0)
	#define I2C_CON_MOD(mod)		((mod) << 1)
	#define I2C_MODE_TX				0x00
	#define I2C_MODE_TRX			0x01
	#define I2C_MODE_RX				0x02
	#define I2C_MODE_RRX			0x03
	#define I2C_CON_MASK			(3 << 1)
	#define I2C_CON_START			(1 << 3)
	#define I2C_CON_STOP			(1 << 4)
	#define I2C_CON_LASTACK			(1 << 5)
	#define I2C_CON_ACTACK			(1 << 6)
	#define I2C_CON_TUNING_MASK		(0xff << 8)
	#define I2C_CON_SDA_CFG(cfg)	((cfg) << 8)
	#define I2C_CON_STA_CFG(cfg)	((cfg) << 12)
	#define I2C_CON_STO_CFG(cfg)	((cfg) << 14)
	#define I2C_CON_VERSION_SHIFT	16

#define RK_REG_CLKDIV			0x04 /* Clock divisor register */

#define RK_REG_MRXADDR			0x08 /* Target address for REGISTER_TX */

#define RK_REG_MRXRADDR			0x0c /* Target register address for REGISTER_TX */

#define RK_REG_MTXCNT			0x10 /* Number of bytes to be transmitted */

#define RK_REG_MRXCNT			0x14 /* Number of bytes to be received */

#define RK_REG_IEN				0x18 /* Interrupt enable */
	#define I2C_BTFIEN				(1 << 0)
	#define I2C_BRFIEN				(1 << 1)
	#define I2C_MBTFIEN				(1 << 2)
	#define I2C_MBRFIEN				(1 << 3)
	#define I2C_STARTIEN			(1 << 4)
	#define I2C_STOPIEN				(1 << 5)
	#define I2C_NAKRCVIEN			(1 << 6)

#define RK_REG_IPD				0x1c /* Interrupt pending */
	#define I2C_BTFIPD				(1 << 0)
	#define I2C_BRFIPD				(1 << 1)
	#define I2C_MBTFIPD				(1 << 2)
	#define I2C_MBRFIPD				(1 << 3)
	#define I2C_STARTIPD			(1 << 4)
	#define I2C_STOPIPD				(1 << 5)
	#define I2C_NAKRCVIPD			(1 << 6)
	#define I2C_IPD_ALL_CLEAN		0x7f

#define RK_REG_FCNT				0x20 /* Finished count */

#define RK_REG_TXDATA_BASE		0x100
#define RK_REG_TXDATA(i)		(RK_REG_TXDATA_BASE + (i) * 4)

#define RK_REG_RXDATA_BASE		0x200
#define RK_REG_RXDATA(i)		(RK_REG_RXDATA_BASE + (i) * 4)

#endif /* __ROCKCHIP_I2C_H__ */
