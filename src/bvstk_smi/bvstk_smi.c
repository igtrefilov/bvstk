#include "bvstk_smi.h"

uint8_t phy_addr   = 0x01;

volatile uint32_t bram_master_data = 0;
volatile uint32_t bram_master_addr = 0;
volatile uint32_t bram_slave_data = 0;
volatile uint32_t bram_slave_addr = 0;
volatile uint32_t slave2host_data = 0;

XScuGic intc_inst;

void start_smi(void){
	int status = SetupInterruptSystem(&intc_inst);
	if (status != XST_SUCCESS) {
		xil_printf("Error setting up interrupts\n\r");
		return XST_FAILURE;
	}

	timeout_write(4321);
	timeout_read();
	while (1) {
		for (uint8_t i = 0; i < 32; i++) {
			mdio_read(phy_addr, i);
		}
		sleep(1);
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
    //xil_printf("[IRQ master]: CSR = 0x%08x,\t", csr);

    if ((csr & 0x28)) { // Если один из буферов заполнен, сбрасываем систему
        Xil_Out32(MASTER_BASEADDR + CSR_m, 0x01);
        //xil_printf("One of the buffers is full, you idiot\n\r");
        return;
    } else if (csr & 0x01) { // Если данные провалились в память
        bram_master_addr = Xil_In32(MASTER_BASEADDR + MEM_AADR_m);
        bram_master_data = Xil_In32(bram_master_addr);
        Xil_Out32(bram_master_addr + SLAVE_RD_OFFSET, bram_master_data); // Перекладываем эти данные в область slave read, чтобы host мог их прочитать
        Xil_Out32(MASTER_BASEADDR + IRQ_m, 1); // Сброс IRQ
        ///xil_printf("| addr = 0x%08x | data = 0x%08x\n\r", bram_master_addr, bram_master_data);
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

int SetupInterruptSystem(XScuGic *p_intc_inst) { // Прерывания
    XScuGic_Config *p_intc_config = XScuGic_LookupConfig(INTC_DEVICE_ID);
    XScuGic_CfgInitialize(p_intc_inst, p_intc_config, p_intc_config->CpuBaseAddress);
    if (NULL == p_intc_config) {
        return XST_FAILURE;
    }
    int status = XScuGic_CfgInitialize(p_intc_inst, p_intc_config, p_intc_config->CpuBaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,p_intc_inst);
    Xil_ExceptionEnable();

    XScuGic_Connect(p_intc_inst, IRQ_MASTER, (Xil_ExceptionHandler)master_ISR, (void *)NULL);
    XScuGic_Enable(p_intc_inst, IRQ_MASTER);
    XScuGic_Connect(p_intc_inst, IRQ_SLAVE, (Xil_ExceptionHandler)slave_ISR, (void *)NULL);
    XScuGic_Enable(p_intc_inst, IRQ_SLAVE);
    return XST_SUCCESS;
}
