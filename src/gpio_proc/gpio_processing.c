#include "gpio_processing.h"

XGpio GpioBtn;
TaskHandle_t xTaskButtonHandle;

void blink_LED(void *p) {
    uint32_t led_state = 0x00000000;
    Xil_Out32(GPIO_BASE_ADDR + GPIO_TRI_OFFSET, 0x00000000);

    while (1) {
        led_state = ~led_state;
        Xil_Out32(GPIO_BASE_ADDR + GPIO_DATA_OFFSET, led_state);
        vTaskDelay(100);
    }
}

void TaskButton(void *pvParameters) {

	XGpio_Initialize(&GpioBtn, BTN_DEVICE_ID);
    XGpio_SetDataDirection(&GpioBtn, GPIO_CHANNEL1, 0xFFFFFFFF);
    XGpio_InterruptEnable(&GpioBtn, GPIO_CHANNEL1);
    XGpio_InterruptGlobalEnable(&GpioBtn);

    xPortInstallInterruptHandler(BTN_INTR_ID, ButtonInterruptHandler, &GpioBtn);
    vPortEnableInterrupt(BTN_INTR_ID);

    vTaskDelete(xTaskButtonHandle);
}

void ButtonInterruptHandler(void *CallbackRef) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    XGpio_InterruptClear(&GpioBtn, GPIO_CHANNEL1);

    xil_printf("Button Pressed!\n");

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
