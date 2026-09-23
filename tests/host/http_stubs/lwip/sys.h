#include <stdint.h>
typedef void *sys_thread_t;
uint32_t sys_now(void);
sys_thread_t sys_thread_new(const char *name, void (*fn)(void *), void *arg, int stack, int priority);
