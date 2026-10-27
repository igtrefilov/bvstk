#ifndef TEST_MEMORY_NEUTRINO_HOST_H
#define TEST_MEMORY_NEUTRINO_HOST_H
#include <stddef.h>
#include <stdint.h>
void *mmap_device_memory(void *, size_t, int, int, uint64_t);
#define PROT_NOCACHE 0
#endif
