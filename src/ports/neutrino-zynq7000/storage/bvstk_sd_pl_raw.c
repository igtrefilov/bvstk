/* Exclusive PL SPI/DMA owner. Only the discovered FAT volume is exported.
 * DMA buffers are physical BRAM, mapped PROT_NOCACHE by the OS port.
 * Neutrino 2024 InterruptMask is ISR-safe (manual PDF p. 6137).
 */
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ports/neutrino-zynq7000/storage/bvstk_sd_pl_status.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h"
#include "services/sd/bvstk_sd_service.h"
#include "services/sd/bvstk_sd_disk.h"
#define SD_SECTOR_SIZE 512U
#define SD_IO_CHUNK 512U
#define SD_MAX_TRANSFER_BYTES (64U * 1024U)
static int owner_fd=-1;
static struct {
    pthread_mutex_t lock;
    iofunc_attr_t attr;
    bvstk_sd_master_t driver;
    bvstk_sd_service_t service;
    bvstk_neutrino_mutex_t mutex;
    bvstk_sd_disk_t disk;
    volatile unsigned masked, interrupts;
    int interrupt_id;
    int ready;
    uint32_t sector_count;
} g_sd;
typedef struct { io_read_t read; struct _xtype_offset offset; } bvstk_sd_read_offset_t;
typedef struct { io_write_t write; struct _xtype_offset offset; } bvstk_sd_write_offset_t;
static void barrier(void *ctx) { (void)ctx; __sync_synchronize(); }
static void disable(void *ctx)
{
    (void)ctx;
    if (__sync_bool_compare_and_swap(&g_sd.masked, 0, 1))
        (void)InterruptMask(BVSTK_IRQ_SD_CONTROLLER, g_sd.interrupt_id);
}
static const struct sigevent *interrupt_handler(void *ctx, int id)
{
    (void)ctx; (void)id;
    disable(NULL);
    __sync_add_and_fetch(&g_sd.interrupts, 1);
    return NULL;
}
static uint32_t count(void *ctx) { (void)ctx; return __sync_add_and_fetch(&g_sd.interrupts, 0); }
static bvstk_status_t prepare(void *ctx)
{
    (void)ctx;
    disable(NULL);
    if (bvstk_mmio_write32(&g_sd.driver.core, SDM_IRQ, SDM_ACK)) return BVSTK_ERR_IO;
    bvstk_platform_sleep_ms(1);
    __sync_lock_test_and_set(&g_sd.interrupts, 0);
    __sync_lock_test_and_set(&g_sd.masked, 0);
    return InterruptUnmask(BVSTK_IRQ_SD_CONTROLLER, g_sd.interrupt_id) < 0 ? BVSTK_ERR_IO : BVSTK_OK;
}
static int transfer(uint32_t sector, void *buffer, uint32_t blocks, int write)
{
    bvstk_status_t rc=write ? bvstk_sd_disk_write(&g_sd.disk,sector,buffer,blocks) :
                              bvstk_sd_disk_read(&g_sd.disk,sector,buffer,blocks);
    if (rc==BVSTK_OK) return 0;
    if (!g_sd.disk.ready) { g_sd.ready=0;g_sd.attr.nbytes=0; }
    errno=rc==BVSTK_ERR_RANGE?EINVAL:EIO;
    return -1;
}
static int sd_read_blocks(uint32_t sector, void *buf, uint32_t n) { return transfer(sector,buf,n,0); }
static int sd_write_blocks(uint32_t sector, void *buf, uint32_t n) { return transfer(sector,buf,n,1); }
static void sd_view_for_ocb(const iofunc_ocb_t *ocb, uint32_t *start, uint32_t *n)
{ (void)ocb; *start=0; *n=g_sd.sector_count; }
static int backend_init(void)
{
    /* Lifetime owner lock is acquired before touching shared SPI/DMA state. */
    owner_fd=open("/dev/shmem/bvstk-sd-pl-owner",O_CREAT|O_EXCL|O_RDWR,0600);
    if (owner_fd<0) {
        fprintf(stderr,"SD-PL: another process owns SPI/DMA\n");return -1;
    }
    if (ThreadCtl(_NTO_TCTL_IO, NULL) < 0) return -1;
    if (pthread_mutex_init(&g_sd.lock, NULL)) return -1;
    if (bvstk_neutrino_mutex_init(&g_sd.mutex) != BVSTK_OK) return -1;
    const bvstk_sd_master_config_t cfg = {
        .core_base=BVSTK_SPI_MASTER_BASE, .dma_base=BVSTK_SD_DMA_BASE,
        .bram_base=BVSTK_SPI_BRAM_BASE, .bram_size=BVSTK_SPI_BRAM_SIZE,
        .init_divider=512, .data_divider=512,
        .events={NULL,prepare,count,disable,barrier},
        .read_mailbox_is_stream=true, .read_descriptor_bytes=8
    };
    if (bvstk_sd_master_open(&g_sd.driver,&cfg,NULL) != BVSTK_OK) return -1;
    g_sd.interrupt_id = InterruptAttach(BVSTK_IRQ_SD_CONTROLLER,interrupt_handler,&g_sd,sizeof(g_sd),0);
    if (g_sd.interrupt_id < 0) return -1;
    disable(NULL);
    bvstk_sd_result_t result;
    bvstk_status_t rc=bvstk_sd_service_attach(&g_sd.service,&g_sd.driver,&g_sd.mutex.public_mutex,true);
    if (rc==BVSTK_OK) rc=bvstk_sd_disk_open(&g_sd.disk,&g_sd.service);
    result=g_sd.service.info.last;
    if (rc!=BVSTK_OK) {
        fprintf(stderr,"SD-PL: initialization/probe failed %s; no formatting\n",bvstk_status_string(rc));
        fprintf(stderr,"stage=%s irq_count=%u irq=%08x csr=%08x tx=%08x rx=%08x reply=%08x\n",
            result.hardware.stage ? result.hardware.stage : "unknown",count(NULL),result.hardware.irq,
            result.hardware.csr,result.hardware.tx_status,result.hardware.rx_status,result.hardware.reply);
        for (unsigned i=0;i<SDM_INIT_WORDS;++i) fprintf(stderr," init[%u]=%08x",i,result.hardware.init_reply[i]);
        fprintf(stderr,"\n");
        disable(NULL); (void)InterruptDetach(g_sd.interrupt_id); return -1;
    }
    g_sd.sector_count=g_sd.disk.volume.sectors;g_sd.ready=1;
    fprintf(stdout,"SD-PL: FAT%u LBA=%u sectors=%u IRQ=%u\n",g_sd.disk.volume.fat_bits,
        g_sd.disk.volume.first_lba,g_sd.disk.volume.sectors,count(NULL));
    return 0;
}
static int sd_io_read(resmgr_context_t *ctp, io_read_t *msg,
                      iofunc_ocb_t *ocb)
{
    uint32_t view_start_sector;
    uint32_t view_sector_count;
    uint64_t offset;
    uint64_t view_bytes;
    size_t requested;
    uint64_t available;
    size_t count;
    int update_ocb;
    size_t processed = 0U;
    uint8_t buffer[SD_IO_CHUNK];
    int verify;

    verify = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }
    sd_view_for_ocb(ocb, &view_start_sector, &view_sector_count);
    view_bytes = (uint64_t)view_sector_count * SD_SECTOR_SIZE;
    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_sd_read_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_sd_read_offset_t *)msg)->offset.offset;
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }
    if (offset > view_bytes) {
        return _RESMGR_ERRNO(ENOSPC);
    }
    if (offset == view_bytes) {
        return _IO_SET_READ_NBYTES(ctp, 0);
    }
    requested = (size_t)msg->i.nbytes;
    available = view_bytes - offset;
    if (requested > available) return _RESMGR_ERRNO(ENOSPC);
    count = requested;
    if (count == 0U || (offset % SD_SECTOR_SIZE) != 0U ||
        (count % SD_SECTOR_SIZE) != 0U) {
        return _RESMGR_ERRNO(EINVAL);
    }

    if (pthread_mutex_lock(&g_sd.lock) != 0) {
        return _RESMGR_ERRNO(EBUSY);
    }
    while (processed < count) {
        size_t chunk = count - processed;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (sd_read_blocks(view_start_sector +
                           (uint32_t)((offset + processed) / SD_SECTOR_SIZE),
                           buffer, (uint32_t)(chunk / SD_SECTOR_SIZE)) != 0 ||
            resmgr_msgwrite(ctp, buffer, (int)chunk, (int)processed) !=
            (int)chunk) {
            int saved_errno = errno == 0 ? EIO : errno;

            (void)pthread_mutex_unlock(&g_sd.lock);
            return _RESMGR_ERRNO(saved_errno);
        }
        processed += chunk;
    }
    (void)pthread_mutex_unlock(&g_sd.lock);
    if (update_ocb) {
        ocb->offset += (off_t)processed;
    }
    _IO_SET_READ_NBYTES(ctp, processed);
    return EOK;
}

