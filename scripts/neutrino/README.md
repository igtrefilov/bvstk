# Neutrino build and JTAG workflow

The repository stores the portable application code, the AX7020 PL contract,
the required AX7020 BSP runtime snapshot and image-generation scripts.  The
Neutrino SDK itself remains an external host dependency because `qcc` and
`mkifs` are installed tools, not project sources.

Default local dependencies:

- SDK environment: `/etc/profile.d/kpda_env_2024.sh`;
- BSP: `third_party/neutrino/bsp/ax7020`;
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

The BSP and base image paths can be changed with `NEUTRINO_BSP_DIR` and
`NEUTRINO_BASE_BUILD`. Other machine-specific settings are controlled with
`QCC_VARIANT`, `UART_DEVICE`, `DEVICE_IP` and `SSH_IDENTITY`. If no SSH key is
provided, the image build generates a local key pair under `build/neutrino/`;
these generated private keys must not be committed.
