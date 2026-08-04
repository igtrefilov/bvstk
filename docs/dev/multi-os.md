# FreeRTOS and Neutrino architecture

The repository produces two OS-specific artifacts from one product contract:

```text
Vivado design/XSA
       |
shared PL register, BRAM and IRQ contract
       |
       +-- FreeRTOS platform and drivers -> app_bvstk.elf
       |
       +-- Neutrino platform and services -> bvstkctl + IFS
```

The common code owns hardware addresses, register layouts, normalized status
codes and service semantics.  OS-specific directories own scheduler, MMIO,
interrupt, filesystem, network and boot integration.

## Current implementation

- `src/pl_common` is the versioned contract exported by the current XSA.
- `src/platform/freertos` validates that contract against `xparameters.h` and
  uses direct Xilinx MMIO.
- `src/platform/neutrino` obtains I/O privileges with
  `ThreadCtl(_NTO_TCTL_IO)` and maps physical regions with
  `mmap_device_memory()`.
- `src/services_common` provides normalized status and checked PL access.
- `/usr/bin/bvstkctl` is the first Neutrino frontend for listing, probing,
  reading and explicitly writing PL regions.

The existing FreeRTOS network, filesystem, HTTP, DCP2 and device workers stay
functional while their logic is incrementally moved above the common service
API.  Neutrino uses the same PL contract and will receive equivalent service
frontends without importing FreeRTOS, lwIP or Xilinx BSP APIs into common code.

## Build commands

```sh
./build.sh freertos
./build.sh neutrino
./build.sh neutrino-image
./build.sh all
```

Generated artifacts are kept outside Git in `build/neutrino` and `vitis_ws`.
The Neutrino SDK and binary BSP are external dependencies and are not copied
into this repository.
