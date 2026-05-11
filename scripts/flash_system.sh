#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

DEVICE_IP="${DEVICE_IP:-}"
DEVICE_USER="${DEVICE_USER:-root}"

source "${SCRIPT_DIR}/common.sh"

BUILD_BEFORE_FLASH=false
SELECTED_SKU="${FLASH_SKU:-${SYSTEM_SKU:-}}"

show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -r, --remote <ip>   Device IP address"
    echo "  -u, --user <user>   Remote username (default: ${DEVICE_USER})"
    echo "  --build             Build the selected target before flashing"
    echo "  --sku <target>      Target to flash: emmc, sdmmc, ${EMMC_SKU}, or ${SDMMC_SKU}"
    echo "  --help              Show this help message"
    echo
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--remote)
            require_arg "$1" "${2:-}"
            DEVICE_IP="$2"
            shift 2
            ;;
        -u|--user)
            require_arg "$1" "${2:-}"
            DEVICE_USER="$2"
            shift 2
            ;;
        --build)
            BUILD_BEFORE_FLASH=true
            shift
            ;;
        --sku|--target|--variant)
            require_arg "$1" "${2:-}"
            SELECTED_SKU="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            msg_err "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

if [ -z "$DEVICE_IP" ]; then
    if [ ! -t 0 ]; then
        msg_err "Error: device IP is required in non-interactive mode"
        msg_err "Set DEVICE_IP=<ip> or pass -r <ip>."
        exit 1
    fi

    printf "Device IP: " >&2
    read -r DEVICE_IP

    if [ -z "$DEVICE_IP" ]; then
        msg_err "Error: device IP is required"
        exit 1
    fi
fi

if [ -n "$SELECTED_SKU" ]; then
    SELECTED_SKU=$(normalize_system_sku "$SELECTED_SKU") || exit 1
else
    SELECTED_SKU=$(prompt_system_sku) || exit 1
fi

SELECTED_LABEL=$(system_variant_label "$SELECTED_SKU") || exit 1

check_ping "${DEVICE_IP}"
check_ssh "${DEVICE_USER}" "${DEVICE_IP}"

if [ "$BUILD_BEFORE_FLASH" = true ]; then
    msg_info ">> Building ${SELECTED_LABEL} system image before flash..."
    PROMPT_VARIANT_TESTS=0 "${SCRIPT_DIR}/build_system.sh" --sku "$SELECTED_SKU"

    # The build can take a while; re-confirm the device is still reachable.
    msg_info ">> Re-checking device before flash..."
    check_ping "${DEVICE_IP}"
    check_ssh "${DEVICE_USER}" "${DEVICE_IP}"
fi

system_tar="$(system_variant_dir "$SELECTED_SKU")/${SYSTEM_TAR_NAME}"
if [ ! -f "$system_tar" ]; then
    msg_err "Error: ${system_tar} not found."
    msg_err "Re-run with --build to build this target first."
    exit 1
fi

msg_info ">> Flashing ${SELECTED_LABEL} system image (${SELECTED_SKU}) to ${DEVICE_USER}@${DEVICE_IP}..."
msg_info "  Transferring ${SYSTEM_TAR_NAME} to /userdata/jetkvm/update_system.tar..."
sshdev "cat > /userdata/jetkvm/update_system.tar" < "$system_tar"
msg_ok "  OK: Transfer complete"

msg_info "  Running rk_ota..."
sshdev "rk_ota --misc=update --tar_path=/userdata/jetkvm/update_system.tar --save_dir=/userdata/jetkvm/ota_save --partition=all"
msg_ok "  OK: rk_ota completed"

msg_info "  Rebooting device..."
sshdev "reboot" || true

wait_for_device

msg_ok "OK: System flash completed"
