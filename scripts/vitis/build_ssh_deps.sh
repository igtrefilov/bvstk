#!/usr/bin/env bash
set -euo pipefail

# Build the pinned wolfSSL/wolfSSH sources bundled with bvstk.  The result is
# deliberately kept under build/ because it is target- and toolchain-specific.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

SSH_DEPS_OUTPUT="${BVSTK_SSH_DEPS_OUTPUT:-$REPO_ROOT/build/ssh-deps}"
SSH_DEPS_BSP_INCLUDE="${BVSTK_SSH_BSP_INCLUDE:-}"
SSH_DEPS_FATFS_INCLUDE="${BVSTK_SSH_FATFS_INCLUDE:-}"
SSH_DEPS_XILINX_INCLUDE="${BVSTK_SSH_XILINX_INCLUDE:-}"
SSH_DEPS_FREERTOS_INCLUDE="${BVSTK_SSH_FREERTOS_INCLUDE:-}"
SSH_DEPS_LWIP_INCLUDE="${BVSTK_SSH_LWIP_INCLUDE:-}"
SSH_DEPS_LWIP_CONTRIB_INCLUDE="${BVSTK_SSH_LWIP_CONTRIB_INCLUDE:-}"
SSH_DEPS_JOBS="${BVSTK_SSH_DEPS_JOBS:-$(nproc 2>/dev/null || echo 1)}"
SSH_DEPS_CC="${CC:-}"
SSH_DEPS_AR="${AR:-}"
SSH_DEPS_RANLIB="${RANLIB:-}"

WOLFSSL_ARCHIVE="$REPO_ROOT/third_party/dist/wolfssl-5.9.2.tar.gz"
WOLFSSH_ARCHIVE="$REPO_ROOT/third_party/dist/wolfssh-1.5.0.tar.gz"
SCP_PATCH="$REPO_ROOT/scripts/vitis/wolfssh-scp-single-file.patch"
SFTP_PATCH="$REPO_ROOT/scripts/vitis/wolfssh-sftp-fatfs.patch"

usage() {
    cat >&2 <<EOF
Usage: $0 --bsp-include PATH [--fatfs-include PATH] [--xilinx-include PATH] [--freertos-include PATH] [--lwip-include PATH] [--lwip-contrib-include PATH] [--output PATH] [--jobs N]

Builds the bundled wolfSSL/wolfSSH libraries for the current Xilinx ARM
toolchain. The BSP include directory must contain the generated FatFs headers.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bsp-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_BSP_INCLUDE="$2"
            shift 2
            ;;
        --fatfs-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_FATFS_INCLUDE="$2"
            shift 2
            ;;
        --xilinx-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_XILINX_INCLUDE="$2"
            shift 2
            ;;
        --freertos-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_FREERTOS_INCLUDE="$2"
            shift 2
            ;;
        --lwip-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_LWIP_INCLUDE="$2"
            shift 2
            ;;
        --lwip-contrib-include)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_LWIP_CONTRIB_INCLUDE="$2"
            shift 2
            ;;
        --output)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_OUTPUT="$2"
            shift 2
            ;;
        --jobs)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            SSH_DEPS_JOBS="$2"
            shift 2
            ;;
        -h|--help)
            usage >&1
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

if [[ -z "$SSH_DEPS_BSP_INCLUDE" || ! -d "$SSH_DEPS_BSP_INCLUDE" ]]; then
    echo "ERROR: generated BSP include directory is required: $SSH_DEPS_BSP_INCLUDE" >&2
    exit 1
fi
if [[ -z "$SSH_DEPS_FATFS_INCLUDE" ]]; then
    SSH_DEPS_FATFS_INCLUDE="$SSH_DEPS_BSP_INCLUDE"
fi
if [[ ! -f "$SSH_DEPS_FATFS_INCLUDE/ff.h" ]]; then
    echo "ERROR: FatFs include directory must contain ff.h: $SSH_DEPS_FATFS_INCLUDE" >&2
    exit 1
fi
if [[ -z "$SSH_DEPS_XILINX_INCLUDE" ]]; then
    SSH_DEPS_XILINX_INCLUDE="$SSH_DEPS_BSP_INCLUDE"
fi
if [[ ! -f "$SSH_DEPS_XILINX_INCLUDE/xil_types.h" ]]; then
    echo "ERROR: Xilinx include directory must contain xil_types.h: $SSH_DEPS_XILINX_INCLUDE" >&2
    exit 1
