#!/usr/bin/env bash
set -e

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

source "${SCRIPT_DIR}/common.sh"

msg_info ">> Building rv1106-system..."
cd "$ROOT_DIR"

msg_info "  Cleaning build environment..."
if [ -d "${ROOT_DIR}/output" ]; then
    msg_info "  Removing output directory..."
    # We need sudo to remove the output directory
    sudo rm -rf "${ROOT_DIR}/output" || true
fi
./build.sh clean

msg_info "  Running build.sh lunch..."
./build.sh lunch BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk

msg_info "  Running build.sh..."
./build.sh

if [ ! -f "output/image/update_ota.tar" ]; then
    msg_err "Error: output/image/update_ota.tar not found after build"
    exit 1
fi
if [ ! -f "output/image/update.img" ]; then
    msg_err "Error: output/image/update.img not found after build"
    exit 1
fi

msg_info "  Computing SHA256 checksums..."
sha256sum output/image/update_ota.tar | awk '{print $1}' > output/image/update_ota.tar.sha256
sha256sum output/image/update.img | awk '{print $1}' > output/image/update.img.sha256

msg_ok "OK: Build completed"
