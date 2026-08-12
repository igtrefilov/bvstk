/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __BAIKALT1_WDT_H__
#define __BAIKALT1_WDT_H__

#include <sys/types.h>
#include <sys/platform.h>
#include <baikalt1/baikalt1_intr.h>

/* Baikal T1 watchdog dev params */
#define BT1_WDT_BASE_ADDR          0x1F04C000
#define BT1_WDT_REG_SIZE           0x1000
#define BT1_WDT_IRQ                BT1_INTR_WDT
#define BT1_WDT_CLOCK_HZ           16000000

/* Watchdog registers */
#define BT1_WDT_CR                 0x00
#define BT1_WDT_TORR               0x04
#define BT1_WDT_CRR                0x0C

/* Watchdog registers values */
#define BT1_WDT_CR_EN              0x00
#define BT1_WDT_CR_RMOD            0x01
#define BT1_WDT_CR_RMOD_VAL        0x00
#define BT1_WDT_CR_RMOD_INTR       0x01
#define BT1_WDT_CRR_RESTART_VAL    0x76

#define BT1_WDT_CR_EN_ENABLE       0x01

#define BT1_WDT_TORR_TOP_INT_SHIFT 4
#define BT1_WDT_TORR_TOP_INT_MASK  _BITFIELD32L(BT1_WDT_TORR_TOP_INT_SHIFT, 0xf)

#define BT1_WDT_MODE_INTR          BT1_WDT_CR_RMOD_INTR
#define BT1_WDT_MODE_POLL          BT1_WDT_CR_RMOD_VAL

#define BT1_WDT_MAX_TOP            15

#define WDT_TOP_IN_SEC(value)      ((1U << (16 + (value))) / BT1_WDT_CLOCK_HZ)

#endif /* __BAIKALT1_WDT_H__ */
