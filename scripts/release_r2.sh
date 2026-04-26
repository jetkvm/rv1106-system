#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

BUILD_VERSION=""
R2_PATH="${R2_PATH:-r2://jetkvm-update/system}"
SIGNING_KEY_FPR=""
UNSIGNED=false
OTA_ROOT_KEY_FPR="AF5A36A993D828FEFE7C18C2D1B9856C26A79E95"

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 --version <version> (--signing-key <fingerprint> | --unsigned)"
    echo
    echo "Options:"
    echo "  --version <version>   Release version (e.g., 0.2.7)"
    echo "  --signing-key <fpr>   Sign the OTA payload with the trusted production key"
    echo "  --unsigned            Upload without an OTA signature (dev/prerelease only)"
    echo "  --help                Show this help message"
    echo
}

validate_signing_key() {
    local signing_key_fpr="$1"
    local root_fpr

    if [ -z "$signing_key_fpr" ]; then
        msg_err "Error: signing key fingerprint is required"
        exit 1
    fi

    command -v gpg >/dev/null 2>&1 || { msg_err "Error: gpg not installed"; exit 1; }
    gpg --list-secret-keys --with-colons "$signing_key_fpr" >/dev/null 2>&1 || {
        msg_err "Error: Signing key ${signing_key_fpr} not found in local GPG keyring"
        exit 1
    }

    root_fpr=$(gpg --list-secret-keys --with-colons "$signing_key_fpr" | awk -F: '/^fpr:/ { print $10; exit }')
    if [ -z "$root_fpr" ]; then
        msg_err "Error: Could not determine root fingerprint for signing key ${signing_key_fpr}"
        exit 1
    fi

    if [ "$root_fpr" != "$OTA_ROOT_KEY_FPR" ]; then
        msg_err "Error: Signing key ${signing_key_fpr} belongs to root ${root_fpr}, expected ${OTA_ROOT_KEY_FPR}"
        exit 1
    fi
}

require_arg() {
    local option="$1"
    local value="${2:-}"

    if [ -z "$value" ]; then
        msg_err "Error: ${option} requires a value"
        exit 1
    fi
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            require_arg "$1" "${2:-}"
            BUILD_VERSION="$2"
            shift 2
            ;;
        --signing-key)
            require_arg "$1" "${2:-}"
            SIGNING_KEY_FPR="$2"
            shift 2
            ;;
        --unsigned)
            UNSIGNED=true
            shift
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

if [ -n "$SIGNING_KEY_FPR" ] && [ "$UNSIGNED" = true ]; then
    msg_err "Error: choose either --signing-key or --unsigned, not both"
    exit 1
fi

if [ -z "$SIGNING_KEY_FPR" ] && [ "$UNSIGNED" != true ]; then
    msg_err "Error: choose --signing-key <fingerprint> for production or --unsigned for dev/prerelease uploads"
    exit 1
fi

if [ -n "$SIGNING_KEY_FPR" ]; then
    validate_signing_key "$SIGNING_KEY_FPR"
fi

command -v rclone >/dev/null 2>&1 || { msg_err "Error: rclone not installed"; exit 1; }

cd "$ROOT_DIR"

ota_tar="$OTA_TAR"
ota_sha="${OTA_TAR}.sha256"
ota_sig="${OTA_TAR}.sig"
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

if [ -n "$SIGNING_KEY_FPR" ]; then
    msg_info ">> Signing OTA payload with ${SIGNING_KEY_FPR}..."
    read -p "Ensure the YubiKey is inserted and ready, then continue signing? [y/N] " confirm_sign
    if [ "$confirm_sign" != "y" ]; then
        msg_warn "Signing cancelled."
        exit 1
    fi
    rm -f "$ota_sig"

    for attempt in 1 2 3; do
        if gpg --yes --detach-sign --output "$ota_sig" --local-user "$SIGNING_KEY_FPR" "$ota_tar"; then
            break
        fi
        rm -f "$ota_sig"
        if [ "$attempt" -eq 3 ]; then
            msg_err "Error: GPG signing failed after 3 attempts"
            exit 1
        fi
        msg_warn "GPG signing failed (attempt ${attempt}/3). Please retry."
    done

    if [ ! -f "$ota_sig" ]; then
        msg_err "Error: Signature file not created: $ota_sig"
        exit 1
    fi
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
if [ -n "$SIGNING_KEY_FPR" ]; then
    msg_info "      Signature: ${ota_sig} → system.tar.sig"
fi
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
if [ -n "$SIGNING_KEY_FPR" ]; then
    rclone copyto --progress "$ota_sig" "${R2_PATH}/${BUILD_VERSION}/system.tar.sig"
fi
rclone copyto --progress "$full_img" "${R2_PATH}/${BUILD_VERSION}/update.img"
rclone copyto --progress "$img_sha" "${R2_PATH}/${BUILD_VERSION}/update.img.sha256"

msg_ok "OK: Uploaded to R2: ${R2_PATH}/${BUILD_VERSION}/"