static int sd_io_write(resmgr_context_t *ctp, io_write_t *msg,
                       iofunc_ocb_t *ocb)
{
    uint32_t view_start_sector;
    uint32_t view_sector_count;
    uint64_t offset;
    uint64_t view_bytes;
    size_t requested;
    uint64_t available;
    size_t count;
    size_t processed = 0U;
    size_t message_offset;
    int update_ocb;
    uint8_t buffer[SD_IO_CHUNK];
    int verify;

    verify = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }
    if ((ocb->ioflag & O_ACCMODE) == O_RDONLY) {
        return _RESMGR_ERRNO(EBADF);
    }
    sd_view_for_ocb(ocb, &view_start_sector, &view_sector_count);
    view_bytes = (uint64_t)view_sector_count * SD_SECTOR_SIZE;
    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        message_offset = sizeof(msg->i);
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_sd_write_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_sd_write_offset_t *)msg)->offset.offset;
        message_offset = sizeof(msg->i) + sizeof(struct _xtype_offset);
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }
    if (offset >= view_bytes) {
        return _RESMGR_ERRNO(ENOSPC);
    }
    requested = (size_t)msg->i.nbytes;
    available = view_bytes - offset;
    if (requested > available) return _RESMGR_ERRNO(ENOSPC);
    count = requested;
    if (count == 0U || (offset % SD_SECTOR_SIZE) != 0U ||
        (count % SD_SECTOR_SIZE) != 0U) {
        return _RESMGR_ERRNO(EINVAL);
    }

    if (pthread_mutex_lock(&g_sd.lock) != 0) {
        return _RESMGR_ERRNO(EBUSY);
    }
    while (processed < count) {
        size_t chunk = count - processed;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (resmgr_msgread(ctp, buffer, (int)chunk,
                           (int)message_offset + (int)processed) !=
                (int)chunk ||
            sd_write_blocks(view_start_sector +
                            (uint32_t)((offset + processed) / SD_SECTOR_SIZE),
                            buffer, (uint32_t)(chunk / SD_SECTOR_SIZE)) != 0) {
            int saved_errno = errno == 0 ? EIO : errno;

            (void)pthread_mutex_unlock(&g_sd.lock);
            return _RESMGR_ERRNO(saved_errno);
        }
        processed += chunk;
    }
    (void)pthread_mutex_unlock(&g_sd.lock);
    if (update_ocb) {
        ocb->offset += (off_t)processed;
    }
    _IO_SET_WRITE_NBYTES(ctp, processed);
    return EOK;
}


