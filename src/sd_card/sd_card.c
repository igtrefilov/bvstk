#include "sd_card.h"

#define SD_CARD_DEVICE_ID   XPAR_XSDPS_0_DEVICE_ID
#define BLOCK_SIZE          512

XSdPs SD_Ptr;
u8 ReadBuffer[BLOCK_SIZE];

s32 sd_init(XSdPs SD_Ptr){

	XSdPs_Config *Config;
	XSdPs SdInstance;
	int Curr_Status;
	u32 BlockCount = 1;
	u32 Arg = 0;

	xil_printf("Initializing SD card...\n");

	Config = XSdPs_LookupConfig(XPAR_XSDPS_0_DEVICE_ID);
	if (!Config) {
	    xil_printf("SD Config lookup failed\r\n");
	    return XST_FAILURE;
	}

	Curr_Status = XSdPs_CfgInitialize(&SdInstance, Config, Config->BaseAddress);
	if (Curr_Status != XST_SUCCESS) {
	    xil_printf("SD Initialization failed\r\n");
	    return XST_FAILURE;
	}

	Curr_Status = XSdPs_Select_Card(&SdInstance);
	if (Curr_Status != XST_SUCCESS) {
	    xil_printf("Error: Failed to select SD card\n");
	    return XST_FAILURE;
	}

	xil_printf("Reading data from SD card...\n");

	Curr_Status = XSdPs_ReadPolled(&SdInstance, Arg, BlockCount, ReadBuffer);
	if (Curr_Status != XST_SUCCESS) {
	    xil_printf("Error: Failed to read data from SD card.\n");
	    return XST_FAILURE;
	}

	xil_printf("Data read from SD card:\n");
	for (int i = 0; i < 16; i++) {
	    xil_printf("0x%02x ", ReadBuffer[i]);
	}
	xil_printf("\n");

	xil_printf("Read operation completed successfully.\n");

	return XST_SUCCESS;
}
