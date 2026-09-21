#include "apps/freertos/diagnostics/pl_spi_test/tests.h"
#include "apps/freertos/diagnostics/pl_spi_test/config.h"
#include "apps/freertos/diagnostics/pl_spi_test/driver.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"

typedef enum { PASS, FAIL, SKIP, BLOCKED } verdict_t;
static unsigned totals[4];
static pl_spi_t device;
static bool block_addressing;
static uint8_t baseline[SPI_BLOCK_RX];
static uint8_t received[SPI_BLOCK_RX];
static uint8_t pattern[SPI_BLOCK_BYTES];
static uint8_t neighbours[2][SPI_BLOCK_RX];

static void result(const char *id, verdict_t verdict, const char *reason)
{
    static const char *const names[] = { "PASS", "FAIL", "SKIP", "BLOCKED" };
    ++totals[verdict];
    xil_printf("RESULT %s %s %s\r\n", id, names[verdict], reason);
}

static bool check(const char *id, bool passed, const char *reason)
{
    result(id, passed ? PASS : FAIL, reason);
    return passed;
}

static void value(const char *name, uint32_t expected, uint32_t actual)
{
    xil_printf("  %s expected=%08x actual=%08x\r\n", name,
                (unsigned)expected, (unsigned)actual);
}

static void observation(const pl_spi_observation_t *o, pl_spi_result_t status)
{
    xil_printf("  transport=%s elapsed_ms=%u CSR=%08x IRQ=%08x seen=%08x GIC=%u\r\n",
                pl_spi_result_name(status), (unsigned)o->elapsed_ms,
                (unsigned)o->csr, (unsigned)o->irq, (unsigned)o->irq_seen,
                (unsigned)o->irq_count);
    xil_printf("  DMA TX CR=%08x SR=%08x RX CR=%08x SR=%08x LENGTH=%u\r\n",
                (unsigned)o->tx_control, (unsigned)o->tx_status,
                (unsigned)o->rx_control, (unsigned)o->rx_status,
                (unsigned)o->rx_length);
    xil_printf("  response=%08x buffer_guards=%s\r\n", (unsigned)o->response,
                o->guards_ok ? "intact" : "CHANGED");
}

static bool software_checks(void)
{
    static const uint8_t cmd0[] = { 0x40U, 0U, 0U, 0U, 0U };
    static const uint8_t cmd8[] = { 0x48U, 0U, 0U, 1U, 0xAAU };
    static const uint8_t digits[] = "123456789";
    uint32_t words[2];
    bool ok = pl_spi_crc7(cmd0, sizeof(cmd0)) == 0x95U &&
              pl_spi_crc7(cmd8, sizeof(cmd8)) == 0x87U &&
              pl_spi_crc16(digits, 9U) == 0x31C3U &&
              pl_spi_crc32(digits, 9U) == UINT32_C(0xCBF43926);
    pl_spi_encode_command(8U, 0x1AAU, words);
    ok = ok && words[0] == UINT32_C(0xAA010000) && words[1] == 0x8748U;
    pl_spi_encode_command(16U, 512U, words);
    ok = ok && words[0] == 0x00020000U && (words[1] & 0xFFU) == 0x50U;
    return check("SW.ENCODING", ok, "CRC known vectors and SD descriptor byte order");
}

static bool access_checks(void)
{
    uint32_t first, second;
    pl_spi_core_reset(&device);
    if (pl_spi_dma_reset(&device) != PL_SPI_OK) {
        result("ACCESS.DMA", FAIL, "DMA reset did not complete; stop hardware tests");
        return false;
    }
    pl_spi_write(&device, SPI_DELAY, 1U);
    first = pl_spi_read(&device, SPI_DELAY);
    pl_spi_write(&device, SPI_DELAY, 0x00010501U);
    second = pl_spi_read(&device, SPI_DELAY);
    value("DELAY first", 1U, first);
    value("DELAY second", 0x00010501U, second);
    if (!check("ACCESS.CORE", first == 1U && second == 0x00010501U,
                "two distinct documented register values at the BSP address")) {
        return false;
    }
    pl_spi_bram_write(&device, SPI_TX_AREA, 0x13579BDFU);
    first = pl_spi_bram_read(&device, SPI_TX_AREA);
    pl_spi_bram_write(&device, SPI_TX_AREA, 0xECA86420U);
    second = pl_spi_bram_read(&device, SPI_TX_AREA);
    value("BRAM first", 0x13579BDFU, first);
    value("BRAM second", 0xECA86420U, second);
    return check("ACCESS.BRAM", first == 0x13579BDFU && second == 0xECA86420U,
                  "two complementary BRAM patterns");
}

