/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

#ifndef __PUBLIC_GPIO_USER_H__
#define __PUBLIC_GPIO_USER_H__

#include <devctl.h>
/*
 * GPIO pin configuration information
 */
typedef struct {
	uint8_t     pin_num;
} gpio_input_t;

typedef struct {
	uint8_t     pin_num;
} gpio_output_t;

typedef struct {
	uint8_t     pin_num;
	uint8_t     data;
} gpio_read_t;

typedef struct {
	uint8_t     pin_num;
	uint8_t     data;
} gpio_write_t;

/*
 * The following devctls are used by a client application
 * to control the GPIO interface.
 */
#define _DCMD_GPIO           _DCMD_MISC

#define DCMD_GPIO_SET_INPUT  __DIOT(_DCMD_GPIO, 0, gpio_input_t)
#define DCMD_GPIO_SET_OUTPUT __DIOT(_DCMD_GPIO, 1, gpio_output_t)
#define DCMD_GPIO_WRITE      __DIOT(_DCMD_GPIO, 2, gpio_write_t)
#define DCMD_GPIO_READ       __DIOTF(_DCMD_GPIO, 3, gpio_read_t)

/*
 * GPIO API calls
 */
int gpio_close(int fd);
int	gpio_open(void);

int gpio_set_input(int fd, uint8_t pin_num);
int gpio_set_output(int fd, uint8_t pin_num);

int     gpio_write(int fd, uint8_t pin_num, uint8_t value);
uint8_t gpio_read(int fd, uint8_t pin_num);

#endif /* __PUBLIC_GPIO_USER_H__ */
