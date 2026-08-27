#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"
DEFAULT_BSP_DIR="$REPO_ROOT/third_party/neutrino/bsp/ax7020"
BSP_DIR="${NEUTRINO_BSP_DIR:-$DEFAULT_BSP_DIR}"
BASE_BUILD="${NEUTRINO_BASE_BUILD:-$BSP_DIR/images/zynq7000-ax7020-ssh.build}"
GENERATED_BUILD="$BUILD_DIR/zynq7000-ax7020-bvstk.build"
IFS_FILE="${NEUTRINO_IFS_FILE:-$BUILD_DIR/ifs-zynq7000-ax7020-bvstk.raw}"
BVSTKCTL="$BUILD_DIR/bvstkctl"
BVSTKD="$BUILD_DIR/bvstkd"
I2C_CLIENT="$BUILD_DIR/i2c"
BVSTK_SHELL="$BUILD_DIR/bvstk-shell"
BVSTK_QSPI_FAT="$BUILD_DIR/bvstk-qspi-fat"
BVSTK_SD_RAW="$BUILD_DIR/bvstk-sd-raw"
SSH_IDENTITY="${SSH_IDENTITY:-$BUILD_DIR/ax7020_ssh_client}"
SSH_KEY_DIR="${NEUTRINO_KEY_DIR:-$BUILD_DIR/ssh}"
SSH_HOST_KEY="$SSH_KEY_DIR/ssh_host_rsa_key"
SSH_AUTHORIZED_KEYS="$SSH_KEY_DIR/authorized_keys"
ROOT_SHADOW_FILE="${NEUTRINO_ROOT_SHADOW_FILE:-$BUILD_DIR/root.shadow}"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "$1 not found; source the Neutrino 2024 development kit" >&2
    exit 1
  fi
}

ensure_ssh_keys() {
  local ssh_identity_pub="$SSH_IDENTITY.pub"

  umask 077
  mkdir -p "$SSH_KEY_DIR" "$(dirname "$SSH_IDENTITY")"

  if [[ ! -f "$SSH_HOST_KEY" || ! -f "$SSH_HOST_KEY.pub" ]]; then
    ssh-keygen -q -t rsa -b 2048 -m PEM -N '' -f "$SSH_HOST_KEY"
  fi

  if [[ ! -f "$SSH_IDENTITY" ]]; then
    ssh-keygen -q -t rsa -b 2048 -N '' -C ax7020-neutrino -f "$SSH_IDENTITY"
  elif [[ ! -f "$ssh_identity_pub" ]]; then
    ssh-keygen -y -f "$SSH_IDENTITY" > "$ssh_identity_pub"
  fi

  cp "$ssh_identity_pub" "$SSH_AUTHORIZED_KEYS"
  chmod 0600 "$SSH_HOST_KEY" "$SSH_IDENTITY" "$SSH_AUTHORIZED_KEYS"
  chmod 0644 "$SSH_HOST_KEY.pub" "$ssh_identity_pub"
}

ensure_root_shadow() {
  umask 077
  mkdir -p "$BUILD_DIR"

  if [[ -n "${NEUTRINO_ROOT_SHADOW_FILE:-}" ]]; then
    if [[ ! -f "$ROOT_SHADOW_FILE" ]]; then
      echo "Neutrino root shadow file not found: $ROOT_SHADOW_FILE" >&2
      exit 1
    fi
    return
  fi

  if [[ ! -f "$ROOT_SHADOW_FILE" ]]; then
    printf 'root:*:90:18565:0:0:0:0:0\n' > "$ROOT_SHADOW_FILE"
  fi
}

if ! command -v mkifs >/dev/null 2>&1 && [[ -r /etc/profile.d/kpda_env_2024.sh ]]; then
  # shellcheck disable=SC1091
  source /etc/profile.d/kpda_env_2024.sh
fi

require_command mkifs
require_command ssh-keygen
if [[ ! -f "$BASE_BUILD" ]]; then
  echo "Base Neutrino build file not found: $BASE_BUILD" >&2
  exit 1
fi
if [[ ! -d "$BSP_DIR/install" ]]; then
  echo "BSP install tree not found: $BSP_DIR/install" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"
ensure_ssh_keys
ensure_root_shadow
"$SCRIPT_DIR/build.sh"
cp "$BASE_BUILD" "$GENERATED_BUILD"
printf '\n# Burevestnik multi-OS applications\n[perms=0755] /usr/bin/i2c = %s\n[perms=0755] /usr/bin/bvstkctl = %s\n[perms=0755] /usr/bin/bvstkd = %s\n[perms=0755] /usr/bin/bvstk-shell = %s\n[perms=0755] /usr/sbin/bvstk-qspi-fat = %s\n' \
  "$I2C_CLIENT" "$BVSTKCTL" "$BVSTKD" "$BVSTK_SHELL" "$BVSTK_QSPI_FAT" >> "$GENERATED_BUILD"
printf '[perms=0755] /usr/sbin/bvstk-sd-raw = %s\n' "$BVSTK_SD_RAW" >> "$GENERATED_BUILD"

BVSTK_SSH_HOST_KEY="$SSH_HOST_KEY" \
BVSTK_SSH_HOST_KEY_PUB="$SSH_HOST_KEY.pub" \
BVSTK_SSH_AUTHORIZED_KEYS="$SSH_AUTHORIZED_KEYS" \
BVSTK_ROOT_SHADOW_FILE="$ROOT_SHADOW_FILE" \
BVSTK_KSHRC="$REPO_ROOT/scripts/neutrino/kshrc" \
BVSTK_PROFILE="$REPO_ROOT/scripts/neutrino/profile" \
mkifs -r "$BSP_DIR/install" "$GENERATED_BUILD" "$IFS_FILE"

echo "Neutrino IFS: $IFS_FILE"
echo "Included applications: /usr/bin/i2c, /usr/bin/bvstkctl, /usr/bin/bvstkd, /usr/bin/bvstk-shell, /usr/sbin/bvstk-qspi-fat and /usr/sbin/bvstk-sd-raw"
