#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

RELEASE_KIND=""
RELEASE_REF=""
WORKTREE_PARENT=""
WORKTREE_DIR=""

source "${SCRIPT_DIR}/common.sh"

BUILD_BOARD_CONFIG="${BUILD_BOARD_CONFIG:-BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk}"

show_help() {
    echo "Usage: $0 --kind <dev|prod> --ref <commit|tag|branch>"
    echo
    echo "Options:"
    echo "  --kind <kind>     Release kind: dev or prod"
    echo "  --ref <git-ref>   Source git ref to build, test, and publish"
    echo "  --help            Show this help message"
    echo
}

cleanup() {
    local exit_code=$?

    if [ -n "$WORKTREE_DIR" ] && [ -d "$WORKTREE_DIR" ]; then
        msg_info ">> Cleaning up temporary worktree..."
        git -C "$ROOT_DIR" worktree remove --force "$WORKTREE_DIR" >/dev/null 2>&1 || rm -rf "$WORKTREE_DIR"
    fi

    if [ -n "$WORKTREE_PARENT" ] && [ -d "$WORKTREE_PARENT" ]; then
        rm -rf "$WORKTREE_PARENT"
    fi

    exit "$exit_code"
}
trap cleanup EXIT

while [[ $# -gt 0 ]]; do
    case $1 in
        --kind)
            if [ -z "${2:-}" ] || [[ "$2" == -* ]]; then
                msg_err "Error: --kind requires a value (dev or prod)"
                exit 1
            fi
            RELEASE_KIND="$2"
            shift 2
            ;;
        --ref)
            if [ -z "${2:-}" ] || [[ "$2" == -* ]]; then
                msg_err "Error: --ref requires a value"
                exit 1
            fi
            RELEASE_REF="$2"
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

if [ -z "$RELEASE_KIND" ] || [ -z "$RELEASE_REF" ]; then
    msg_err "Error: --kind and --ref are required"
    show_help
    exit 1
fi

if [ "$RELEASE_KIND" != "dev" ] && [ "$RELEASE_KIND" != "prod" ]; then
    msg_err "Error: --kind must be 'dev' or 'prod'"
    exit 1
fi

cd "$ROOT_DIR"

if [ -n "$(git status --porcelain)" ]; then
    if [ "${ALLOW_DIRTY:-}" = "1" ]; then
        msg_warn "WARNING: Working tree is dirty, continuing because ALLOW_DIRTY=1"
    else
        msg_err "Error: Working tree is dirty. Commit or stash changes, or rerun with ALLOW_DIRTY=1."
        exit 1
    fi
fi

command -v gh >/dev/null 2>&1 || { msg_err "Error: gh CLI not installed"; exit 1; }
gh auth status >/dev/null 2>&1 || { msg_err "Error: gh CLI not authenticated. Run 'gh auth login'"; exit 1; }
command -v rclone >/dev/null 2>&1 || { msg_err "Error: rclone not installed"; exit 1; }

resolved_commit=$(git rev-parse --verify "${RELEASE_REF}^{commit}")
WORKTREE_PARENT=$(mktemp -d)
WORKTREE_DIR="${WORKTREE_PARENT}/repo"

msg_info ">> Preparing temporary worktree from ${RELEASE_REF}..."
git worktree add --detach "$WORKTREE_DIR" "$resolved_commit" >/dev/null

base_version=$(cat "${WORKTREE_DIR}/VERSION" 2>/dev/null || echo "0.0.0")
if [ "$RELEASE_KIND" = "dev" ]; then
    build_version="${base_version}-dev$(date -u +%Y%m%d%H%M)"
    release_label="DEV Release (Pre-release)"
    release_done="OK: Dev release complete: release/v${build_version}"
else
    build_version="${base_version}"
    release_label="PRODUCTION Release"
    release_done="OK: Production release complete: release/v${build_version}"
fi

export RELEASE_SOURCE_REF="$RELEASE_REF"
export RELEASE_SOURCE_COMMIT="$resolved_commit"

