/*------------------------------------------------------------------------*/
/* Sample code that implements the FatFs synchronization hooks with FreeRTOS */
/*------------------------------------------------------------------------*/

#include "ff.h"
#include "FreeRTOS.h"
#include "semphr.h"

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

int ff_cre_syncobj (BYTE vol, FF_SYNC_t* sobj)
{
    (void)vol;
    *sobj = xSemaphoreCreateMutex();
    return (int)(*sobj != NULL);
}

int ff_del_syncobj (FF_SYNC_t sobj)
{
    vSemaphoreDelete(sobj);
    return 1;
}

int ff_req_grant (FF_SYNC_t sobj)
{
    return (int)(xSemaphoreTake(sobj, FF_FS_TIMEOUT) == pdTRUE);
}

void ff_rel_grant (FF_SYNC_t sobj)
{
    xSemaphoreGive(sobj);
}

#endif