static void register_checks(void)
{
    static const uint32_t packets[] = { 0x2DU, 0x2EU, 0x2FU, 0xFFFFFFFEU };
    static const uint32_t delays[] = { 1U, 2U, 0x80000001U, 0xFFFFFFFFU };
    uint32_t offset, actual, csr;
    size_t i;
    bool ok;
    pl_spi_observation_t o = { 0 };
    pl_spi_result_t status;

    pl_spi_core_reset(&device);
    csr = pl_spi_read(&device, SPI_CSR) & 0xFU;
    value("reset FIFO flags (saved page)", 0xAU, csr);
    check("REG.RESET_STATUS", csr == 0xAU, "CSR empty/full fields per saved specification");
    ok = pl_spi_read(&device, SPI_PACKET) == 0U &&
         pl_spi_read(&device, SPI_DELAY) == 0U;
    check("REG.RESET_CONFIG", ok, "readable configuration registers reset to zero");

    ok = true;
    for (i = 0U; i < sizeof(packets) / sizeof(packets[0]); ++i) {
        pl_spi_write(&device, SPI_PACKET, packets[i]);
        actual = pl_spi_read(&device, SPI_PACKET);
        value("PACKET", packets[i], actual);
        ok = ok && actual == packets[i];
    }
    check("REG.PACKET", ok, "mode in bits 1:0; count in bits 31:2 on readback");
    ok = true;
    for (i = 0U; i < sizeof(delays) / sizeof(delays[0]); ++i) {
        pl_spi_write(&device, SPI_DELAY, delays[i]);
        actual = pl_spi_read(&device, SPI_DELAY);
        value("DELAY", delays[i], actual);
        ok = ok && actual == delays[i];
    }
    check("REG.DELAY", ok, "all 32 delay bits retained; start is disabled");

    for (offset = 0U; offset < device.bram_size; offset += 4U) {
        pl_spi_bram_write(&device, offset, SPI_SENTINEL ^ offset);
    }
    pl_spi_core_reset(&device);
    ok = true;
    for (offset = 0U; offset < device.bram_size; offset += 4U) {
        actual = pl_spi_bram_read(&device, offset);
        if (actual != (SPI_SENTINEL ^ offset)) {
            xil_printf("  BRAM mismatch at +%04x\r\n", (unsigned)offset);
            value("word", SPI_SENTINEL ^ offset, actual);
            ok = false;
            break;
        }
    }
    check("REG.RESET_BRAM", ok, "every BRAM word retained across core reset");
    check("REG.RESET_WRITTEN", pl_spi_read(&device, SPI_PACKET) == 0U &&
          pl_spi_read(&device, SPI_DELAY) == 0U, "written configuration cleared by reset");

    status = pl_spi_dma_reset(&device);
    pl_spi_observe(&device, &o);
    observation(&o, status);
    check("DMA.RESET", status == PL_SPI_OK &&
          ((o.tx_control | o.rx_control) & (DMA_RUN | DMA_RESET)) == 0U &&
          (o.tx_status & DMA_HALTED) != 0U && (o.rx_status & DMA_HALTED) != 0U &&
          ((o.tx_status | o.rx_status) & (DMA_ERRORS | DMA_IOC)) == 0U,
          "both channels halted, reset complete, no stale completion/error");
}

static bool raw_rx_untouched(void)
{
    uint32_t offset;
    if (device.rx_capacity == 0U) return false;
    for (offset = 0U; offset < device.rx_capacity; offset += 4U) {
        if (pl_spi_bram_read(&device, SPI_RX_AREA + offset) != SPI_SENTINEL) {
            xil_printf("  unexpected RX data at +%04x\r\n", (unsigned)offset);
            return false;
        }
    }
    return true;
}

