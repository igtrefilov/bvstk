/*
 *
 * (c) 2025-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#ifndef __RPI4B_GPIO_H__
#define __RPI4B_GPIO_H__

#include <sys/bitfield.h>

#define BCM2711_GPIO_SIZE_REGISTR_BYTES     (4)
#define BCM2711_GPIO_SIZE_REGISTR_BITS      (32)
#define BCM2711_GPIO_BASE                   (0xFE200000UL) /* Low peripheral mode */

/* Function */
#define BCM2711_GPIO_GPFSEL0                (0x00UL)  /* GPIO 0..9   */
#define BCM2711_GPIO_GPFSEL1                (0x04UL)  /* GPIO 10..19 */
#define BCM2711_GPIO_GPFSEL2                (0x08UL)  /* GPIO 20..29 */
#define BCM2711_GPIO_GPFSEL3                (0x0CUL)  /* GPIO 30..39 */
#define BCM2711_GPIO_GPFSEL4                (0x10UL)  /* GPIO 40..49 */
#define BCM2711_GPIO_GPFSEL5                (0x14UL)  /* GPIO 50..53 */
#define BCM2711_GPIO_FUNC_NUM_REG           (6)
#define BCM2711_GPIO_FUNC_NUM_BITS          (3)
#define BCM2711_GPIO_FUNC_MASK              (__MASK8(3))
#define BCM2711_GPIO_FUNC_INPUT             (0)
#define BCM2711_GPIO_FUNC_OUTPUT            (__BIT8(0))
#define BCM2711_GPIO_FUNC_ALT_0             (__BIT8(2))
#define BCM2711_GPIO_FUNC_ALT_1             (__BIT8(0) | __BIT8(2))
#define BCM2711_GPIO_FUNC_ALT_2             (__BIT8(1) | __BIT8(2))
#define BCM2711_GPIO_FUNC_ALT_3             (__BIT8(0) | __BIT8(1) | __BIT8(2))
#define BCM2711_GPIO_FUNC_ALT_4             (__BIT8(0) | __BIT8(1))
#define BCM2711_GPIO_FUNC_ALT_5             (__BIT8(1))

/* Output Set */
#define BCM2711_GPIO_GPSET0                 (0x1CUL)  /* GPIO 0..31  */
#define BCM2711_GPIO_GPSET1                 (0x20UL)  /* GPIO 32..53 */
#define BCM2711_GPIO_SET_NUM_REG            (2)
#define BCM2711_GPIO_SET_NUM_BITS           (1)

/* Output Clear */
#define BCM2711_GPIO_GPCLR0                 (0x28UL)  /* GPIO 0..31  */
#define BCM2711_GPIO_GPCLR1                 (0x2CUL)  /* GPIO 32..53 */
#define BCM2711_GPIO_CLR_NUM_REG            (2)
#define BCM2711_GPIO_CLR_NUM_BITS           (1)

/* Pin Level */
#define BCM2711_GPIO_GPLEV0                 (0x34UL)  /* GPIO 0..31  */
#define BCM2711_GPIO_GPLEV1                 (0x38UL)  /* GPIO 32..53 */
#define BCM2711_GPIO_LEV_NUM_REG            (2)
#define BCM2711_GPIO_LEV_NUM_BITS           (1)

/* Event Detect Status */
#define BCM2711_GPIO_GPEDS0                 (0x40UL)
#define BCM2711_GPIO_GPEDS1                 (0x44UL)

/* Rising Edge Detect Enable */
#define BCM2711_GPIO_GPREN0                 (0x4CUL)
#define BCM2711_GPIO_GPREN1                 (0x50UL)

/* Falling Edge Detect Enable */
#define BCM2711_GPIO_GPFEN0                 (0x58UL)
#define BCM2711_GPIO_GPFEN1                 (0x5CUL)

/* High Detect Enable */
#define BCM2711_GPIO_GPHEN0                 (0x64UL)
#define BCM2711_GPIO_GPHEN1                 (0x68UL)

/* Low Detect Enable */
#define BCM2711_GPIO_GPLEN0                 (0x70UL)
#define BCM2711_GPIO_GPLEN1                 (0x74UL)

/* Async Rising Edge Detect Enable */
#define BCM2711_GPIO_GPAREN0                (0x7CUL)
#define BCM2711_GPIO_GPAREN1                (0x80UL)

/* Async Falling Edge Detect Enable */
#define BCM2711_GPIO_GPAFEN0                (0x88UL)
#define BCM2711_GPIO_GPAFEN1                (0x8CUL)

/* GPIO Pull-up / Pull-down Registers */
#define BCM2711_GPIO_PUP_PDN_CNTRL_REG0     (0xE4UL)
#define BCM2711_GPIO_PUP_PDN_CNTRL_REG1     (0xE8UL)
#define BCM2711_GPIO_PUP_PDN_CNTRL_REG2     (0xECUL)
#define BCM2711_GPIO_PUP_PDN_CNTRL_REG3     (0xF0UL)
#define BCM2711_GPIO_PUP_NUM_REG            (4)
#define BCM2711_GPIO_PUP_NUM_BITS           (2)
#define BCM2711_GPIO_PUP_MASK               (__MASK8(2))
#define BCM2711_GPIO_PUP_UP                 (__BIT8(0))
#define BCM2711_GPIO_PUP_DOWN               (__BIT8(1))

#endif /* __RPI4B_GPIO_H__ */
