#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")

DEVICE_IP="${DEVICE_IP:-192.168.1.77}"
DEVICE_USER="${DEVICE_USER:-root}"
KVM_REPO="${KVM_REPO:-https://github.com/jetkvm/kvm.git}"
KVM_BRANCH="${KVM_BRANCH:-dev}"
KVM_DIR=""
TEMP_DIR=""

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -r, --remote <ip>     Device IP address (default: ${DEVICE_IP})"
    echo "  -u, --user <user>     Remote username (default: ${DEVICE_USER})"
    echo "      --kvm-dir <path>  Path to existing kvm repo (skips clone)"
    echo "      --kvm-branch <b>  KVM branch to clone (default: ${KVM_BRANCH})"
    echo "  --help               Show this help message"
    echo
}

cleanup() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        msg_info "Cleaning up temporary directory: $TEMP_DIR"
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

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
        --kvm-dir)
            KVM_DIR="$2"
            if [ ! -d "$KVM_DIR" ]; then
                msg_err "Error: KVM directory does not exist: $KVM_DIR"
                exit 1
            fi
            shift 2
            ;;
        --kvm-branch)
            KVM_BRANCH="$2"
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

if [ -z "$KVM_DIR" ]; then
    TEMP_DIR=$(mktemp -d)
    KVM_DIR="$TEMP_DIR/kvm"
    msg_info ">> Cloning jetkvm/kvm (${KVM_BRANCH})..."
    git clone --branch "$KVM_BRANCH" --depth 1 "$KVM_REPO" "$KVM_DIR"
    msg_ok "OK: Cloned to $KVM_DIR"
else
    msg_info ">> Using existing KVM directory: $KVM_DIR"
fi

cd "$KVM_DIR"
msg_info ">> Running E2E tests (this deploys the app)..."
make frontend
make test_e2e DEVICE_IP="$DEVICE_IP"
msg_ok "OK: E2E tests passed"
