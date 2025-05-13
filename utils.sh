#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Function to display help message
show_help() {
    echo "Usage: $0 <feature>"
    echo
    echo
    echo "Features:"
    echo "  kernel-menuconfig   Configure kernel options"
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
    popd
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

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        kernel-menuconfig)
            shift
            kernel_menuconfig
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