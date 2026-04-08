#!/usr/bin/env bash
set -e
set -o pipefail

SCRIPT_DIR=$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")
ROOT_DIR=$(realpath "${SCRIPT_DIR}/..")

TEST_REF=""
WORKTREE_PARENT=""
WORKTREE_DIR=""

source "${SCRIPT_DIR}/common.sh"

BUILD_BOARD_CONFIG="${BUILD_BOARD_CONFIG:-BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk}"

show_help() {
    echo "Usage: $0 --ref <commit|tag|branch>"
    echo
    echo "Options:"
    echo "  --ref <git-ref>   Source git ref to build, flash, and test"
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
        --ref)
            if [ -z "${2:-}" ] || [[ "$2" == -* ]]; then
                msg_err "Error: --ref requires a value"
                exit 1
            fi
            TEST_REF="$2"
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

if [ -z "$TEST_REF" ]; then
    msg_err "Error: --ref is required"
    show_help
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

resolved_commit=$(git rev-parse --verify "${TEST_REF}^{commit}")
WORKTREE_PARENT=$(mktemp -d)
WORKTREE_DIR="${WORKTREE_PARENT}/repo"

msg_info ">> Preparing temporary worktree from ${TEST_REF}..."
git worktree add --detach "$WORKTREE_DIR" "$resolved_commit" >/dev/null

export RELEASE_SOURCE_REF="$TEST_REF"
export RELEASE_SOURCE_COMMIT="$resolved_commit"

echo ""
msg_info "═══════════════════════════════════════════════════════"
msg_info "  TEST From Ref"
msg_info "═══════════════════════════════════════════════════════"
print_release_source
msg_info "  Worktree: ${WORKTREE_DIR}"
if [ -n "${BUILD_VERSION:-}" ]; then
    msg_info "  Build version override: ${BUILD_VERSION}"
fi
msg_info "  Time:    $(date -u +%FT%T%z)"
msg_info "═══════════════════════════════════════════════════════"
echo ""

make_args=()

for var_name in DEVICE_IP DEVICE_USER JETKVM_REMOTE_HOST KVM_DIR KVM_BRANCH KVM_REPO SKIP_BUILD BUILD_VERSION; do
    if [ -n "${!var_name:-}" ]; then
        make_args+=( "${var_name}=${!var_name}" )
    fi
done

msg_info ">> Running build, flash, and test in temporary worktree..."
(
    cd "$WORKTREE_DIR"
    export RELEASE_SOURCE_REF RELEASE_SOURCE_COMMIT
    if [ -n "${BUILD_VERSION:-}" ]; then
        export BUILD_VERSION
    fi
    msg_info "  Selecting board config ${BUILD_BOARD_CONFIG} in temporary worktree..."
    ./build.sh lunch "${BUILD_BOARD_CONFIG}"
    make RELEASE_REF= "${make_args[@]}" test
)

echo ""
msg_ok "OK: Test completed for ${TEST_REF}"
