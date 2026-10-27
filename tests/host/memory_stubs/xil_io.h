#ifndef TEST_MEMORY_XIL_IO_H
#define TEST_MEMORY_XIL_IO_H
#include <stdint.h>
typedef uintptr_t UINTPTR;
uint8_t Xil_In8(UINTPTR);
uint16_t Xil_In16(UINTPTR);
uint32_t Xil_In32(UINTPTR);
void Xil_Out8(UINTPTR, uint8_t);
void Xil_Out16(UINTPTR, uint16_t);
void Xil_Out32(UINTPTR, uint32_t);
#endif
