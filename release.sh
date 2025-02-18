#!/bin/bash
set -eE
set -o pipefail
set -x

export BUILD_VERSION=$(cat VERSION)

# Check if the version already exists
if rclone lsf r2://jetkvm-update/system/$BUILD_VERSION/ | grep -q .; then
    echo "Error: Version $BUILD_VERSION already exists in the remote storage."
    exit 1
fi

./build.sh lunch BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk
./build.sh
sha256sum output/image/update_ota.tar | awk '{print $1}' > output/image/update_ota.tar.sha256
sha256sum output/image/update.img | awk '{print $1}' > output/image/update.img.sha256

# Check if the version already exists

rclone copyto output/image/update_ota.tar r2://jetkvm-update/system/$BUILD_VERSION/system.tar
rclone copyto output/image/update_ota.tar.sha256 r2://jetkvm-update/system/$BUILD_VERSION/system.tar.sha256
rclone copyto output/image/update.img r2://jetkvm-update/system/$BUILD_VERSION/update.img
rclone copyto output/image/update.img.sha256 r2://jetkvm-update/system/$BUILD_VERSION/update.img.sha256