static int status_query(resmgr_context_t *ctp, io_devctl_t *msg, iofunc_ocb_t *ocb)
{
    int rc=iofunc_devctl_default(ctp,msg,ocb);
    if (rc!=(int)_RESMGR_DEFAULT) return rc;
    if (msg->i.dcmd!=BVSTK_SD_PL_STATUS) return ENOTTY;
    if (msg->i.nbytes!=(int)sizeof(bvstk_sd_pl_status_t)) return EINVAL;
    bvstk_sd_pl_status_t *out=_DEVCTL_DATA(msg->i);
    memset(out,0,sizeof(*out));
    out->version=1;out->ready=g_sd.ready;out->fat_bits=g_sd.disk.volume.fat_bits;
    out->first_lba=g_sd.disk.volume.first_lba;out->sectors=g_sd.disk.volume.sectors;
    out->irq_count=count(NULL);out->last_error=g_sd.service.info.last.status;
    out->last_lba=g_sd.service.info.last.lba;
    snprintf(out->stage,sizeof(out->stage),"%s",g_sd.service.info.last.hardware.stage ?
        g_sd.service.info.last.hardware.stage : "unknown");
    msg->o.ret_val=0;msg->o.nbytes=sizeof(*out);
    return _RESMGR_PTR(ctp,&msg->o,sizeof(msg->o)+sizeof(*out));
}
int main(void)
{
    if (backend_init()) {
        /* No disk was published. A later explicit startup may try again. */
        if (owner_fd>=0) { close(owner_fd);unlink("/dev/shmem/bvstk-sd-pl-owner"); }
        return EXIT_FAILURE;
    }
    dispatch_t *d=dispatch_create();
    if (!d) return EXIT_FAILURE;
    resmgr_attr_t ra={0};ra.nparts_max=2;ra.msg_max_size=SD_MAX_TRANSFER_BYTES;
    resmgr_connect_funcs_t connect;
    resmgr_io_funcs_t io;
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS,&connect,_RESMGR_IO_NFUNCS,&io);
    iofunc_attr_init(&g_sd.attr,S_IFREG|0660,NULL,NULL);
    g_sd.attr.nbytes=(off_t)((uint64_t)g_sd.sector_count*512U);
    io.read=sd_io_read;io.write=sd_io_write;io.devctl=status_query;
    if (resmgr_attach(d,&ra,"/dev/bvstk-sd-pl-fat",_FTYPE_ANY,0,&connect,&io,&g_sd.attr)<0)
        return EXIT_FAILURE;
    dispatch_context_t *ctx=dispatch_context_alloc(d);
    if (!ctx) return EXIT_FAILURE;
    for (;;) {
        ctx=dispatch_block(ctx);
        if (!ctx) return EXIT_FAILURE;
        dispatch_handler(ctx);
    }
}