static void raw_checks(void)
{
    static const uint32_t single[] = { 0x40123456U };
    static const uint32_t many[] = { 0x40000000U, 0x40951234U, 0x5678ABCDU };
    static const uint32_t idle[] = { 0xFFFFFFFFU };
    static const struct { const char *id; uint32_t mode, count, bytes; } cases[] = {
        { "FIFO.SINGLE", SPI_SINGLE, 11U, 1U },
        { "FIFO.COUNTED", SPI_COUNTED, 11U, 11U },
        { "FIFO.STREAM", SPI_STREAM, 11U, 12U }
    };
    pl_spi_observation_t o;
    pl_spi_result_t status;
    bool progress, ok;
    size_t i;

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        status = pl_spi_raw(&device, cases[i].mode, cases[i].count,
                            i == 0U ? single : many, i == 0U ? 1U : 3U,
                            true, true, &o);
        observation(&o, status);
        value("received bytes", cases[i].bytes, o.rx_length);
        ok = status == PL_SPI_OK && o.rx_length == cases[i].bytes && o.guards_ok;
        if (cases[i].mode == SPI_STREAM) {
            ok = ok && (o.irq_seen & SPI_DOC_IRQ_END) == 0U;
        } else {
            ok = ok && (o.irq_seen & SPI_DOC_IRQ_END) != 0U;
        }
        check(cases[i].id, ok, "legacy FIFO example: received extent and documented end flag");
    }
    status = pl_spi_raw(&device, SPI_COUNTED, 4U, idle, 1U, true, true, &o);
    observation(&o, status);
    progress = check("FIFO.READ_ENABLED", status == PL_SPI_OK &&
                       o.rx_length == 4U && o.guards_ok, "positive receive control");
    if (!progress) {
        result("FIFO.START_INHIBIT", BLOCKED, "positive receive control failed");
        result("FIFO.WRITE_ONLY", BLOCKED, "positive receive control failed");
    } else {
        status = pl_spi_raw(&device, SPI_COUNTED, 4U, idle, 1U, true, false, &o);
        observation(&o, status);
        check("FIFO.START_INHIBIT", status == PL_SPI_OK &&
              (o.rx_status & DMA_IOC) == 0U && (o.irq_seen & SPI_DOC_IRQ_END) == 0U &&
              raw_rx_untouched() && o.guards_ok,
              "no PS-visible receive/completion with start=0");
        status = pl_spi_raw(&device, SPI_COUNTED, 4U, idle, 1U, false, true, &o);
        observation(&o, status);
        check("FIFO.WRITE_ONLY", status == PL_SPI_OK &&
              (o.rx_status & DMA_IOC) == 0U &&
              raw_rx_untouched() && o.guards_ok,
              "transmit completion without receive data when read_en=0");
    }
    pl_spi_core_reset(&device);
    (void)pl_spi_dma_reset(&device);
}

static void tx_fifo_boundary(void)
{
    uint32_t words = 0U, csr;
    pl_spi_observation_t o = { 0 };
    pl_spi_core_reset(&device);
    pl_spi_write(&device, SPI_CSR, 0U);
    pl_spi_irq_prepare(&device);
    csr = pl_spi_read(&device, SPI_CSR);
    while ((csr & SPI_DOC_TX_FULL) == 0U && words < PL_SPI_TEST_TX_FIFO_WORDS) {
        pl_spi_write(&device, SPI_TX_FIFO, UINT32_MAX);
        ++words;
        csr = pl_spi_read(&device, SPI_CSR);
    }
    pl_spi_delay_ms(1U);
    pl_spi_observe(&device, &o);
    csr = o.csr;
    xil_printf("  TX FIFO fill words=%u XSA depth=%u CSR=%08x\r\n",
                (unsigned)words, (unsigned)PL_SPI_TEST_TX_FIFO_WORDS, (unsigned)csr);
    observation(&o, PL_SPI_OK);
    if (check("FIFO.TX_FULL", words != 0U && words <= PL_SPI_TEST_TX_FIFO_WORDS &&
                (csr & SPI_DOC_TX_FULL) != 0U,
                "FIFO reaches full with start=0; no write submitted after full")) {
        check("IRQ.TX_FULL", (o.irq_seen & SPI_DOC_IRQ_TX_FULL) != 0U && o.irq_count != 0U,
              "documented FIFO-full cause and delivery to PS");
    } else {
        result("IRQ.TX_FULL", BLOCKED, "documented full condition not observed");
    }
    pl_spi_core_reset(&device);
}