if [ "$RELEASE_KIND" = "prod" ]; then
    release_tags=$(gh release list --repo jetkvm/rv1106-system --limit 10 --json tagName --jq -r '.[].tagName')
    latest_dev=$(printf '%s\n' "$release_tags" | grep -m 1 "^release/v${build_version}-dev" || true)

    if [ -z "$latest_dev" ]; then
        echo ""
        msg_warn "WARNING: No dev release found for ${build_version}"
        echo ""
        read -p "Release production from ${RELEASE_REF} without prior dev release? [y/N] " confirm
        if [ "$confirm" != "y" ]; then
            exit 1
        fi
    else
        msg_ok "OK: Found prior dev release: ${latest_dev}"
    fi
fi

echo ""
msg_info "═══════════════════════════════════════════════════════"
msg_info "  ${release_label}"
msg_info "═══════════════════════════════════════════════════════"
msg_info "  Version: ${build_version}"
msg_info "  Tag:     release/v${build_version}"
print_release_source
msg_info "  Worktree: ${WORKTREE_DIR}"
msg_info "  Time:    $(date -u +%FT%T%z)"
msg_info "═══════════════════════════════════════════════════════"
echo ""

if [ "$RELEASE_KIND" = "dev" ]; then
    read -p "Proceed with DEV release from ${RELEASE_REF}? [y/N] " confirm
else
    read -p "Proceed with PRODUCTION release from ${RELEASE_REF}? [y/N] " confirm
fi
if [ "$confirm" != "y" ]; then
    exit 1
fi

make_args=( "BUILD_VERSION=${build_version}" )

for var_name in DEVICE_IP DEVICE_USER JETKVM_REMOTE_HOST KVM_DIR KVM_BRANCH KVM_REPO SKIP_BUILD R2_PATH; do
    if [ -n "${!var_name:-}" ]; then
        make_args+=( "${var_name}=${!var_name}" )
    fi
done

msg_info ">> Running build, flash, and test in temporary worktree..."
(
    cd "$WORKTREE_DIR"
    export RELEASE_SOURCE_REF RELEASE_SOURCE_COMMIT BUILD_VERSION="$build_version"
    msg_info "  Selecting board config ${BUILD_BOARD_CONFIG} in temporary worktree..."
    ./build.sh lunch "${BUILD_BOARD_CONFIG}"
    make RELEASE_REF= "${make_args[@]}" test

    echo ""
    msg_info "═══════════════════════════════════════════════════════"
    msg_info "  Final Check: R2 Upload"
    msg_info "═══════════════════════════════════════════════════════"
    msg_info "  Version: ${build_version}"
    msg_info "  Destination: ${R2_PATH}/${build_version}/"
    print_release_source
    msg_info "═══════════════════════════════════════════════════════"
    echo ""
    read -p "Proceed to R2 upload for ${build_version}? [y/N] " confirm
    if [ "$confirm" != "y" ]; then
        msg_warn "R2 upload cancelled before publish."
        exit 1
    fi

    ./scripts/release_r2.sh --version "$build_version"

    github_args=( "--version" "$build_version" "--target-commit" "$resolved_commit" "--source-ref" "$RELEASE_REF" )
    if [ "$RELEASE_KIND" = "dev" ]; then
        github_args+=( "--prerelease" )
    fi

    echo ""
    msg_info "═══════════════════════════════════════════════════════"
    msg_info "  Final Check: GitHub Release"
    msg_info "═══════════════════════════════════════════════════════"
    msg_info "  Version: ${build_version}"
    msg_info "  Tag:     release/v${build_version}"
    if [ "$RELEASE_KIND" = "dev" ]; then
        msg_info "  Type:    prerelease"
    else
        msg_info "  Type:    production release"
    fi
    print_release_source
    msg_info "═══════════════════════════════════════════════════════"
    echo ""
    read -p "Proceed to GitHub tag/release for ${build_version}? [y/N] " confirm
    if [ "$confirm" != "y" ]; then
        msg_warn "GitHub release cancelled before publish."
        exit 1
    fi

    ./scripts/release_github.sh "${github_args[@]}"
)

echo ""
msg_ok "${release_done}"
