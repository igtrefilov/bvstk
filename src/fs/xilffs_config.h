#ifndef XILFFS_CONFIG_H
#define XILFFS_CONFIG_H

#include "xparameters.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifndef HANDLE
typedef SemaphoreHandle_t HANDLE;
#endif

#ifdef FILE_SYSTEM_USE_LFN
#undef FILE_SYSTEM_USE_LFN
#endif
#define FILE_SYSTEM_USE_LFN 3

#endif /* XILFFS_CONFIG_H */
