#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")

source "${SCRIPT_DIR}/common.sh"

DEVICE_IP="${DEVICE_IP:-}"
DEVICE_USER="${DEVICE_USER:-root}"

if [ -z "$DEVICE_IP" ]; then
    msg_err "Error: DEVICE_IP is required"
    msg_err "Usage: DEVICE_IP=<ip> $0"
    exit 1
fi

check_ping "$DEVICE_IP"
check_ssh "$DEVICE_USER" "$DEVICE_IP"
