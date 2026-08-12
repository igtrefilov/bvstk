# Аппаратная карта AX7020

## 1. Источник контракта

Программная карта платы находится в:

```text
src/hardware/boards/ax7020/bvstk_hw_config.h
src/hardware/boards/ax7020/bvstk_pl_regions.h
src/hardware/boards/ax7020/bvstk_pl_regions.c
```

FreeRTOS сверяет значения с Vitis `xparameters.h` через `bvstk_hw_validate.c`. Neutrino компилирует общие константы напрямую.

## 2. MMIO-регионы

| Регион | Base address | Size | Тип |
|---|---:|---:|---|
| `i2c-master` | `0x43C00000` | `0x10000` | control |
| `smi-master` | `0x43C10000` | `0x10000` | control |
| `smi-slave` | `0x43C20000` | `0x10000` | control |
| `spi-master` | `0x43C30000` | `0x10000` | control |
| `i2c-slave` | `0x43C40000` | `0x10000` | control |
| `i2c-bram` | `0x40000000` | `0x2000` | BRAM |
| `smi-bram` | `0x42000000` | `0x2000` | BRAM |
| `spi-bram` | `0x44000000` | `0x2000` | BRAM |

Регионы доступны по именам в `bvstkctl pl list`, PL service и DCP2 MEM policy.

## 3. IRQ

| Источник | Macro | GIC ID |
|---|---|---:|
| SMI master | `BVSTK_IRQ_SMI_MASTER` | `61` |
| SMI slave | `BVSTK_IRQ_SMI_SLAVE` | `62` |
| I2C master | `BVSTK_IRQ_I2C_MASTER` | `63` |
| I2C slave | `BVSTK_IRQ_I2C_SLAVE` | `64` |
| SPI master | `BVSTK_IRQ_SPI_MASTER` | `65` |

I2C slave IRQ получает FreeRTOS adapter `src/ports/freertos-xilinx/os/i2c/`. Остальные cores выполняют операции через общий MMIO/sync слой; конкретный IRQ flow определяется целевой интеграцией.

## 4. BRAM layout

### I2C

| Макрос | Offset | Назначение |
|---|---:|---|
| `BVSTK_I2C_BRAM_SLAVE_WR_OFFSET` | `0x0000` | slave write mailbox |
| `BVSTK_I2C_BRAM_MASTER_OFFSET` | `0x0500` | master transaction buffer |
| `BVSTK_I2C_BRAM_SLAVE_RD_OFFSET` | `0x1000` | slave read mailbox |

### SMI

| Макрос | Offset | Назначение |
|---|---:|---|
| `BVSTK_SMI_MASTER_WR_OFFSET` | `0x0000` | master command |
| `BVSTK_SMI_SLAVE_WR_OFFSET` | `0x1000` | slave write mailbox |
| `BVSTK_SMI_SLAVE_RD_OFFSET` | `0x2000` | slave read mailbox |

### SPI

SPI BRAM имеет размер `0x2000`; offsets control-регистров и packet fields находятся в `src/hardware/pl/spi/bvstk_spi_regs.h`.

## 5. Проверка платформы

```mermaid
flowchart TD
    XSA[design.xsa] --> BSP[xparameters.h]
    Header[bvstk_hw_config.h] --> Check[bvstk_hw_validate.c]
    BSP --> Check
    Check --> Build[FreeRTOS build]
    Bit[design.bit] --> Run[JTAG runtime]
    Build --> Run
```

```sh
./build.sh check
./build.sh freertos
./build.sh neutrino
```

При изменении адресов, размеров, IRQ или BRAM offsets требуется обновление XSA, bitstream, board contract, OS adapters и соответствующих тестов.
