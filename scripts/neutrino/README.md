# Neutrino build and JTAG workflow

The repository stores the portable application code, the AX7020 PL contract
and image-generation scripts.  The proprietary Neutrino SDK and BSP remain
external dependencies.

Default local dependencies:

- SDK environment: `/etc/profile.d/kpda_env_2024.sh`;
- BSP: `/home/ilya/neutrino/kpda-bsp-xilinx-zynq7000-2024-bin-20260430-43ee921596/kpda-bsp-xilinx-zynq7000`;
- base image description: `images/zynq7000-ax7020-ssh.build`.

Build the application:

```sh
./build.sh neutrino
```

Build an IFS containing `/usr/bin/bvstkctl`:

```sh
./build.sh neutrino-image
```

Program the PL, load the IFS over JTAG and verify it over SSH:

```sh
./run.sh neutrino jtag
```

The external paths can be changed with `NEUTRINO_BSP_DIR`,
`NEUTRINO_BASE_BUILD`, `QCC_VARIANT`, `UART_DEVICE`, `DEVICE_IP` and
`SSH_IDENTITY`.
