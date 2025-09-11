#include "dma_processing.h"

XAxiDma DMAInstance;
TaskHandle_t DMA_intr_handle;
QueueHandle_t dmaQueue;
u32 *currentDMABuffer = NULL;
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
XAxiDma *InstancePtr;
u32 IrqStatus;

u32 *allocated_buffers[MAX_BUFFERS_COUNT];
volatile int current_buff_index;

struct {
    u32 *buffer;
    uint16_t length;
} dmaData;

/*void print_rcv_bytes() {
    if (currentDMABuffer == NULL || dmaData.length == 0) {
        xil_printf("No data to print or buffer is NULL\r\n");
        return;
    }

    xil_printf("Received bytes (%d):\r\n", dmaData.length);
    for (int i = 0; i < dmaData.length; i++) {
        xil_printf("%02X ", currentDMABuffer[i]);
        if ((i + 1) % 16 == 0) {
            xil_printf("\r\n");
        }
    }
    xil_printf("\r\n");
}*/

void alloc_buffer_init(void){
	for (int i = 0; i < MAX_BUFFERS_COUNT; i++) {
	    allocated_buffers[i] = malloc(DMA_BUFFER_SIZE);
	    //xil_printf("alloc buff: %d %08X\r\n", i, allocated_buffers[i]);
	    if (allocated_buffers[i] == NULL) {
	        //xil_printf("Memory allocation failed for buffer %d\r\n", i);
	    }else{
	    	//xil_printf("Memory allocation %d. - success!\r\n", i);
	    }
	}
}

void dma_config(u32 *buffer, int length) {
    Xil_DCacheDisable();
    S2MM_DMACR |= 0x1;
    S2MM_DA = (u32)buffer;
    //S2MM_DA = 0x1F000000;
    S2MM_LENGTH = length;
}

void init_dma_intr(void *pvParameters) {
    int Status;
    XAxiDma_Config *Config;

    current_buff_index = 0;
    alloc_buffer_init();
    Config = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (Config == NULL) {
        xil_printf("No config found for %d\r\n", DMA_DEV_ID);
        vTaskDelete(NULL);
    }

    Status = XAxiDma_CfgInitialize(&DMAInstance, Config);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA initialization failed\r\n");
        vTaskDelete(NULL);
    }

    if (XAxiDma_HasSg(&DMAInstance)) {
        xil_printf("Device configured as SG mode\r\n");
        vTaskDelete(NULL);
    }

    XAxiDma_IntrDisable(&DMAInstance, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrEnable(&DMAInstance, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    xPortInstallInterruptHandler(DMA_INTR_ID, dma_intr_handler, &DMAInstance);
    vPortEnableInterrupt(DMA_INTR_ID);

    currentDMABuffer = allocated_buffers[0];

    dma_config(currentDMABuffer, DMA_BUFFER_SIZE);
    vTaskDelete(NULL);

}

void dma_intr_handler(void *CallbackRef) {
    xHigherPriorityTaskWoken = pdFALSE;
    InstancePtr = (XAxiDma *)CallbackRef;

    //xil_printf("interrupt occur! \r\n");
    IrqStatus = XAxiDma_IntrGetIrq(InstancePtr, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrAckIrq(InstancePtr, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

    if (IrqStatus & XAXIDMA_IRQ_ERROR_MASK) {
        //xil_printf("DMA error interrupt occurred. Resetting DMA...\n");
        XAxiDma_Reset(InstancePtr);
        while (!XAxiDma_ResetIsDone(InstancePtr)) {}
        return;
    }

    if (IrqStatus & XAXIDMA_IRQ_IOC_MASK) {
        u32 receivedBytes = XAxiDma_ReadReg(InstancePtr->RegBase, 0x58);

        if (dmaQueue == NULL) {
            xil_printf("DMA queue not ready, data skipped\r\n");
            return;
        }

        dmaData.buffer = currentDMABuffer;
        dmaData.length = receivedBytes;

        //print_rcv_bytes();

        if(dmaQueue!=NULL){
			BaseType_t result = xQueueSendFromISR(dmaQueue, &dmaData, &xHigherPriorityTaskWoken);
			if (result != pdPASS) {
				xil_printf("Error: Failed to send DMA data to queue\r\n");
				//vPortFree(currentDMABuffer);
			}
			if(current_buff_index > MAX_BUFFERS_COUNT){
				current_buff_index = 0;
			}
			currentDMABuffer = allocated_buffers[current_buff_index];
			current_buff_index++;
			//xil_printf("current_index: %d\r\n", current_buff_index);
			//xil_printf("currentDMABuffer: %08X\r\n", currentDMABuffer);
			dma_config(currentDMABuffer, DMA_BUFFER_SIZE);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
