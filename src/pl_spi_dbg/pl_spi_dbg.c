#include "pl_spi_dbg.h"

static pl_spi_dbg_ctx_t g_ctx = {
    .base_addr = PL_SPI_DBG_DEFAULT_BASEADDR,
    .bram_base_addr = PL_SPI_DBG_DEFAULT_BRAM_BASEADDR,
    .mode = PL_SPI_DBG_MODE_MULTI,
    .read_en = true,
};

static volatile bool g_spi_irq_seen = false;
static bool g_spi_irq_inited = false;

/**
 * @brief Записывает 32-битное значение в регистр SPI PL-ядра.
 *
 * @param reg_offset Смещение регистра относительно базового адреса SPI.
 * @param value      Значение для записи.
 */
static inline void reg_write(uint32_t reg_offset, uint32_t value)
{
    Xil_Out32(g_ctx.base_addr + reg_offset, value);
}

/**
 * @brief Сбрасывает сигнал прерывания в SPI PL-ядре.
 *
 * Выполняет последовательность записи в регистр IRQ с битом
 * `irq_dispel`, затем возвращает бит в ноль.
 */
static void pl_spi_dbg_irq_clear(void)
{
    uint32_t dispel = (1U << PL_SPI_DBG_IRQ_WR_DISPEL_BIT);
    reg_write(PL_SPI_DBG_REG_IRQ, dispel);
    reg_write(PL_SPI_DBG_REG_IRQ, 0U);
}

/**
 * @brief Применяет параметры режима отладочной передачи SPI.
 *
 * Нормализует входные значения (делитель - четный и >=2, таймаут >=1)
 * и записывает параметры в регистры ядра.
 *
 * @param clk_div_even_ge_2 Тактовый делитель SPI (четный, не менее 2).
 * @param timeout_ticks_ge_1 Таймаут между пакетами в тиках (не менее 1).
 * @param read_en Разрешение чтения (full-duplex), true/false.
 */
static void pl_spi_dbg_configure(uint16_t clk_div_even_ge_2,
                                 uint32_t timeout_ticks_ge_1,
                                 bool read_en)
{
    uint16_t clk_div = clk_div_even_ge_2;
    uint32_t timeout = timeout_ticks_ge_1;

    if (clk_div < 2U) clk_div = 2U;
    if ((clk_div & 0x1U) != 0U) clk_div++;
    if (timeout == 0U) timeout = 1U;

    g_ctx.mode = PL_SPI_DBG_MODE_MULTI;
    g_ctx.read_en = read_en;

    reg_write(PL_SPI_DBG_REG_CLK_DIV, (uint32_t)clk_div);
    reg_write(PL_SPI_DBG_REG_TIMEOUT, timeout);
}

/**
 * @brief Загружает слова в TX FIFO и запускает SPI-передачу.
 *
 * @param tx_words      Указатель на буфер слов для передачи.
 * @param tx_word_count Количество 32-битных слов в буфере.
 *
 * @return true  Передача успешно запущена.
 * @return false Входные параметры невалидны.
 */
static bool pl_spi_dbg_transfer_words(const uint32_t *tx_words,
                                      size_t tx_word_count)
{
    size_t i;
    uint32_t packets_num;
    uint32_t packet_reg;
    uint32_t csr_start;

    if (tx_words == NULL || tx_word_count == 0U) return false;

    packets_num = (uint32_t)(tx_word_count * 4U);
    packet_reg = (packets_num << 2) | ((uint32_t)g_ctx.mode & 0x3U);
    csr_start = (1U << PL_SPI_DBG_CSR_WR_START_BIT);
    if (g_ctx.read_en) csr_start |= (1U << PL_SPI_DBG_CSR_WR_READ_EN_BIT);

    reg_write(PL_SPI_DBG_REG_PACKET, packet_reg);

    for (i = 0U; i < tx_word_count; ++i) {
        reg_write(PL_SPI_DBG_REG_TX_FIFO, tx_words[i]);
    }

    reg_write(PL_SPI_DBG_REG_CSR, csr_start);
    return true;
}

/**
 * @brief Обработчик IRQ от SPI PL-ядра.
 *
 * @param callback_ref Пользовательский контекст (не используется).
 */
static void pl_spi_dbg_isr(void *callback_ref)
{
    (void)callback_ref;
    g_spi_irq_seen = true;
    pl_spi_dbg_irq_clear();
}

/**
 * @brief Регистрирует и включает обработчик IRQ для SPI PL-ядра.
 *
 * @return true Инициализация выполнена или уже была выполнена ранее.
 */
static bool pl_spi_dbg_irq_init(void)
{
    if (g_spi_irq_inited) return true;
    xPortInstallInterruptHandler(PL_SPI_DBG_IRQ_ID, pl_spi_dbg_isr, NULL);
    vPortEnableInterrupt(PL_SPI_DBG_IRQ_ID);
    g_spi_irq_inited = true;
    return true;
}

/**
 * @brief Инициализирует отладочный SPI-модуль.
 *
 * Выполняет базовую настройку SPI PL-ядра:
 * - назначает базовые адреса регистров и BRAM,
 * - выставляет параметры тайминга/режима передачи,
 * - регистрирует и включает обработчик прерывания.
 */
void start_pl_spi_dbg(void)
{
    g_ctx.base_addr = PL_SPI_DBG_DEFAULT_BASEADDR;
    g_ctx.bram_base_addr = PL_SPI_DBG_DEFAULT_BRAM_BASEADDR;
    pl_spi_dbg_configure(512U, 1U, true);
    (void)pl_spi_dbg_irq_init();

    xil_printf("PL SPI DBG: ready (base=0x%08lX, bram=0x%08lX)\r\n",
               (unsigned long)g_ctx.base_addr,
               (unsigned long)g_ctx.bram_base_addr);
}

/**
 * @brief Запускает тестовую SPI-передачу и ожидает IRQ-подтверждение.
 *
 * Передает одно 32-битное тестовое слово в SPI PL-ядро и ожидает
 * срабатывание прерывания в течение ограниченного числа тиков.
 *
 * @return true  Если IRQ получено в пределах таймаута.
 * @return false Если IRQ не инициализировано или не получено по таймауту.
 */
bool pl_spi_dbg_transfer(void)
{
    uint32_t tx = 0xFF000000U;
    uint32_t i;

    if (!g_spi_irq_inited) {
        xil_printf("PL SPI DBG transfer: irq is not initialized\r\n");
        return false;
    }

    g_spi_irq_seen = false;
    g_ctx.read_en = false;
    (void)pl_spi_dbg_transfer_words(&tx, 1U);

    for (i = 0U; i < 200U; ++i) {
        if (g_spi_irq_seen) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    xil_printf("PL SPI DBG irq example: %s\r\n", g_spi_irq_seen ? "irq received" : "irq timeout");
    return g_spi_irq_seen;
}
