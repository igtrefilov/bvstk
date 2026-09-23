#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "hardware/pl/i2c/bvstk_i2c_regs.h"

static uint64_t now, started;
static unsigned starts, resets, clears, bram_reads;
static int mode, active;
static uint32_t irq;
enum { REPLY, NACK, NO_REPLY };

uint64_t bvstk_platform_now_ms(void) { return now; }
void bvstk_platform_sleep_ms(uint32_t ms) { now += ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t base, size_t size)
{
    memset(r,0,sizeof(*r));r->physical_base=base;r->size=size;return 0;
}
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { memset(r,0,sizeof(*r)); }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r,size_t offset,uint32_t *value)
{
    if (r->physical_base==BVSTK_I2C_BRAM_BASE) {
        assert(offset==BVSTK_I2C_BRAM_MASTER_OFFSET+4U);
        assert(mode==REPLY && active && now-started>=4U);
        bram_reads++;*value=0x5a;return 0;
    }
    assert(r->physical_base==BVSTK_I2C_MASTER_BASE);
    if (active && mode==NACK && now-started>=1U) irq|=BVSTK_I2C_MSTR_IRQ_ERR_ACK;
    if (active && mode==REPLY && now-started>=4U) irq|=BVSTK_I2C_MSTR_IRQ_DATA_VALID;
    if (offset==BVSTK_I2C_MSTR_CSR_OFFSET) *value=active && now-started<2U ? 1U : 0U;
    else { assert(offset==BVSTK_I2C_MSTR_IRQ_OFFSET);*value=irq; }
    return 0;
}
int bvstk_mmio_write32(const bvstk_mmio_region_t *r,size_t offset,uint32_t value)
{
    assert(r->physical_base==BVSTK_I2C_MASTER_BASE);
    if (offset==BVSTK_I2C_MSTR_IRQ_OFFSET) {
        assert(value==BVSTK_I2C_MSTR_IRQ_RESET_BIT);irq=0;clears++;
    } else if (offset==BVSTK_I2C_MSTR_CSR_OFFSET) {
        if (value & BVSTK_I2C_MSTR_SOFT_RESET_BIT) {active=0;irq=0;resets++;}
        else {assert(value==BVSTK_I2C_MSTR_START_BIT);active=1;started=now;starts++;}
    } else assert(offset==BVSTK_I2C_MSTR_TX_FIFO_OFFSET);
    return 0;
}
static void reset_case(int next_mode)
{
    mode=next_mode;active=0;starts=resets=clears=bram_reads=0;now=started=0;
    irq=BVSTK_I2C_MSTR_IRQ_DATA_VALID; /* stale reply from another device */
}
int main(void)
{
    bvstk_i2c_master_hw_t hw;uint8_t value;
    assert(bvstk_i2c_master_hw_init(&hw,NULL,NULL)==BVSTK_OK);
    reset_case(REPLY);value=0xcc;
    assert(bvstk_i2c_master_hw_read_reg(&hw,0x36,0,&value,10)==BVSTK_OK);
    assert(value==0x5a && starts==1 && clears==1 && bram_reads==1 && now>=4);
    reset_case(NACK);value=0xcc;
    assert(bvstk_i2c_master_hw_read_reg(&hw,0x41,0,&value,10)==BVSTK_ERR_IO);
    assert(value==0xcc && starts==1 && clears==1 && bram_reads==0 && resets==1);
    reset_case(NACK);
    assert(bvstk_i2c_master_hw_write_reg(&hw,0x41,1,1,10)==BVSTK_ERR_IO);
    assert(starts==1 && resets==1); /* no automatic write replay after NACK */
    reset_case(NO_REPLY);value=0xcc;
    assert(bvstk_i2c_master_hw_read_reg(&hw,0x41,0,&value,5)==BVSTK_ERR_TIMEOUT);
    assert(value==0xcc && starts==3 && clears==3 && bram_reads==0);
    reset_case(REPLY);value=0xcc;
    assert(bvstk_i2c_master_hw_read_reg(&hw,0x36,0,&value,10)==BVSTK_OK);
    assert(value==0x5a);
    bvstk_i2c_master_hw_shutdown(&hw);
    puts("I2C master tests passed: fresh BRAM reply, NACK, no stale data, bounded timeout, recovery");
    return 0;
}