fi
if [[ -z "$SSH_DEPS_FREERTOS_INCLUDE" ]]; then
    SSH_DEPS_FREERTOS_INCLUDE="$SSH_DEPS_BSP_INCLUDE"
fi
if [[ ! -f "$SSH_DEPS_FREERTOS_INCLUDE/FreeRTOS.h" ]]; then
    echo "ERROR: FreeRTOS include directory must contain FreeRTOS.h: $SSH_DEPS_FREERTOS_INCLUDE" >&2
    exit 1
fi
if [[ -z "$SSH_DEPS_LWIP_INCLUDE" ]]; then
    SSH_DEPS_LWIP_INCLUDE="$SSH_DEPS_BSP_INCLUDE"
fi
if [[ ! -f "$SSH_DEPS_LWIP_INCLUDE/lwip/sockets.h" ]]; then
    echo "ERROR: lwIP include directory must contain lwip/sockets.h: $SSH_DEPS_LWIP_INCLUDE" >&2
    exit 1
fi
if [[ -z "$SSH_DEPS_LWIP_CONTRIB_INCLUDE" ]]; then
    SSH_DEPS_LWIP_CONTRIB_INCLUDE="$SSH_DEPS_LWIP_INCLUDE"
fi
if [[ ! -f "$SSH_DEPS_LWIP_CONTRIB_INCLUDE/lwipopts.h" ]]; then
    echo "ERROR: Xilinx lwIP include directory must contain lwipopts.h: $SSH_DEPS_LWIP_CONTRIB_INCLUDE" >&2
    exit 1
fi

for required_file in \
    "$WOLFSSL_ARCHIVE" \
    "$WOLFSSH_ARCHIVE" \
    "$SCP_PATCH" \
    "$SFTP_PATCH"; do
    if [[ ! -f "$required_file" ]]; then
        echo "ERROR: bundled SSH dependency file is missing: $required_file" >&2
        exit 1
    fi
done

if [[ -z "$SSH_DEPS_CC" ]]; then
    SSH_DEPS_CC="$(command -v arm-none-eabi-gcc 2>/dev/null || true)"
fi
if [[ -z "$SSH_DEPS_CC" ]]; then
    SSH_DEPS_CC="$(command -v arm-xilinx-eabi-gcc 2>/dev/null || true)"
fi
if [[ -z "$SSH_DEPS_CC" ]]; then
    echo "ERROR: ARM GCC was not found. Source the Xilinx/Vitis settings script first." >&2
    exit 1
fi

SSH_DEPS_TOOL_DIR="$(dirname "$SSH_DEPS_CC")"
if [[ -z "$SSH_DEPS_AR" ]]; then
    for candidate in \
        "$SSH_DEPS_TOOL_DIR/arm-none-eabi-ar" \
        "$SSH_DEPS_TOOL_DIR/arm-xilinx-eabi-ar"; do
        if [[ -x "$candidate" ]]; then
            SSH_DEPS_AR="$candidate"
            break
        fi
    done
fi
if [[ -z "$SSH_DEPS_RANLIB" ]]; then
    for candidate in \
        "$SSH_DEPS_TOOL_DIR/arm-none-eabi-ranlib" \
        "$SSH_DEPS_TOOL_DIR/arm-xilinx-eabi-ranlib"; do
        if [[ -x "$candidate" ]]; then
            SSH_DEPS_RANLIB="$candidate"
            break
        fi
    done
fi
if [[ -z "$SSH_DEPS_AR" || -z "$SSH_DEPS_RANLIB" ]]; then
    echo "ERROR: ARM ar/ranlib were not found next to $SSH_DEPS_CC" >&2
    exit 1
fi

