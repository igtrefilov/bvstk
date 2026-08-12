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
these generated private keys must not be committed. By default the root
password is locked. To use password authentication, provide a local Neutrino
`/etc/shadow` line through `NEUTRINO_ROOT_SHADOW_FILE`; the file is injected
into the IFS and must remain outside Git.

Generate a compatible local shadow file before building the image:

```sh
./scripts/neutrino/generate_root_shadow.py
./build.sh neutrino-image
```

The utility asks for the password twice without echoing it, writes
`build/neutrino/root.shadow` with mode `0600`, and generates a random salt.
Use `--output /abs/path/root.shadow` together with
`NEUTRINO_ROOT_SHADOW_FILE=/abs/path/root.shadow` when the file should be kept
outside the build directory. The generated hash is compatible with the
Neutrino `$G$` format and is not stored in Git. The target IFS is read-only at
runtime, so changing `/etc/shadow` with `passwd` on the running target is not
supported. To replace an already generated password, run the utility with
`--force`.
