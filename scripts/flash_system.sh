#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

DEVICE_IP="${DEVICE_IP:-192.168.1.77}"
DEVICE_USER="${DEVICE_USER:-root}"

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -r, --remote <ip>   Device IP address (default: ${DEVICE_IP})"
    echo "  -u, --user <user>   Remote username (default: ${DEVICE_USER})"
    echo "  --help              Show this help message"
    echo
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--remote)
            DEVICE_IP="$2"
            shift 2
            ;;
        -u|--user)
            DEVICE_USER="$2"
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

check_ping "${DEVICE_IP}"
check_ssh "${DEVICE_USER}" "${DEVICE_IP}"

ota_tar="$OTA_TAR"
if [ ! -f "$ota_tar" ]; then
    msg_err "Error: ${ota_tar} not found. Run 'make build' first."
    exit 1
fi

msg_info ">> Flashing system image to device..."
msg_info "  Transferring update_ota.tar to /userdata/jetkvm/update_system.tar......"
sshdev "cat > /userdata/jetkvm/update_system.tar" < "$ota_tar"
msg_ok "  OK: Transfer complete"

msg_info "  Running rk_ota..."
sshdev "rk_ota --misc=update --tar_path=/userdata/jetkvm/update_system.tar --save_dir=/userdata/jetkvm/ota_save --partition=all"
msg_ok "  OK: rk_ota completed"

msg_info "  Rebooting device..."
sshdev "reboot" || true

wait_for_device

msg_ok "OK: System flash completed"
