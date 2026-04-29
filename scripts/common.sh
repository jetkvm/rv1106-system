#!/usr/bin/env bash
set -e
set -o pipefail

# -----------------------------------------------------------------------------
# Color output helpers
# -----------------------------------------------------------------------------
HAS_TTY=true
if [ -z "$TERM" ] || [ "$TERM" = "dumb" ]; then
    HAS_TTY=false
fi

# Use $'...' syntax for ANSI codes (portable in bash)
C_RST=$'\e[0m'
C_ERR=$'\e[31m'
C_OK=$'\e[32m'
C_WARN=$'\e[33m'
C_INFO=$'\e[35m'

if [ "$HAS_TTY" = true ] && command -v tput >/dev/null 2>&1; then
    C_RST="$(tput sgr0 2>/dev/null || echo "$C_RST")"
    C_ERR="$(tput setaf 1 2>/dev/null || echo "$C_ERR")"
    C_OK="$(tput setaf 2 2>/dev/null || echo "$C_OK")"
    C_WARN="$(tput setaf 3 2>/dev/null || echo "$C_WARN")"
    C_INFO="$(tput setaf 5 2>/dev/null || echo "$C_INFO")"
fi

msg() { printf '%s%s%s\n' "$2" "$1" "$C_RST"; }
msg_info() { msg "$1" "$C_INFO"; }
msg_ok() { msg "$1" "$C_OK"; }
msg_err() { msg "$1" "$C_ERR"; }
msg_warn() { msg "$1" "$C_WARN"; }

# -----------------------------------------------------------------------------
# SSH helper
# -----------------------------------------------------------------------------
sshdev() {
    if [ -z "${DEVICE_IP:-}" ]; then
        msg_err "Error: DEVICE_IP is not set"
        exit 1
    fi
    local user="${DEVICE_USER:-root}"
    ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no "${user}@${DEVICE_IP}" "$@"
}

# -----------------------------------------------------------------------------
# Connectivity checks
# -----------------------------------------------------------------------------
check_ping() {
    local host=$1
    msg_info ">> Checking if device is reachable at ${host}..."
    if ! ping -c 3 -W 5 "${host}" > /dev/null 2>&1; then
        msg_err "Error: Cannot reach device at ${host}"
        msg_err "Please verify the IP address and network connectivity"
        exit 1
    fi
    msg_info "OK: Device is reachable"
}

check_ssh() {
    local user=$1
    local host=$2
    msg_info ">> Checking SSH connectivity to ${user}@${host}..."
    if ! ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o ConnectTimeout=10 "${user}@${host}" "echo 'SSH connection successful'" > /dev/null 2>&1; then
        msg_err "Error: Cannot establish SSH connection to ${user}@${host}"
        msg_err "Please verify SSH access and credentials"
        exit 1
    fi
    msg_info "OK: SSH connection successful"
}

wait_for_device() {
    msg_info ">> Waiting for device to come back online..."

    # Wait a bit for reboot to start
    sleep 10

    # Wait for ping (up to 120 seconds)
    msg_info "  Waiting for device to respond to ping..."
    local max_attempts=60
    local attempt=1
    while [ $attempt -le $max_attempts ]; do
        if ping -c 1 -W 2 "${DEVICE_IP}" > /dev/null 2>&1; then
            msg_ok "  OK: Device is responding to ping"
            break
        fi
        attempt=$((attempt + 1))
        sleep 2
    done

    if [ $attempt -gt $max_attempts ]; then
        msg_err "Error: Device did not respond to ping within timeout"
        exit 1
    fi

    # Wait for SSH (up to 60 seconds)
    msg_info "  Waiting for SSH to become available..."
    max_attempts=30
    attempt=1
    while [ $attempt -le $max_attempts ]; do
        if sshdev "echo 'SSH OK'" > /dev/null 2>&1; then
            msg_ok "  OK: SSH is available"
            break
        fi
        attempt=$((attempt + 1))
        sleep 2
    done

    if [ $attempt -gt $max_attempts ]; then
        msg_err "Error: SSH did not become available within timeout"
        exit 1
    fi

    # Wait for web interface (up to 180 seconds)
    msg_info "  Waiting for web interface..."
    max_attempts=90
    attempt=1
    while [ $attempt -le $max_attempts ]; do
        if curl -s --max-time 5 "http://${DEVICE_IP}" > /dev/null 2>&1; then
            msg_ok "  OK: Web interface is ready"
            break
        fi
        attempt=$((attempt + 1))
        sleep 2
    done

    if [ $attempt -gt $max_attempts ]; then
        msg_err "Error: Web interface did not become available within timeout"
        exit 1
    fi

    msg_ok "OK: Device is online and ready"
}

# -----------------------------------------------------------------------------
# Build artifact paths
# -----------------------------------------------------------------------------
OUTPUT_IMAGE_DIR="${ROOT_DIR:-$(pwd)}/output/image"
OTA_TAR_NAME="update_ota.tar"
FULL_IMG_NAME="update.img"
SD_IMG_ZIP_NAME="update_sd.img.zip"
OTA_TAR="${OUTPUT_IMAGE_DIR}/${OTA_TAR_NAME}"
FULL_IMG="${OUTPUT_IMAGE_DIR}/${FULL_IMG_NAME}"
SD_IMG_ZIP="${OUTPUT_IMAGE_DIR}/${SD_IMG_ZIP_NAME}"

# -----------------------------------------------------------------------------
# System release variants
# -----------------------------------------------------------------------------
SYSTEM_RELEASE_DIR="${ROOT_DIR:-$(pwd)}/release-artifacts/system"
SYSTEM_TAR_NAME="system.tar"

EMMC_SKU="jetkvm-v2"
EMMC_BOARD_CONFIG="BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk"
SDMMC_SKU="jetkvm-v2-sdmmc"
SDMMC_BOARD_CONFIG="BoardConfig_IPC/BoardConfig-SDMMC-NONE-RV1106_JETKVM_V2.mk"

system_variant_dir() {
    local sku="$1"
    echo "${SYSTEM_RELEASE_DIR}/${sku}"
}

# Recovery artifact filename per SKU (eMMC -> update.img, SDMMC ->
# update_sd.img.zip). Keep in sync with RECOVERY_ARTIFACT_BY_SKU in
# cloud-api/src/releases.ts.
recovery_artifact_for_sku() {
    local sku="$1"
    case "$sku" in
        "$SDMMC_SKU") echo "$SD_IMG_ZIP_NAME" ;;
        "$EMMC_SKU")  echo "$FULL_IMG_NAME" ;;
        *)
            msg_err "Error: no recovery artifact mapped for SKU '${sku}'"
            exit 1
            ;;
    esac
}

# Absolute path of the recovery artifact in the build output dir.
recovery_source_for_sku() {
    local sku="$1"
    local artifact
    # Capture separately so an unknown-SKU exit in recovery_artifact_for_sku
    # (which would otherwise only kill the command-substitution subshell)
    # actually fails this function.
    artifact=$(recovery_artifact_for_sku "$sku") || return $?
    echo "${OUTPUT_IMAGE_DIR}/${artifact}"
}
