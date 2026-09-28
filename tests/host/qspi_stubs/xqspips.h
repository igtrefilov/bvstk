#ifndef TEST_QSPI_XQSPIPS_H
#define TEST_QSPI_XQSPIPS_H
#include "xil_types.h"
#define XQSPIPS_FORCE_SSELECT_OPTION 1U
#define XQSPIPS_HOLD_B_DRIVE_OPTION 2U
#define XQSPIPS_CLK_PRESCALE_8 2U
typedef struct { u32 BaseAddress; } XQspiPs_Config;
typedef struct { u32 unused; } XQspiPs;
XQspiPs_Config *XQspiPs_LookupConfig(u16 id);
int XQspiPs_CfgInitialize(XQspiPs *q, XQspiPs_Config *cfg, u32 base);
int XQspiPs_SelfTest(XQspiPs *q);
int XQspiPs_SetOptions(XQspiPs *q, u32 options);
void XQspiPs_SetClkPrescaler(XQspiPs *q, u8 prescaler);
void XQspiPs_SetSlaveSelect(XQspiPs *q);
int XQspiPs_PolledTransfer(XQspiPs *q, u8 *tx, u8 *rx, u32 count);
#endif
