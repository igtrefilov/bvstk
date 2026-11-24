/*------------------------------------------------------------------------*/
/* Sample code that implements the FatFs synchronization hooks with FreeRTOS */
/*------------------------------------------------------------------------*/

/* FatFs synchronization hooks for the FreeRTOS/Xilinx target. */
#include "ports/freertos-xilinx/fs-fatfs/bvstk_fatfs.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#if FF_USE_LFN == 3

void* ff_memalloc (UINT msize)
{
    return pvPortMalloc(msize);
}

void ff_memfree (void* mblock)
{
    vPortFree(mblock);
}

#endif

#if FF_FS_REENTRANT

/* R0.13b uses one open-object table across all volumes. Its operations span
 * multiple internal calls around disk I/O; per-volume mutexes cannot protect
 * the table. The mount path also clears table entries before FatFs locks. */
static SemaphoreHandle_t fatfs_mutex;

static SemaphoreHandle_t shared_mutex(void)
{
    vTaskSuspendAll();
    if (!fatfs_mutex) fatfs_mutex = xSemaphoreCreateRecursiveMutex();
    (void)xTaskResumeAll();
    return fatfs_mutex;
}

FRESULT bvstk_fatfs_mount(FATFS *fs, const TCHAR *path, BYTE immediate)
{
    SemaphoreHandle_t mutex = shared_mutex();
    if (!mutex) return FR_NOT_ENOUGH_CORE;
    if (xSemaphoreTakeRecursive(mutex, FF_FS_TIMEOUT) != pdTRUE) return FR_TIMEOUT;
    FRESULT rc = f_mount(fs, path, immediate);
    xSemaphoreGiveRecursive(mutex);
    return rc;
}

int ff_cre_syncobj (BYTE vol, FF_SYNC_t* sobj)
{
    (void)vol;
    *sobj = shared_mutex();
    return (int)(*sobj != NULL);
}

int ff_del_syncobj (FF_SYNC_t sobj)
{
    /* Other mounted volumes continue to use this process-lifetime mutex. */
    return sobj == fatfs_mutex;
}

int ff_req_grant (FF_SYNC_t sobj)
{
    return (int)(xSemaphoreTakeRecursive(sobj, FF_FS_TIMEOUT) == pdTRUE);
}

void ff_rel_grant (FF_SYNC_t sobj)
{
    xSemaphoreGiveRecursive(sobj);
}

#endif
