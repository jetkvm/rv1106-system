#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Function to display help message
show_help() {
    echo "Usage: $0 <feature>"
    echo
    echo
    echo "Features:"
    echo "  kernel-menuconfig   Configure kernel options"
    echo "  busybox-menuconfig  Configure busybox options"
    exit 0
}

# Function to configure kernel options
kernel_menuconfig() {
    # get current directory of the file
    local config_name="rv1106-jetkvm-v2_defconfig"
    local current_dir=$(dirname "$(readlink -f "$0")")

    set -x
    set -e
    pushd "${current_dir}/sysdrv/source/kernel" > /dev/null
    cp "./arch/arm/configs/${config_name}" .config
    make ARCH=arm menuconfig
    make ARCH=arm savedefconfig
    cp defconfig "${current_dir}/sysdrv/source/kernel/arch/arm/configs/${config_name}"
    make ARCH=arm mrproper
    popd > /dev/null
    set +x
    set +e

    # check if git is installed and the current directory is a git repository
    # if yes, show the diff of the staged files
    if command -v git &> /dev/null && git rev-parse --is-inside-work-tree &> /dev/null; then
        echo "Changes made to the kernel configuration:"
        git diff "${current_dir}/sysdrv/source/kernel/arch/arm/configs/${config_name}"
    else
        echo "Git is not installed or not in a git repository."
    fi
}

busybox_menuconfig() {
    local current_dir=$(dirname "$(readlink -f "$0")")
    export RK_PROJECT_TOOLCHAIN_CROSS=arm-rockchip830-linux-uclibcgnueabihf
    export PATH="${current_dir}/tools/linux/toolchain/${RK_PROJECT_TOOLCHAIN_CROSS}/bin":$PATH
    make -C "${current_dir}/sysdrv" busybox busybox_menuconfig
    cp "${current_dir}/sysdrv/source/busybox/objs_config_normal/.config" "${current_dir}/sysdrv/tools/board/busybox/config_normal"
    # check if git is installed and the current directory is a git repository
    # if yes, show the diff of the staged files
    if command -v git &> /dev/null && git rev-parse --is-inside-work-tree &> /dev/null; then
        echo "Changes made to the busybox configuration:"
        git diff "${current_dir}/sysdrv/tools/board/busybox/config_normal"
    else
        echo "Git is not installed or not in a git repository."
    fi
}

# If there's no argument, show the help message
if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        kernel-menuconfig)
            shift
            kernel_menuconfig
            exit 0
            ;;
        busybox-menuconfig)
            shift
            busybox_menuconfig
            exit 0
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done