#!/usr/bin/env bash
set -euo pipefail

DEVICE_IP="${DEVICE_IP:-192.168.0.10}"
SSH_IDENTITY="${SSH_IDENTITY:-/home/ilya/neutrino/ax7020_ssh_client}"
SSH_USER="${SSH_USER:-root}"

if [[ ! -f "$SSH_IDENTITY" ]]; then
  echo "SSH identity not found: $SSH_IDENTITY" >&2
  exit 1
fi

SSH_OPTIONS=(
  -i "$SSH_IDENTITY"
  -o BatchMode=yes
  -o ConnectTimeout=2
  -o StrictHostKeyChecking=no
  -o UserKnownHostsFile=/dev/null
)

for attempt in $(seq 1 30); do
  if ssh "${SSH_OPTIONS[@]}" "$SSH_USER@$DEVICE_IP" \
      '/usr/bin/bvstkctl version && /usr/bin/bvstkctl pl list'; then
    exit 0
  fi
  sleep 1
done

echo "Neutrino SSH verification failed for $SSH_USER@$DEVICE_IP" >&2
exit 1
