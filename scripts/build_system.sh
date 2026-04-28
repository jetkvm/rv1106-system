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

BUILD_LOG_DIR="${BUILD_LOG_DIR:-${ROOT_DIR}/release-artifacts/logs}"

run_quiet() {
    local label="$1"
    shift

    if [ "${VERBOSE_BUILD:-0}" = "1" ]; then
        "$@"
        return
    fi

    mkdir -p "$BUILD_LOG_DIR"
    local safe_label="${label// /_}"
    local log_file="${BUILD_LOG_DIR}/$(date -u +%Y%m%d%H%M%S)-${safe_label}.log"

    msg_info "  ${label}..."
    msg_info "    log: ${log_file#${ROOT_DIR}/}"
    set +e
    "$@" > "$log_file" 2>&1
    local status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        msg_ok "  OK: ${label}"
        return
    fi

    msg_err "Error: ${label} failed (exit ${status}); log: ${log_file}"
    awk 'BEGIN { IGNORECASE = 1 } /error|failed|permission denied|not found|no such file/ { print }' "$log_file" | tail -n 80 >&2 || true
    msg_err "Last 40 log lines:"
    tail -n 40 "$log_file" >&2 || true
    return "$status"
}

stage_system_variant() {
    local label="$1"
    local sku="$2"
    local stage_dir
    local recovery_name
    local recovery_src

    stage_dir=$(system_variant_dir "$sku")
    mkdir -p "$stage_dir"

    if [ ! -f "$OTA_TAR" ]; then
        msg_err "Error: $OTA_TAR not found after ${label} build"
        exit 1
    fi

    # SDMMC's update.img is a packed Rockchip update image, not a raw disk
    # image, so it can't be written to a microSD card. We ship the dd-able
    # update_sd.img.zip as the recovery artifact instead; update.img is
    # still built for SDMMC but deliberately not staged.
    recovery_name=$(recovery_artifact_for_sku "$sku")
    recovery_src=$(recovery_source_for_sku "$sku")

    if [ ! -f "$recovery_src" ]; then
        msg_err "Error: $recovery_src not found after ${label} build"
        exit 1
    fi

    msg_info "  Staging ${label} system artifacts (${sku})..."
    cp --reflink=auto "$OTA_TAR" "${stage_dir}/${SYSTEM_TAR_NAME}"
    cp --reflink=auto "$recovery_src" "${stage_dir}/${recovery_name}"
    sha256sum "${stage_dir}/${SYSTEM_TAR_NAME}" | awk '{print $1}' > "${stage_dir}/${SYSTEM_TAR_NAME}.sha256"
    sha256sum "${stage_dir}/${recovery_name}" | awk '{print $1}' > "${stage_dir}/${recovery_name}.sha256"
}

prompt_test_system_variant() {
    local label="$1"
    local sku="$2"
    local confirm
    local device_ip="${DEVICE_IP:-192.168.1.77}"
    local device_user="${DEVICE_USER:-root}"
    local test_args=("-r" "$device_ip" "-u" "$device_user")

    if [ "${PROMPT_VARIANT_TESTS:-1}" != "1" ]; then
        return 0
    fi

    echo ""
    read -p "Test ${label} (${sku}) on a device now? [y/N] " confirm
    if [ "$confirm" != "y" ]; then
        msg_warn "Skipping ${label} E2E test"
        return 0
    fi

    if [ -z "${JETKVM_REMOTE_HOST:-}" ]; then
        msg_err "Error: JETKVM_REMOTE_HOST is required to run E2E tests"
        msg_err "Re-run with JETKVM_REMOTE_HOST=<user@host> and DEVICE_IP=${device_ip} if needed"
        exit 1
    fi

    test_args+=("--remote-host" "$JETKVM_REMOTE_HOST")
    if [ -n "${KVM_DIR:-}" ]; then
        test_args+=("--kvm-dir" "$KVM_DIR")
    fi
    if [ -n "${KVM_BRANCH:-}" ]; then
        test_args+=("--kvm-branch" "$KVM_BRANCH")
    fi

    msg_info "  Flashing ${label} build to ${device_user}@${device_ip}..."
    ./scripts/flash_system.sh -r "$device_ip" -u "$device_user"

    msg_info "  Running ${label} E2E tests..."
    ./scripts/run_e2e_tests.sh "${test_args[@]}"
}

build_system_variant() {
    local label="$1"
    local sku="$2"
    local board_config="$3"

    run_quiet "Selecting ${label} board (${sku})" ./build.sh lunch "$board_config"
    run_quiet "Updating JetKVM app binary for ${label} (${sku})" ./update_app.sh "$sku"
    run_quiet "Building ${label} system image" ./build.sh

    stage_system_variant "$label" "$sku"
    prompt_test_system_variant "$label" "$sku"
}

msg_info ">> Building rv1106-system..."
cd "$ROOT_DIR"

msg_info "  Cleaning previous build output..."
sudo rm -rf output/
run_quiet "Cleaning SDK output" ./build.sh clean

rm -rf "$SYSTEM_RELEASE_DIR"

build_system_variant "SDMMC" "$SDMMC_SKU" "$SDMMC_BOARD_CONFIG"

msg_info "  Cleaning build output before EMMC..."
sudo rm -rf output/
run_quiet "Cleaning SDK output before EMMC" ./build.sh clean

build_system_variant "EMMC" "$EMMC_SKU" "$EMMC_BOARD_CONFIG"

msg_ok "OK: Build completed"
