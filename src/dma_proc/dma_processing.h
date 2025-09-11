#ifndef DMA_PROCESSING_H
#define DMA_PROCESSING_H

#include <stdio.h>
#include <stdint.h>
#include "xparameters.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xaxidma.h"

// Definition of DMA Register Addresses
#define DMA_BASE_ADDR       	XPAR_AXI_DMA_0_BASEADDR
#define S2MM_DMACR          	(*(volatile uint32_t *)(DMA_BASE_ADDR + 0x30))
#define S2MM_DMASR          	(*(volatile uint32_t *)(DMA_BASE_ADDR + 0x34))
#define S2MM_DA             	(*(volatile uint32_t *)(DMA_BASE_ADDR + 0x48))
#define S2MM_DA_MSB         	(*(volatile uint32_t *)(DMA_BASE_ADDR + 0x4C))
#define S2MM_LENGTH         	(*(volatile uint32_t *)(DMA_BASE_ADDR + 0x58))

#define DDR_DMA_CURRENT_ADDR	0x1F000000

#define DMA_BUFFER_SIZE			4096
#define THREAD_STACKSIZE 		1024
#define BUFFER_SIZE 			2048
#define MAX_BUFFERS_COUNT 		2048

#define DMA_DEV_ID      XPAR_AXIDMA_0_DEVICE_ID // ID AXI DMA
#define DMA_INTR_ID     XPAR_FABRIC_AXIDMA_0_VEC_ID // ID AXI DMA

void dma_config(u32 *buffer, int length);
//void init_dma_intr(void);
void init_dma_intr(void *pvParameters);
void dma_intr_handler(void *CallbackRef);
void printBufferData(const u32 *buffer, uint16_t length);

#endif // DMA_PROCESSING_H
