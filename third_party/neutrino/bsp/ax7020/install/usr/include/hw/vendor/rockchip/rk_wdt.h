/*
	(c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
*/

#ifndef __RK_WDT_H__
#define __RK_WDT_H__

#define RK_HWI_WDT          "wdog"

#define RK_WDT_SIZE         0x18

/* Register for selecting the timeout value */
#define RK_WDT_TORR         0x0004

/* Restart key */
#define RK_WDT_MAGIC_KEY    0x76

/* Register for restarting the WDT counter */
#define RK_WDT_CRR          0x000C

#define RK_WDT_PCLK         24000000

#define RK_WDT_COUNT_MIN   0
#define RK_WDT_COUNT_MAX   15

#define RK_WDT_ENABLE      0x1
#define RK_WDT_DISABLE     0x0

#endif /* __RK_WDT_H__ */
