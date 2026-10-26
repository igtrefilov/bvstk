#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"
GOST_DIR="$BUILD_DIR/gost"
SOURCE_DIR="${NEUTRINO_GOST_SOURCE_DIR:-$GOST_DIR/source}"
STAGE_DIR="$GOST_DIR/stage"
QCC_VARIANT="${QCC_VARIANT:-8.3.0,gcc_ntoarmv7le}"
# openssl_1_1_1 branch, pinned so a later upstream change cannot change an IFS.
GOST_REPOSITORY=https://github.com/gost-engine/engine.git
GOST_REVISION=739f957615eb33a33a6485ae7cf29c7c679fd59a

if ! command -v qcc >/dev/null 2>&1 && [[ -r /etc/profile.d/kpda_env_2024.sh ]]; then
  # shellcheck disable=SC1091
  source /etc/profile.d/kpda_env_2024.sh
fi

for command in qcc cmake git make ntoarmv7-strip sha256sum; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "$command not found; source the Neutrino 2024 development kit" >&2
    exit 1
  fi
done
: "${KPDA_TARGET:?source the Neutrino 2024 development kit}"
: "${KPDA_HOST:?source the Neutrino 2024 development kit}"
OPENSSL_ROOT="$KPDA_TARGET/armle-v7"
OPENSSL_HEADER="$OPENSSL_ROOT/usr/include/openssl/opensslv.h"
OPENSSL_CONFIG="$OPENSSL_ROOT/etc/openssl/openssl.cnf"
if ! grep -Eq 'OPENSSL_VERSION_TEXT[[:space:]]+"OpenSSL 1\.1\.1' "$OPENSSL_HEADER"; then
  echo "GOST ENGINE requires the Neutrino ARMv7 OpenSSL 1.1.1 SDK" >&2
  exit 1
fi
if grep -Eq '^[[:space:]]*openssl_conf[[:space:]]*=' "$OPENSSL_CONFIG"; then
  echo "SDK openssl.cnf already sets openssl_conf; merge its initialization with engine.cnf first" >&2
  exit 1
fi

mkdir -p "$GOST_DIR"
if [[ ! -d "$SOURCE_DIR/.git" ]]; then
  if [[ -e "$SOURCE_DIR" ]]; then
    echo "GOST source path already exists and is not a Git checkout: $SOURCE_DIR" >&2
    exit 1
  fi
  git init -q "$SOURCE_DIR"
  git -C "$SOURCE_DIR" remote add origin "$GOST_REPOSITORY"
fi
if ! git -C "$SOURCE_DIR" rev-parse --verify HEAD >/dev/null 2>&1; then
  git -C "$SOURCE_DIR" fetch --depth=1 origin "$GOST_REVISION"
  git -C "$SOURCE_DIR" checkout --detach "$GOST_REVISION"
fi
if [[ "$(git -C "$SOURCE_DIR" rev-parse HEAD)" != "$GOST_REVISION" ]] ||
   [[ -n "$(git -C "$SOURCE_DIR" status --porcelain)" ]]; then
  echo "GOST sources must be a clean checkout of $GOST_REVISION: $SOURCE_DIR" >&2
  exit 1
fi

cmake -S "$SOURCE_DIR" -B "$GOST_DIR/armv7" -G 'Unix Makefiles' \
  -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/gost/armv7-toolchain.cmake" \
  -DCMAKE_C_COMPILER_TARGET="$QCC_VARIANT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT/usr" \
  -DOPENSSL_INCLUDE_DIR="$OPENSSL_ROOT/usr/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_ROOT/usr/lib/libcrypto.so" \
  -DOPENSSL_SSL_LIBRARY="$OPENSSL_ROOT/usr/lib/libssl.so" \
  -DOPENSSL_ENGINES_DIR=/usr/lib/engines-1.1 \
  -DSKIP_PERL_TESTS=ON
cmake --build "$GOST_DIR/armv7" --target gost_engine --parallel "${JOBS:-4}"

install -Dm0755 "$OPENSSL_ROOT/usr/bin/openssl" "$STAGE_DIR/usr/bin/openssl"
install -Dm0755 "$OPENSSL_ROOT/usr/lib/libssl.so.1.1" "$STAGE_DIR/usr/lib/libssl.so.1.1"
install -Dm0755 "$GOST_DIR/armv7/bin/gost.so" "$STAGE_DIR/usr/lib/engines-1.1/gost.so"
ntoarmv7-strip --strip-unneeded "$STAGE_DIR/usr/lib/engines-1.1/gost.so"
install -Dm0644 "$SOURCE_DIR/LICENSE" "$STAGE_DIR/usr/share/licenses/gost-engine/LICENSE"
install -Dm0755 "$SCRIPT_DIR/gost/openssl-gost-selftest.sh" "$STAGE_DIR/usr/bin/openssl-gost-selftest"
mkdir -p "$STAGE_DIR/etc/openssl"
{
  printf 'openssl_conf = bvstk_openssl_init\n\n'
  cat "$OPENSSL_CONFIG"
  printf '\n'
  cat "$SCRIPT_DIR/gost/engine.cnf"
} > "$STAGE_DIR/etc/openssl/openssl.cnf"
{
  printf 'repository=%s\nrevision=%s\nqcc_variant=%s\n' "$GOST_REPOSITORY" "$GOST_REVISION" "$QCC_VARIANT"
  grep OPENSSL_VERSION_TEXT "$OPENSSL_HEADER"
  cd "$STAGE_DIR"
  sha256sum usr/bin/openssl usr/lib/libssl.so.1.1 usr/lib/engines-1.1/gost.so etc/openssl/openssl.cnf
} > "$STAGE_DIR/etc/openssl/gost-build.txt"

{
  printf '\n# OpenSSL and GOST ENGINE for Neutrino ARMv7\n'
  printf '[perms=0755] /usr/bin/openssl = %s/usr/bin/openssl\n' "$STAGE_DIR"
  printf '[perms=0755] libssl.so.1.1 = %s/usr/lib/libssl.so.1.1\n' "$STAGE_DIR"
  printf '[perms=0755] /usr/lib/engines-1.1/gost.so = %s/usr/lib/engines-1.1/gost.so\n' "$STAGE_DIR"
  printf '[perms=0755] /usr/bin/openssl-gost-selftest = %s/usr/bin/openssl-gost-selftest\n' "$STAGE_DIR"
  printf '[perms=0644] /etc/openssl/openssl.cnf = %s/etc/openssl/openssl.cnf\n' "$STAGE_DIR"
  printf '[perms=0644] /etc/openssl/gost-build.txt = %s/etc/openssl/gost-build.txt\n' "$STAGE_DIR"
  printf '[perms=0644] /usr/share/licenses/gost-engine/LICENSE = %s/usr/share/licenses/gost-engine/LICENSE\n' "$STAGE_DIR"
} > "$GOST_DIR/gost.build"

echo "Neutrino GOST ENGINE: $STAGE_DIR/usr/lib/engines-1.1/gost.so"