static bool init_valid(pl_spi_result_t status, const pl_spi_observation_t *o)
{
    size_t i;
    if (status != PL_SPI_OK) return false;
    for (i = 0U; i < SPI_INIT_WORDS; ++i) {
        if (o->mailbox[i] == SPI_SENTINEL) return false;
    }
    return o->mailbox[MB_CMD0 / 4U] == 1U &&
        o->mailbox[MB_CMD8_R1 / 4U] == 1U &&
        o->mailbox[MB_CMD8_DATA / 4U] == 0x1AAU &&
        o->mailbox[MB_ACMD41 / 4U] == 0U &&
        o->mailbox[MB_CMD55 / 4U] <= 1U &&
        o->mailbox[MB_CMD58_R1 / 4U] == 0U &&
        (o->mailbox[MB_CMD58_OCR / 4U] & 0x80000000U) != 0U &&
        (o->mailbox[MB_CMD58_OCR / 4U] & 0x00FF8000U) != 0U;
}

static bool initialize(const char *id)
{
    const char *irq_id = strcmp(id, "SD.INIT") == 0 ? "IRQ.INIT" : "IRQ.REINIT";
    pl_spi_observation_t o;
    pl_spi_result_t status = pl_spi_initialize_card(&device, &o);
    size_t i;
    bool ok = init_valid(status, &o);
    observation(&o, status);
    for (i = 0U; i < SPI_INIT_WORDS; ++i) {
        xil_printf("  mailbox[%02x]=%08x\r\n", (unsigned)(i * 4U), (unsigned)o.mailbox[i]);
    }
    check(id, ok, "fresh CMD0/CMD8/ACMD41/CMD55/CMD58, OCR power/voltage, init events");
    if (ok) {
        block_addressing = (o.mailbox[MB_CMD58_OCR / 4U] & 0x40000000U) != 0U;
        xil_printf("  SD addressing=%s\r\n", block_addressing ? "block" : "byte");
        check(irq_id, o.irq_count != 0U, "initialization IRQ delivered to the PS GIC handler");
    } else {
        result(irq_id, BLOCKED, "initialization did not complete correctly");
    }
    return ok;
}

static bool lba_argument(uint32_t lba, uint32_t *argument)
{
    if (!block_addressing && lba > UINT32_MAX / SPI_BLOCK_BYTES) return false;
    *argument = block_addressing ? lba : lba * SPI_BLOCK_BYTES;
    return true;
}

static bool command_ok(uint8_t command, uint32_t argument, const uint8_t *data,
                        pl_spi_observation_t *o)
{
    pl_spi_result_t status = pl_spi_command(&device, command, argument, data, NULL, o);
    xil_printf("  CMD%u argument=%08x\r\n", (unsigned)command, (unsigned)argument);
    observation(o, status);
    return status == PL_SPI_OK && o->response == 0U && o->guards_ok;
}

static bool read_sector(uint32_t lba, uint8_t *data, pl_spi_observation_t *o)
{
    uint32_t argument;
    uint16_t expected_crc, actual_crc;
    pl_spi_result_t status;
    memset(o, 0, sizeof(*o));
    if (!lba_argument(lba, &argument)) return false;
    status = pl_spi_command(&device, 17U, argument, NULL, data, o);
    if (status != PL_SPI_OK || o->response != 0U ||
        o->rx_length != SPI_BLOCK_RX || !o->guards_ok) {
        xil_printf("  CMD17 LBA=%u\r\n", (unsigned)lba);
        observation(o, status);
        return false;
    }
    expected_crc = pl_spi_crc16(data, SPI_BLOCK_BYTES);
    actual_crc = ((uint16_t)data[SPI_BLOCK_BYTES] << 8) | data[SPI_BLOCK_BYTES + 1U];
    if (expected_crc != actual_crc) {
        value("SD payload CRC16", expected_crc, actual_crc);
        return false;
    }
    return true;
}

static bool compare_bytes(const uint8_t *expected, const uint8_t *actual, size_t size)
{
    size_t i;
    for (i = 0U; i < size; ++i) {
        if (expected[i] != actual[i]) {
            xil_printf("  first mismatch byte=%u expected=%02x actual=%02x\r\n",
                        (unsigned)i, (unsigned)expected[i], (unsigned)actual[i]);
            return false;
        }
    }
    return true;
}

