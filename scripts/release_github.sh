#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

BUILD_VERSION=""
PRERELEASE=false
DRY_RUN=false

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 --version <version> [--prerelease] [--dry-run]"
    echo
    echo "Options:"
    echo "  --version <version>   Release version (e.g., 0.2.7)"
    echo "  --prerelease          Mark release as prerelease"
    echo "  --dry-run             Validate and print release inputs without tagging or uploading"
    echo "  --help                Show this help message"
    echo
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            BUILD_VERSION="$2"
            shift 2
            ;;
        --prerelease)
            PRERELEASE=true
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

if [ "$DRY_RUN" != true ]; then
    command -v gh >/dev/null 2>&1 || { msg_err "Error: gh CLI not installed"; exit 1; }
    gh auth status >/dev/null 2>&1 || { msg_err "Error: gh CLI not authenticated. Run 'gh auth login'"; exit 1; }
fi

cd "$ROOT_DIR"

buildkit="buildkit.tar.zst"
emmc_dir=$(system_variant_dir "$EMMC_SKU")
sdmmc_dir=$(system_variant_dir "$SDMMC_SKU")
github_assets=(
    "${emmc_dir}/${SYSTEM_TAR_NAME}#${OTA_TAR_NAME}"
    "${emmc_dir}/${FULL_IMG_NAME}#${FULL_IMG_NAME}"
    "${sdmmc_dir}/${SYSTEM_TAR_NAME}#update_ota-sdmmc.tar"
    "${sdmmc_dir}/${FULL_IMG_NAME}#update-sdmmc.img"
    "${sdmmc_dir}/${SD_IMG_ZIP_NAME}#${SD_IMG_ZIP_NAME}"
    "${buildkit}#kvm-native-buildkit.tar.zst"
)

if [ ! -f "$buildkit" ]; then
    msg_info ">> buildkit.tar.zst not found, creating..."
    ./make_buildkit.sh
fi

for asset in "${github_assets[@]}"; do
    asset_path="${asset%%#*}"
    if [ ! -f "$asset_path" ]; then
        msg_err "Error: Required file not found: $asset_path"
        msg_err "Run 'make build' first."
        exit 1
    fi
done

if [ "$DRY_RUN" != true ]; then
    if gh release view "release/v${BUILD_VERSION}" --repo jetkvm/rv1106-system >/dev/null 2>&1; then
        msg_err "Error: GitHub release release/v${BUILD_VERSION} already exists"
        exit 1
    fi

    if git rev-parse "release/v${BUILD_VERSION}" >/dev/null 2>&1; then
        msg_err "Error: Git tag release/v${BUILD_VERSION} already exists"
        exit 1
    fi
fi

echo ""
msg_info "═══════════════════════════════════════════════════════"
if [ "$DRY_RUN" = true ]; then
    msg_info "  GitHub Release Dry Run"
else
    msg_info "  GitHub Release"
fi
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Version: ${BUILD_VERSION}"
msg_info "  Tag:     release/v${BUILD_VERSION}"
msg_info "  Branch:  $(git rev-parse --abbrev-ref HEAD)"
msg_info "  Commit:  $(git rev-parse --short HEAD)"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Files to upload:"
for asset in "${github_assets[@]}"; do
    asset_path="${asset%%#*}"
    asset_name="${asset##*#}"
    asset_size=$(du -h "$asset_path" | cut -f1)
    msg_info "    - ${asset_name} (${asset_size})"
done
msg_info "═══════════════════════════════════════════════════════"
echo ""

if [ "$DRY_RUN" = true ]; then
    msg_ok "OK: GitHub dry run complete; skipped tag push and release creation"
    exit 0
fi

read -p "These are the inputs for GitHub. Are you sure? [y/N] " confirm
if [ "$confirm" != "y" ]; then
    msg_warn "GitHub release cancelled."
    exit 1
fi

msg_info ">> Creating git tag release/v${BUILD_VERSION}..."
git tag "release/v${BUILD_VERSION}"
git push origin "release/v${BUILD_VERSION}"

msg_info ">> Creating GitHub release..."
release_args=(
    "release/v${BUILD_VERSION}"
    "${github_assets[@]}"
    "--title" "${BUILD_VERSION}"
    "--generate-notes"
)
if [ "$PRERELEASE" = true ]; then
    release_args+=( "--prerelease" )
fi
gh release create "${release_args[@]}"

msg_ok "OK: GitHub release created: release/v${BUILD_VERSION}"