SSH_DEPS_BSP_INCLUDE="$(cd "$SSH_DEPS_BSP_INCLUDE" && pwd)"
SSH_DEPS_FATFS_INCLUDE="$(cd "$SSH_DEPS_FATFS_INCLUDE" && pwd)"
SSH_DEPS_XILINX_INCLUDE="$(cd "$SSH_DEPS_XILINX_INCLUDE" && pwd)"
SSH_DEPS_FREERTOS_INCLUDE="$(cd "$SSH_DEPS_FREERTOS_INCLUDE" && pwd)"
SSH_DEPS_LWIP_INCLUDE="$(cd "$SSH_DEPS_LWIP_INCLUDE" && pwd)"
SSH_DEPS_LWIP_CONTRIB_INCLUDE="$(cd "$SSH_DEPS_LWIP_CONTRIB_INCLUDE" && pwd)"
SSH_DEPS_OUTPUT="$(mkdir -p "$SSH_DEPS_OUTPUT" && cd "$SSH_DEPS_OUTPUT" && pwd)"
SSH_DEPS_WORK="$SSH_DEPS_OUTPUT/source"
SSH_DEPS_INSTALL="$SSH_DEPS_OUTPUT/install"
SSH_DEPS_STAMP="$SSH_DEPS_OUTPUT/.stamp"
WOLFSSL_INSTALL="$SSH_DEPS_INSTALL/wolfssl"
WOLFSSH_INSTALL="$SSH_DEPS_INSTALL/wolfssh"

SSH_DEPS_CFLAGS="-mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -O2"
WOLFSSL_CFLAGS="$SSH_DEPS_CFLAGS -DWOLFSSL_LWIP -DWC_RNG_SEED_CB -I$SSH_DEPS_BSP_INCLUDE"
WOLFSSH_CFLAGS="$SSH_DEPS_CFLAGS -DNO_FILESYSTEM -DWOLFSSH_FATFS -DWOLFSSH_BVSTK_FATFS -DWOLFSSH_LWIP -DWOLFSSL_LWIP -DWOLFSSH_TERM -DNO_TERMIOS -DWOLFSSH_SCP_USER_CALLBACKS -DWOLFSSH_MAX_SFTP_RW=4096 -DWOLFSSH_MAX_SFTP_RECV=8192 -I$SSH_DEPS_BSP_INCLUDE -I$SSH_DEPS_FATFS_INCLUDE -I$SSH_DEPS_XILINX_INCLUDE -I$SSH_DEPS_FREERTOS_INCLUDE -I$SSH_DEPS_LWIP_INCLUDE -I$SSH_DEPS_LWIP_CONTRIB_INCLUDE -I$WOLFSSL_INSTALL/include"

bsp_fingerprint="$({
    find "$SSH_DEPS_BSP_INCLUDE" "$SSH_DEPS_FATFS_INCLUDE" "$SSH_DEPS_XILINX_INCLUDE" "$SSH_DEPS_FREERTOS_INCLUDE" "$SSH_DEPS_LWIP_INCLUDE" "$SSH_DEPS_LWIP_CONTRIB_INCLUDE" -type f -print0 |
        sort -z | xargs -0 sha256sum
} | sha256sum | cut -d' ' -f1)"
source_stamp="$({
    sha256sum "$WOLFSSL_ARCHIVE" "$WOLFSSH_ARCHIVE" "$SCP_PATCH" "$SFTP_PATCH" "$0"
    printf '%s\n' "$SSH_DEPS_BSP_INCLUDE" "$SSH_DEPS_FATFS_INCLUDE" "$SSH_DEPS_XILINX_INCLUDE" "$SSH_DEPS_FREERTOS_INCLUDE" "$SSH_DEPS_LWIP_INCLUDE" "$SSH_DEPS_LWIP_CONTRIB_INCLUDE" "$SSH_DEPS_CC" "$SSH_DEPS_AR" "$SSH_DEPS_RANLIB" "$WOLFSSL_CFLAGS" "$WOLFSSH_CFLAGS" "$bsp_fingerprint"
} | sha256sum | cut -d' ' -f1)"

if [[ -f "$SSH_DEPS_STAMP" && "$(<"$SSH_DEPS_STAMP")" == "$source_stamp" && \
      -f "$WOLFSSL_INSTALL/lib/libwolfssl.a" && \
      -f "$WOLFSSL_INSTALL/include/wolfssl/options.h" && \
      -f "$WOLFSSH_INSTALL/lib/libwolfssh.a" && \
      -f "$WOLFSSH_INSTALL/wolfssh/wolfsftp.h" ]]; then
    echo "Bundled SSH dependencies are ready in $SSH_DEPS_INSTALL"
    exit 0
fi

echo "Building bundled wolfSSL/wolfSSH in $SSH_DEPS_OUTPUT"
rm -rf "$SSH_DEPS_WORK" "$SSH_DEPS_INSTALL"
mkdir -p "$SSH_DEPS_WORK/wolfssl" "$SSH_DEPS_WORK/wolfssh" "$SSH_DEPS_INSTALL"
tar -xzf "$WOLFSSL_ARCHIVE" -C "$SSH_DEPS_WORK/wolfssl"
tar -xzf "$WOLFSSH_ARCHIVE" -C "$SSH_DEPS_WORK/wolfssh"

