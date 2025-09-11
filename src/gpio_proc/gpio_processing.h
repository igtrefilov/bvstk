#ifndef GPIO_PROCESSING_H
#define GPIO_PROCESSING_H

#include "xil_types.h"
#include "xil_cache.h"
#include "xgpio.h"
#include "FreeRTOS.h"
#include "task.h"

//LED Blink defines
#define GPIO_BASE_ADDR XPAR_AXI_GPIO_1_BASEADDR
#define GPIO_DATA_OFFSET 0x00
#define GPIO_TRI_OFFSET  0x04

//BTN
#define GPIO_CHANNEL1 1
#define BTN_DEVICE_ID         XPAR_AXI_GPIO_1_DEVICE_ID
#define BTN_INTR_ID           XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR
#define BTN_CHANNEL           1

void blink_LED(void *p);
void TaskButton(void *pvParameters);
void ButtonInterruptHandler(void *CallbackRef);

#endif // GPIO_PROCESSING_H