static bool read_checks(bool ready)
{
    static const char *const ids[] = {
        "SD.CMD13", "SD.CMD16", "SD.CMD17", "IRQ.COMMAND", "IRQ.ACK",
        "SD.GOLDEN", "SD.REPEAT", "SD.ALTERNATE", "SD.REINIT", "IRQ.REINIT", "SD.RECOVER"
    };
    pl_spi_observation_t o;
    bool ok, set_length;
    uint32_t crc;
    unsigned i;
    if (!ready) {
        for (i = 0U; i < sizeof(ids) / sizeof(ids[0]); ++i) {
            result(ids[i], BLOCKED, "SD initialization prerequisite failed");
        }
        return false;
    }
    check("SD.CMD13", command_ok(13U, 0U, NULL, &o), "fresh two-byte card status is zero");
    set_length = command_ok(16U, SPI_BLOCK_BYTES, NULL, &o);
    check("SD.CMD16", set_length, "512-byte block length accepted by the card");
    ok = (set_length || block_addressing) &&
         read_sector(PL_SPI_TEST_READ_LBA, baseline, &o);
    if (!check("SD.CMD17", ok, "fresh R1=0, 514 bytes, CRC16 and buffer bounds")) {
        for (i = 3U; i < sizeof(ids) / sizeof(ids[0]); ++i) {
            result(ids[i], BLOCKED, "no validated baseline sector");
        }
        return false;
    }
    observation(&o, PL_SPI_OK);
    crc = pl_spi_crc32(baseline, SPI_BLOCK_BYTES);
    xil_printf("  LBA=%u payload CRC32=%08x first=%02x%02x%02x%02x\r\n",
                (unsigned)PL_SPI_TEST_READ_LBA, (unsigned)crc,
                (unsigned)baseline[0], (unsigned)baseline[1],
                (unsigned)baseline[2], (unsigned)baseline[3]);
    check("IRQ.COMMAND", o.irq_count != 0U && (o.irq_seen & SPI_SD_IRQ_COMMAND) != 0U,
          "fresh command completion reached the PS interrupt handler");
    pl_spi_irq_prepare(&device);
    pl_spi_delay_ms(5U);
    pl_spi_observe(&device, &o);
    observation(&o, PL_SPI_OK);
    check("IRQ.ACK", o.irq_count == 0U && o.irq_seen == 0U,
          "no pending cause or reassertion after explicit acknowledgement");

    if (PL_SPI_TEST_HAVE_GOLDEN) {
        value("independent sector CRC32", PL_SPI_TEST_GOLDEN_CRC32, crc);
        check("SD.GOLDEN", crc == PL_SPI_TEST_GOLDEN_CRC32,
              "sector matches the externally prepared reference");
    } else {
        result("SD.GOLDEN", SKIP, "no independent reference configured; repetition is not an oracle");
    }
    ok = true;
    for (i = 0U; i < PL_SPI_TEST_REPEATS; ++i) {
        if (!read_sector(PL_SPI_TEST_READ_LBA, received, &o) ||
            !compare_bytes(baseline, received, SPI_BLOCK_RX)) {
            ok = false;
            break;
        }
    }
    xil_printf("  repeated reads completed=%u requested=%u\r\n",
                i, (unsigned)PL_SPI_TEST_REPEATS);
    check("SD.REPEAT", ok, "independent DMA transfers without resetting the SPI core");
    if (PL_SPI_TEST_READ_LBA == UINT32_MAX) {
        result("SD.ALTERNATE", SKIP, "following LBA cannot be represented");
    } else {
        ok = read_sector(PL_SPI_TEST_READ_LBA + 1U, received, &o) &&
             read_sector(PL_SPI_TEST_READ_LBA, received, &o) &&
             compare_bytes(baseline, received, SPI_BLOCK_RX);
        check("SD.ALTERNATE", ok, "A/B/A reads preserve A; not proof that A and B differ");
    }
    ok = initialize("SD.REINIT") &&
         read_sector(PL_SPI_TEST_READ_LBA, received, &o) &&
         compare_bytes(baseline, received, SPI_BLOCK_RX);
    return check("SD.RECOVER", ok, "core+DMA reset, reinitialization, and the same complete sector");
}

