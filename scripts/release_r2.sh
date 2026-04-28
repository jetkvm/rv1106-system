#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

BUILD_VERSION=""
R2_PATH="${R2_PATH:-r2://jetkvm-update/system}"
SIGNING_KEY_FPR=""
UNSIGNED=false
DRY_RUN=false
OTA_ROOT_KEY_FPR="AF5A36A993D828FEFE7C18C2D1B9856C26A79E95"

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 --version <version> (--signing-key <fingerprint> | --unsigned) [--dry-run]"
    echo
    echo "Options:"
    echo "  --version <version>   Release version (e.g., 0.2.7)"
    echo "  --signing-key <fpr>   Sign the OTA payload with the trusted production key"
    echo "  --unsigned            Upload without an OTA signature (dev/prerelease only)"
    echo "  --dry-run             Sign and validate, but do not upload to R2"
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
        --dry-run)
            DRY_RUN=true
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

if [ "$DRY_RUN" != true ]; then
    command -v rclone >/dev/null 2>&1 || { msg_err "Error: rclone not installed"; exit 1; }
fi

cd "$ROOT_DIR"

validate_system_variant() {
    local label="$1"
    local sku="$2"
    local stage_dir
    local recovery_name

    stage_dir=$(system_variant_dir "$sku")
    recovery_name=$(recovery_artifact_for_sku "$sku")
    for file in \
        "${stage_dir}/${SYSTEM_TAR_NAME}" \
        "${stage_dir}/${SYSTEM_TAR_NAME}.sha256" \
        "${stage_dir}/${recovery_name}" \
        "${stage_dir}/${recovery_name}.sha256"
    do
        if [ ! -f "$file" ]; then
            msg_err "Error: Required ${label} artifact not found: $file"
            msg_err "Run 'make build' first."
            exit 1
        fi
    done
}

sign_system_variant() {
    local label="$1"
    local sku="$2"
    local stage_dir
    local system_tar
    local system_sig

    stage_dir=$(system_variant_dir "$sku")
    system_tar="${stage_dir}/${SYSTEM_TAR_NAME}"
    system_sig="${system_tar}.sig"
    rm -f "$system_sig"

    msg_info "  Signing ${label} OTA payload (${sku})..."
    for attempt in 1 2 3; do
        if gpg --yes --detach-sign --output "$system_sig" --local-user "$SIGNING_KEY_FPR" "$system_tar"; then
            break
        fi
        rm -f "$system_sig"
        if [ "$attempt" -eq 3 ]; then
            msg_err "Error: GPG signing failed after 3 attempts for ${label}"
            exit 1
        fi
        msg_warn "GPG signing failed for ${label} (attempt ${attempt}/3). Please retry."
    done

    if [ ! -f "$system_sig" ]; then
        msg_err "Error: Signature file not created: $system_sig"
        exit 1
    fi
}

print_system_variant() {
    local label="$1"
    local sku="$2"
    local stage_dir
    local recovery_name
    local system_hash
    local recovery_hash

    stage_dir=$(system_variant_dir "$sku")
    recovery_name=$(recovery_artifact_for_sku "$sku")
    system_hash=$(< "${stage_dir}/${SYSTEM_TAR_NAME}.sha256")
    recovery_hash=$(< "${stage_dir}/${recovery_name}.sha256")

    msg_info "    - ${label} (${sku})"
    msg_info "      ${SYSTEM_TAR_NAME} → skus/${sku}/${SYSTEM_TAR_NAME}"
    msg_info "      SHA256: ${system_hash}"
    if [ -n "$SIGNING_KEY_FPR" ]; then
        msg_info "      Signature: ${stage_dir}/${SYSTEM_TAR_NAME}.sig → skus/${sku}/${SYSTEM_TAR_NAME}.sig"
    fi
    msg_info "      ${recovery_name} → skus/${sku}/${recovery_name}"
    msg_info "      SHA256: ${recovery_hash}"
}

upload_system_variant() {
    local sku="$1"
    local stage_dir
    local recovery_name
    local dest

    stage_dir=$(system_variant_dir "$sku")
    recovery_name=$(recovery_artifact_for_sku "$sku")
    dest="${R2_PATH}/${BUILD_VERSION}/skus/${sku}"

    rclone copyto --progress "${stage_dir}/${SYSTEM_TAR_NAME}" "${dest}/${SYSTEM_TAR_NAME}"
    rclone copyto --progress "${stage_dir}/${SYSTEM_TAR_NAME}.sha256" "${dest}/${SYSTEM_TAR_NAME}.sha256"
    if [ -n "$SIGNING_KEY_FPR" ]; then
        rclone copyto --progress "${stage_dir}/${SYSTEM_TAR_NAME}.sig" "${dest}/${SYSTEM_TAR_NAME}.sig"
    fi
    rclone copyto --progress "${stage_dir}/${recovery_name}" "${dest}/${recovery_name}"
    rclone copyto --progress "${stage_dir}/${recovery_name}.sha256" "${dest}/${recovery_name}.sha256"
}

validate_system_variant "SDMMC" "$SDMMC_SKU"
validate_system_variant "EMMC" "$EMMC_SKU"

if [ "$DRY_RUN" != true ]; then
    if rclone lsf "${R2_PATH}/${BUILD_VERSION}/" 2>/dev/null | grep -q .; then
        msg_err "Error: Version ${BUILD_VERSION} already exists in R2"
        exit 1
    fi
fi

if [ -n "$SIGNING_KEY_FPR" ]; then
    msg_info ">> Signing OTA payloads with ${SIGNING_KEY_FPR}..."
    read -p "Ensure the YubiKey is inserted and ready, then continue signing? [y/N] " confirm_sign
    if [ "$confirm_sign" != "y" ]; then
        msg_warn "Signing cancelled."
        exit 1
    fi

    sign_system_variant "SDMMC" "$SDMMC_SKU"
    sign_system_variant "EMMC" "$EMMC_SKU"
fi

echo ""
msg_info "═══════════════════════════════════════════════════════"
if [ "$DRY_RUN" = true ]; then
    msg_info "  R2 Upload Dry Run"
else
    msg_info "  R2 Upload"
fi
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Destination: ${R2_PATH}/${BUILD_VERSION}/skus/<sku>/"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Files to upload:"
print_system_variant "SDMMC" "$SDMMC_SKU"
print_system_variant "EMMC" "$EMMC_SKU"
msg_info "═══════════════════════════════════════════════════════"
echo ""

if [ "$DRY_RUN" = true ]; then
    msg_ok "OK: R2 dry run complete; skipped upload"
    exit 0
fi

read -p "The R2 upload is prepared. These are the files. Do you want to continue? [y/N] " confirm
if [ "$confirm" != "y" ]; then
    msg_warn "R2 upload cancelled."
    exit 1
fi

msg_info ">> Uploading to R2..."
upload_system_variant "$SDMMC_SKU"
upload_system_variant "$EMMC_SKU"

msg_ok "OK: Uploaded to R2: ${R2_PATH}/${BUILD_VERSION}/skus/"
