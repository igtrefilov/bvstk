# AX7020 Neutrino BSP snapshot

This directory contains the AX7020-specific runtime files required to create
the Burevestnik Neutrino IFS:

- `images/zynq7000-ax7020-ssh.build` — the base image description;
- `install/` — board-specific startup, drivers and runtime libraries.

The snapshot intentionally does not contain generated SSH host keys or
`authorized_keys`. `scripts/neutrino/build_image.sh` creates fresh local keys
under `build/neutrino/` and injects them into the generated image.
Password authentication is disabled in the image; access is provided through
the generated public key.

The host-side Neutrino SDK remains required for `qcc`, `mkifs` and the generic
target files referenced by the BSP image description. Those tools are not
AX7020 project sources and are expected to be installed on the build machine.
