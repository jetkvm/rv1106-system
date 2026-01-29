#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

source "${SCRIPT_DIR}/common.sh"

if [ -z "${BUILD_VERSION:-}" ]; then
    base_version=$(cat "${ROOT_DIR}/VERSION" 2>/dev/null || echo "0.0.0")
    export BUILD_VERSION="${base_version}-dev$(date -u +%Y%m%d%H%M)"
    export BUILD_VERSION_SOURCE="local-dev"
fi

msg_info ">> Building rv1106-system..."
cd "$ROOT_DIR"

msg_info "  Updating JetKVM app binary..."
./update_app.sh

msg_info "  Running build.sh lunch..."
./build.sh lunch BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk

msg_info "  Running build.sh..."
./build.sh

if [ ! -f "$OTA_TAR" ]; then
    msg_err "Error: $OTA_TAR not found after build"
    exit 1
fi
if [ ! -f "$FULL_IMG" ]; then
    msg_err "Error: $FULL_IMG not found after build"
    exit 1
fi

msg_info "  Computing SHA256 checksums..."
sha256sum "$OTA_TAR" | awk '{print $1}' > "${OTA_TAR}.sha256"
sha256sum "$FULL_IMG" | awk '{print $1}' > "${FULL_IMG}.sha256"

msg_ok "OK: Build completed"
