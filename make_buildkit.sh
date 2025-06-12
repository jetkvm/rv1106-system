#!/bin/bash

TMP_PATH=$(mktemp -d)
echo "TMP_PATH: $TMP_PATH"
clean_up () {
    ARG=$?
    echo "> Cleaning up, exit code: $ARG"
    rm -rf $TMP_PATH
    exit $ARG
} 
trap clean_up EXIT
set -e

RK_PATH=$(pwd)
RK_ARCH=arm-rockchip830-linux-uclibcgnueabihf
RK_BOARD_PATH=${RK_PATH}/sysdrv/tools/board/

echo "Copying toolchain"
cp -r $RK_PATH/tools/linux/toolchain/$RK_ARCH/* ${TMP_PATH}/

TARGET_SYSROOT_PATH="${TMP_PATH}/${RK_ARCH}/sysroot"
TARGET_INCLUDE_PATH="${TARGET_SYSROOT_PATH}/usr/include"
TARGET_LIB_PATH="${TARGET_SYSROOT_PATH}/usr/lib"

echo "Copying Rockchip Media files"
cp -r $RK_PATH/media/out/include/* ${TARGET_INCLUDE_PATH}/
cp -r $RK_PATH/media/out/lib/* ${TARGET_LIB_PATH}/

echo "Copying dependencies"
function copy_dep() {
    local PKG_NAME=$1
    local PKG_PATH=${RK_BOARD_PATH}/${PKG_NAME}
    echo "Copying ${PKG_NAME} to ${TARGET_LIB_PATH}"   
    cp -r ${PKG_PATH}/out/lib/* ${TARGET_LIB_PATH}/
    echo "Copying headers"
    cp -r ${PKG_PATH}/out/include/* ${TARGET_INCLUDE_PATH}/
}

copy_dep toolkits/openssl
copy_dep toolkits/zlib

cp $RK_PATH/rv1106-jetkvm-v2.cmake ${TMP_PATH}/

tar  "-I zstd -10 -T0 --long=31" -cvf \
    buildkit.tar.zst -C ${TMP_PATH} .

echo "Done"