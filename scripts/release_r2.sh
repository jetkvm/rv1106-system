#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

BUILD_VERSION=""
R2_PATH="${R2_PATH:-r2://jetkvm-update/system}"

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 --version <version>"
    echo
    echo "Options:"
    echo "  --version <version>   Release version (e.g., 0.2.7)"
    echo "  --help                Show this help message"
    echo
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            BUILD_VERSION="$2"
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

if [ -z "$BUILD_VERSION" ]; then
    msg_err "Error: --version is required"
    exit 1
fi

command -v rclone >/dev/null 2>&1 || { msg_err "Error: rclone not installed"; exit 1; }

cd "$ROOT_DIR"

ota_tar="$OTA_TAR"
ota_sha="${OTA_TAR}.sha256"
full_img="$FULL_IMG"
img_sha="${FULL_IMG}.sha256"

for file in "$ota_tar" "$full_img"; do
    if [ ! -f "$file" ]; then
        msg_err "Error: Required file not found: $file"
        exit 1
    fi
done

if [ ! -f "$ota_sha" ]; then
    sha256sum "$ota_tar" | awk '{print $1}' > "$ota_sha"
fi
if [ ! -f "$img_sha" ]; then
    sha256sum "$full_img" | awk '{print $1}' > "$img_sha"
fi

if rclone lsf "${R2_PATH}/${BUILD_VERSION}/" 2>/dev/null | grep -q .; then
    msg_err "Error: Version ${BUILD_VERSION} already exists in R2"
    exit 1
fi

ota_hash=$(cat "$ota_sha")
img_hash=$(cat "$img_sha")

echo ""
msg_info "═══════════════════════════════════════════════════════"
msg_info "  R2 Upload"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Destination: ${R2_PATH}/${BUILD_VERSION}/"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Files to upload:"
msg_info "    - update_ota.tar     → system.tar"
msg_info "      SHA256: ${ota_hash}"
msg_info "    - update.img         → update.img"
msg_info "      SHA256: ${img_hash}"
msg_info "═══════════════════════════════════════════════════════"
echo ""
read -p "The R2 upload is prepared. These are the files. Do you want to continue? [y/N] " confirm
if [ "$confirm" != "y" ]; then
    msg_warn "R2 upload cancelled."
    exit 1
fi

msg_info ">> Uploading to R2..."
rclone copyto --progress "$ota_tar" "${R2_PATH}/${BUILD_VERSION}/system.tar"
rclone copyto --progress "$ota_sha" "${R2_PATH}/${BUILD_VERSION}/system.tar.sha256"
rclone copyto --progress "$full_img" "${R2_PATH}/${BUILD_VERSION}/update.img"
rclone copyto --progress "$img_sha" "${R2_PATH}/${BUILD_VERSION}/update.img.sha256"

msg_ok "OK: Uploaded to R2: ${R2_PATH}/${BUILD_VERSION}/"
