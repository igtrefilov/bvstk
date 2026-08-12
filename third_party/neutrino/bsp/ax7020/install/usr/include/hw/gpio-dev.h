/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __PUBLIC_GPIO_DEV_H__
#define __PUBLIC_GPIO_DEV_H__

#include <stdlib.h>
#include <stdio.h>

struct gpio_dev;
struct gpio_ocb;

#ifndef IOFUNC_ATTR_T

#define IOFUNC_ATTR_T       struct gpio_dev
#define IOFUNC_OCB_T        struct gpio_ocb
#define THREAD_POOL_PARAM_T dispatch_context_t

#endif /* IOFUNC_ATTR_T */

#include <sys/iofunc.h>
#include <sys/dispatch.h>

#define GPIO_DRV_PATH_NAME   "/dev/gpio"

/*
 * Resource manager struct
 */
typedef struct gpio_dev {
	iofunc_attr_t      hdr;
	dispatch_t         *dpp;
	dispatch_context_t *ctp;
	int                id;
	int                gpio_nums;
	void               *hdl;
} gpio_dev_t;

typedef struct gpio_ocb gpio_ocb_t;

#endif /* __PUBLIC_GPIO_DEV_H__ */