patch --batch --forward --strip=1 --directory="$SSH_DEPS_WORK/wolfssh" < "$SCP_PATCH"
patch --batch --forward --strip=1 --directory="$SSH_DEPS_WORK/wolfssh" < "$SFTP_PATCH"

pushd "$SSH_DEPS_WORK/wolfssl" >/dev/null
env \
    ac_cv_vcs_system=none \
    ac_cv_vcs_checkout=no \
    ac_cv_warnings_as_errors=no \
    CC="$SSH_DEPS_CC" \
    AR="$SSH_DEPS_AR" \
    RANLIB="$SSH_DEPS_RANLIB" \
    CFLAGS="$WOLFSSL_CFLAGS" \
    CPPFLAGS="-I$SSH_DEPS_BSP_INCLUDE" \
    LDFLAGS="-nostdlib -Wl,-e,main" \
    ./configure \
        --host=arm-none-eabi \
        --prefix="$WOLFSSL_INSTALL" \
        --disable-shared \
        --enable-static \
        --disable-filesystem \
        --enable-singlethreaded \
        --disable-threadlocal \
        --disable-asm \
        --enable-smallstack \
        --enable-wolfssh \
        --enable-cryptonly \
        --disable-tls \
        --disable-dtls \
        --disable-curl \
        --disable-pkcs12 \
        --disable-crypttests \
        --disable-benchmark
make -j"$SSH_DEPS_JOBS"
make install
popd >/dev/null

pushd "$SSH_DEPS_WORK/wolfssh" >/dev/null
env \
    ac_cv_vcs_system=none \
    ac_cv_vcs_checkout=no \
    ac_cv_warnings_as_errors=no \
    CC="$SSH_DEPS_CC" \
    AR="$SSH_DEPS_AR" \
    RANLIB="$SSH_DEPS_RANLIB" \
    CFLAGS="$WOLFSSH_CFLAGS" \
    CPPFLAGS="-I$SSH_DEPS_BSP_INCLUDE -I$SSH_DEPS_FATFS_INCLUDE -I$SSH_DEPS_XILINX_INCLUDE -I$SSH_DEPS_FREERTOS_INCLUDE -I$SSH_DEPS_LWIP_INCLUDE -I$SSH_DEPS_LWIP_CONTRIB_INCLUDE -I$WOLFSSL_INSTALL/include" \
    LDFLAGS="-L$WOLFSSL_INSTALL/lib -nostdlib -Wl,-e,main" \
    LIBS="-lwolfssl -lc -lm -lgcc -lnosys" \
    ./configure \
        --host=arm-none-eabi \
        --prefix="$WOLFSSH_INSTALL" \
        --disable-shared \
        --enable-static \
        --disable-client \
        --disable-examples \
        --enable-sftp \
        --enable-scp \
        --disable-fwd \
        --disable-agent \
        --disable-certs \
        --disable-keygen \
        --enable-smallstack \
        --enable-shell \
        --disable-term \
        --with-wolfssl="$WOLFSSL_INSTALL"
make -j"$SSH_DEPS_JOBS"
make install
popd >/dev/null

# build.tcl accepts both a wolfSSH source tree and an install prefix whose
# headers live directly below wolfssh/. Keep the install prefix convenient for
# the latter without duplicating the public headers.
ln -s include/wolfssh "$WOLFSSH_INSTALL/wolfssh"

for required_output in \
    "$WOLFSSL_INSTALL/lib/libwolfssl.a" \
    "$WOLFSSL_INSTALL/include/wolfssl/options.h" \
    "$WOLFSSH_INSTALL/lib/libwolfssh.a" \
    "$WOLFSSH_INSTALL/wolfssh/ssh.h" \
    "$WOLFSSH_INSTALL/wolfssh/wolfscp.h" \
    "$WOLFSSH_INSTALL/wolfssh/wolfsftp.h"; do
    if [[ ! -f "$required_output" ]]; then
        echo "ERROR: SSH dependency build did not produce $required_output" >&2
        exit 1
    fi
done

printf '%s\n' "$source_stamp" > "$SSH_DEPS_STAMP"
echo "Bundled SSH dependencies installed in $SSH_DEPS_INSTALL"
