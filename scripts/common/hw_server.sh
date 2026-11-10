#!/usr/bin/env bash

# Convert a host, host:port, or tcp:host:port to the XSCT URL form.
bvstk_set_hw_server_url() {
  local address="${1#tcp:}" host port
  case "$address" in
    *:*:*) echo "Invalid hw_server address: $1" >&2; return 2 ;;
    *:*) host="${address%:*}"; port="${address##*:}" ;;
    *) host="$address"; port=3121 ;;
  esac
  if [[ ! "$host" =~ ^[a-zA-Z0-9._-]+$ || ! "$port" =~ ^[0-9]{1,5}$ ]] ||
     (( 10#$port < 1 || 10#$port > 65535 )); then
    echo "Invalid hw_server address: $1 (expected host or host:port)" >&2
    return 2
  fi
  export HW_SERVER_URL="tcp:$host:$((10#$port))"
}