static void make_pattern(uint32_t lba)
{
    uint32_t i;
    for (i = 0U; i < SPI_BLOCK_BYTES; ++i) {
        pattern[i] = (uint8_t)((i * 37U) ^ (i >> 3) ^
                               (lba >> ((i & 3U) * 8U)) ^ 0x96U);
    }
}

static bool save_neighbours(void)
{
    pl_spi_observation_t o;
    return read_sector(PL_SPI_TEST_SCRATCH_LBA - 1U, neighbours[0], &o) &&
        read_sector(PL_SPI_TEST_SCRATCH_LBA + PL_SPI_TEST_SCRATCH_SECTORS,
                     neighbours[1], &o);
}

static bool check_neighbours(void)
{
    pl_spi_observation_t o;
    bool before = read_sector(PL_SPI_TEST_SCRATCH_LBA - 1U, received, &o) &&
                  compare_bytes(neighbours[0], received, SPI_BLOCK_RX);
    bool after = read_sector(PL_SPI_TEST_SCRATCH_LBA + PL_SPI_TEST_SCRATCH_SECTORS,
                              received, &o) &&
                 compare_bytes(neighbours[1], received, SPI_BLOCK_RX);
    return before && after;
}

static void media_checks(bool ready)
{
    pl_spi_observation_t o;
    const uint32_t sectors = PL_SPI_TEST_SCRATCH_SECTORS;
    uint32_t i, lba, argument, last_argument;
    bool ok = true;
    if (!PL_SPI_TEST_ENABLE_WRITE) {
        result("SD.CMD24", SKIP, "SD writes disabled in config.h");
        result("SD.CMD32_33_38", SKIP, "SD erase disabled in config.h");
        result("SD.MEDIA_BOUNDS", SKIP, "no media mutation requested");
        return;
    }
    xil_printf("  SCRATCH LBA=%u sectors=%u; contents will NOT be restored\r\n",
                (unsigned)PL_SPI_TEST_SCRATCH_LBA, (unsigned)PL_SPI_TEST_SCRATCH_SECTORS);
    if (!ready || !save_neighbours()) {
        result("SD.CMD24", BLOCKED, "card or neighbour snapshots unavailable");
        result("SD.CMD32_33_38", BLOCKED, "write/readback prerequisite unavailable");
        result("SD.MEDIA_BOUNDS", BLOCKED, "neighbour snapshots unavailable");
        return;
    }
    /* Write all distinct patterns before reading any of them back: this
     * exposes address aliasing that immediate write/read pairs could miss. */
    for (i = 0U; i < sectors; ++i) {
        lba = PL_SPI_TEST_SCRATCH_LBA + i;
        make_pattern(lba);
        if (!lba_argument(lba, &argument) || !command_ok(24U, argument, pattern, &o)) {
            ok = false;
            break;
        }
    }
    for (i = 0U; ok && i < sectors; ++i) {
        lba = PL_SPI_TEST_SCRATCH_LBA + i;
        make_pattern(lba);
        ok = read_sector(lba, received, &o) && compare_bytes(pattern, received, SPI_BLOCK_BYTES);
    }
    check("SD.CMD24", ok, "all distinct patterns read back exactly with valid SD CRC16");

    if (!PL_SPI_TEST_ENABLE_ERASE) {
        result("SD.CMD32_33_38", SKIP, "SD erase disabled in config.h");
    } else if (!ok) {
        result("SD.CMD32_33_38", BLOCKED, "no verified pre-erase contents");
    } else {
        ok = lba_argument(PL_SPI_TEST_SCRATCH_LBA, &argument) &&
             lba_argument(PL_SPI_TEST_SCRATCH_LBA + PL_SPI_TEST_SCRATCH_SECTORS - 1U,
                            &last_argument) &&
             command_ok(32U, argument, NULL, &o) &&
             command_ok(33U, last_argument, NULL, &o) &&
             command_ok(38U, 0U, NULL, &o) && command_ok(13U, 0U, NULL, &o);
        memset(pattern, (uint8_t)PL_SPI_TEST_ERASED_BYTE, sizeof(pattern));
        for (i = 0U; ok && i < sectors; ++i) {
            ok = read_sector(PL_SPI_TEST_SCRATCH_LBA + i, received, &o) &&
                 compare_bytes(pattern, received, SPI_BLOCK_BYTES);
        }
        check("SD.CMD32_33_38", ok, "three fresh command completions, status and erased data");
    }
    check("SD.MEDIA_BOUNDS", check_neighbours(), "sectors immediately outside scratch range unchanged");
}

