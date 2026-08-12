/*
 * (c) 2025-2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __PUBLIC_GPIO_H__
#define __PUBLIC_GPIO_H__

#include <hw/gpio-dev.h>
#include <hw/gpio-user.h>

int      gpio_slogf(int severity, const char * const progname, const char *fmt, ...);

void     *gpio_init();
void     gpio_fini(void *hdl);
uint32_t gpio_get_value(void *hdl, int gpio_num);
uint32_t gpio_get_direction(void *hdl, int gpio_num);
int      gpio_cmd_set_input(void *hdl, gpio_input_t *pin);
int      gpio_cmd_set_output(void *hdl, gpio_output_t *pin);
int      gpio_cmd_read(void *hdl, gpio_read_t *buf);
int      gpio_cmd_write(void *hdl, gpio_write_t *buf);

#endif /* __PUBLIC_GPIO_H__ */
