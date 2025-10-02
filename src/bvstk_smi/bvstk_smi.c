#include "bvstk_smi.h"

uint8_t phy_addr   = 0x01;
uint32_t bram_master_data = 0;
uint32_t bram_master_addr = 0;
uint32_t bram_slave_data = 0;
uint32_t bram_slave_addr = 0;
uint32_t slave2host_data = 0;

void start_smi(void){
	smi_irq_install();
        if (xTaskCreate(smi_task,
                        "smi_task",
                        SMI_TASK_STACK_SIZE,
                        NULL,
                        SMI_TASK_PRIORITY,
                        NULL) != pdPASS) {
                xil_printf("Error creating SMI task\n\r");
        }
}

void smi_task(void *pvParameters)
{
        (void) pvParameters;

        timeout_write(4321);
        timeout_read();

        while (1) {
                for (uint8_t i = 0; i < 32; i++) {
                        mdio_read(phy_addr, i);
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                xil_printf("32 registers were read from PHY\n\r");
        }
}

void mdio_write(uint8_t phy, uint8_t reg, uint16_t data)
{
    uint32_t x = (uint32_t)data
                | ((uint32_t)(reg & 0x1F) << 16)
                | ((uint32_t)(phy & 0x1F) << 21)
                | ((uint32_t)1u << 26);
    Xil_Out32(MASTER_BASEADDR + TX_FIFO_m, x);
}

void mdio_read(uint8_t phy, uint8_t reg)
{
    uint32_t x = 0
                | ((uint32_t)(reg & 0x1F) << 16)
                | ((uint32_t)(phy & 0x1F) << 21)
                | ((uint32_t)0u << 26);
    Xil_Out32(MASTER_BASEADDR + TX_FIFO_m, x);
}

void timeout_write(uint16_t timeout)
{
    uint32_t x = 0
                | ((uint32_t)(timeout & 0x7FFF) << 15)
                | ((uint32_t)1u << 31);
    Xil_Out32(MASTER_BASEADDR + TIMEOUT_m, x);
    //xil_printf("\n\rTimeout = %lu us \n\r\n\r", timeout/100);
}

uint16_t timeout_read()
{
    uint16_t timeout = (Xil_In32(MASTER_BASEADDR + TIMEOUT_m) & 0x3FFF8000) >> 15;
    xil_printf("\n\rTimeout = %lu us \n\r\n\r", timeout/100);
    return timeout;
}

static void master_ISR(void *CallBackRef) {
    uint32_t csr = Xil_In32(MASTER_BASEADDR + CSR_m);
    xil_printf("[IRQ master]: CSR = 0x%08x,\t", csr);

    if ((csr & 0x28)) { // Если один из буферов заполнен, сбрасываем систему
        Xil_Out32(MASTER_BASEADDR + CSR_m, 0x01);
        xil_printf("One of the buffers is full, you idiot\n\r");
        return;
    } else if (csr & 0x01) { // Если данные провалились в память
        bram_master_addr = Xil_In32(MASTER_BASEADDR + MEM_AADR_m);
        bram_master_data = Xil_In32(bram_master_addr);
        Xil_Out32(bram_master_addr + SLAVE_RD_OFFSET, bram_master_data); // Перекладываем эти данные в область slave read, чтобы host мог их прочитать
        Xil_Out32(MASTER_BASEADDR + IRQ_m, 1); // Сброс IRQ
        xil_printf("| addr = 0x%08x | data = 0x%08x\n\r", bram_master_addr, bram_master_data);
        return;
    }
    //xil_printf("\n\r");
}

static void slave_ISR(void *CallBackRef) {
    uint32_t csr = Xil_In32(SLAVE_BASEADDR + CSR_s);

    if ((csr & 0x01)) { // Если хост записал данные, обрабатываем их и если всё норм, передаем их на запись в ядро master
        Xil_Out32(MASTER_BASEADDR + CSR_m, 0x01);
        bram_slave_addr = Xil_In32(SLAVE_BASEADDR + MEM_ADDR_s);
        bram_slave_data = Xil_In32(bram_slave_addr);
        uint8_t reg_addr = bram_slave_addr & 0x1F;
        mdio_write(phy_addr, reg_addr, bram_slave_data & 0xFFFF);
        //xil_printf("[IRQ slave]: | addr = 0x%08x | data = 0x%08x\n\r", bram_slave_addr, bram_slave_data);
    } else if (csr & 0x04) { // Если хост считал данные из BRAM, то просто оповещаем об этом проц
        slave2host_data = Xil_In32(SLAVE_BASEADDR + S2H);
        //xil_printf("slave2host_data = 0x%08x\n\r", slave2host_data);
    }
    //xil_printf("[IRQ slave]: | addr = 0x%08x | data = 0x%08x\n\r", bram_slave_addr, bram_slave_data);
    Xil_Out32(SLAVE_BASEADDR + IRQ_s, 1);
}

void smi_irq_install(void) {
    xPortInstallInterruptHandler(IRQ_MASTER, master_ISR, NULL);
    xPortInstallInterruptHandler(IRQ_SLAVE,  slave_ISR,  NULL);

    vPortEnableInterrupt(IRQ_MASTER);
    vPortEnableInterrupt(IRQ_SLAVE);
}