static void coverage_limits(void)
{
    result("PHY.TIMING", SKIP, "SCK/CS timing and physical edge counts not observable by PS alone");
    result("PHY.MODES", SKIP, "other CPOL/CPHA, widths and CS polarity need a different bitstream");
    result("FIFO.BYTE_ORDER", BLOCKED, "no independent programmable SPI peer or raw-data oracle");
    result("FIFO.OVERFLOW", BLOCKED, "RX overflow/data loss beyond full need a controlled stream");
    result("BRAM.LIMIT_IRQ", BLOCKED, "saved page does not define the warning boundary/register contract");
    result("SD.FAULT_INJECTION", BLOCKED, "no programmable card timeout/CRC-error source");
    result("REG.SD_INIT_LAYOUT", BLOCKED, "saved page and newer diagrams use different clock-count fields");
}

static void summary(void)
{
    const char *verdict = totals[FAIL] != 0U ? "FAIL" :
        ((totals[BLOCKED] | totals[SKIP]) != 0U ? "INCOMPLETE" : "PASS");
    xil_printf("SUMMARY PASS=%u FAIL=%u SKIP=%u BLOCKED=%u VERDICT=%s\r\n",
                totals[PASS], totals[FAIL], totals[SKIP], totals[BLOCKED], verdict);
    xil_printf("END PL_SPI_TEST\r\n");
}

static void test_task(void *argument)
{
    bool ready;
    pl_spi_observation_t o = { 0 };
    pl_spi_result_t status;
    (void)argument;
    xil_printf("\r\nBEGIN PL_SPI_TEST format=1 freertos\r\n");
    xil_printf("Software black box: AXI-Lite, BRAM, DMA, PS GIC.\r\n");
    xil_printf("Contract: saved SPI page + page_68812882 SD examples; discrepancies retained.\r\n");
    if (!software_checks()) {
        result("HARDWARE", BLOCKED, "software encoding self-check failed");
        summary();
        vTaskDelete(NULL);
        return;
    }
    if (!pl_spi_open(&device)) {
        result("ACCESS.OPEN", FAIL, "BSP mapping or initialized GIC unavailable");
        summary();
        vTaskDelete(NULL);
        return;
    }
    xil_printf("BSP core=%08x DMA=%08x BRAM=%08x size=%u IRQ=%u\r\n",
                (unsigned)device.core, (unsigned)device.dma, (unsigned)device.bram,
                (unsigned)device.bram_size, (unsigned)device.irq_id);
    xil_printf("Policy write=%u erase=%u read_LBA=%u repeats=%u\r\n",
                (unsigned)PL_SPI_TEST_ENABLE_WRITE, (unsigned)PL_SPI_TEST_ENABLE_ERASE,
                (unsigned)PL_SPI_TEST_READ_LBA, (unsigned)PL_SPI_TEST_REPEATS);
    if (!access_checks()) {
        result("SUITE", BLOCKED, "access prerequisite failed; no SD commands submitted");
        pl_spi_close(&device);
        summary();
        vTaskDelete(NULL);
        return;
    }
    register_checks();
    raw_checks();
    tx_fifo_boundary();
    ready = initialize("SD.INIT");
    ready = read_checks(ready);
    media_checks(ready);
    coverage_limits();
    status = pl_spi_dma_reset(&device);
    pl_spi_core_reset(&device);
    pl_spi_observe(&device, &o);
    check("CLEANUP", status == PL_SPI_OK &&
          ((o.tx_control | o.rx_control) & (DMA_RUN | DMA_RESET)) == 0U,
          "DMA stopped and core reset; no automatic retry of failed media writes");
    pl_spi_close(&device);
    summary();
    vTaskDelete(NULL);
}

int pl_spi_test_start(void)
{
    return xTaskCreate(test_task, "pl_spi_test", 4096U, NULL,
                       tskIDLE_PRIORITY + 2U, NULL) == pdPASS ? 0 : -1;
}
