#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

BUILD_VERSION=""
PRERELEASE=false

source "${SCRIPT_DIR}/common.sh"

show_help() {
    echo "Usage: $0 --version <version> [--prerelease]"
    echo
    echo "Options:"
    echo "  --version <version>   Release version (e.g., 0.2.7)"
    echo "  --prerelease          Mark release as prerelease"
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

command -v gh >/dev/null 2>&1 || { msg_err "Error: gh CLI not installed"; exit 1; }
gh auth status >/dev/null 2>&1 || { msg_err "Error: gh CLI not authenticated. Run 'gh auth login'"; exit 1; }

cd "$ROOT_DIR"

ota_tar="$OTA_TAR"
full_img="$FULL_IMG"
buildkit="buildkit.tar.zst"
buildkit_name="kvm-native-buildkit.tar.zst"

if [ ! -f "$buildkit" ]; then
    msg_info ">> buildkit.tar.zst not found, creating..."
    ./make_buildkit.sh
fi

for file in "$ota_tar" "$full_img" "$buildkit"; do
    if [ ! -f "$file" ]; then
        msg_err "Error: Required file not found: $file"
        exit 1
    fi
done

if gh release view "release/v${BUILD_VERSION}" --repo jetkvm/rv1106-system >/dev/null 2>&1; then
    msg_err "Error: GitHub release release/v${BUILD_VERSION} already exists"
    exit 1
fi

if git rev-parse "release/v${BUILD_VERSION}" >/dev/null 2>&1; then
    msg_err "Error: Git tag release/v${BUILD_VERSION} already exists"
    exit 1
fi

ota_size=$(du -h "$ota_tar" | cut -f1)
img_size=$(du -h "$full_img" | cut -f1)
kit_size=$(du -h "$buildkit" | cut -f1)

echo ""
msg_info "═══════════════════════════════════════════════════════"
msg_info "  GitHub Release"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Version: ${BUILD_VERSION}"
msg_info "  Tag:     release/v${BUILD_VERSION}"
msg_info "  Branch:  $(git rev-parse --abbrev-ref HEAD)"
msg_info "  Commit:  $(git rev-parse --short HEAD)"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Files to upload:"
msg_info "    - update_ota.tar    (${ota_size})"
msg_info "    - update.img        (${img_size})"
msg_info "    - ${buildkit_name}  (${kit_size})"
msg_info "═══════════════════════════════════════════════════════"
echo ""
read -p "These are the inputs for GitHub. Are you sure? [y/N] " confirm
if [ "$confirm" != "y" ]; then
    msg_warn "GitHub release cancelled."
    exit 1
fi

msg_info ">> Creating git tag release/v${BUILD_VERSION}..."
git tag "release/v${BUILD_VERSION}"
git push origin "release/v${BUILD_VERSION}"

msg_info ">> Creating GitHub release..."
release_args=( "release/v${BUILD_VERSION}" "$ota_tar" "$full_img" "${buildkit}#${buildkit_name}" "--title" "${BUILD_VERSION}" "--generate-notes" )
if [ "$PRERELEASE" = true ]; then
    release_args+=( "--prerelease" )
fi
gh release create "${release_args[@]}"

msg_ok "OK: GitHub release created: release/v${BUILD_VERSION}"
