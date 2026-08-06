# Bundled SSH dependencies

This directory contains the source archives required to build the optional
FreeRTOS SSH/SCP/SFTP service without downloading dependencies or relying on a
machine-specific `/tmp` directory.

Pinned sources:

- `wolfssl-5.9.2.tar.gz` — wolfSSL commit `5b22fa901e81d925a70ab1584ae792c8e92e34a5`;
- `wolfssh-1.5.0.tar.gz` — wolfSSH commit `6f0cbe3f137fb3c074730acc1dd2cdbfd685d8f5`.

The archives include the Autotools-generated configure inputs needed for an
offline build. The project-specific wolfSSH changes are kept as reviewable
patches in `scripts/vitis/` and are applied to a fresh build copy.

The original wolfSSL and wolfSSH license and licensing files are included in
their respective archives. Both projects are distributed by wolfSSL under
their applicable GPL/commercial licensing terms; review those files before
redistributing a product.

Do not place compiler output here. `scripts/vitis/build_ssh_deps.sh` extracts
the archives and creates target-specific libraries under `build/ssh-deps/`.
