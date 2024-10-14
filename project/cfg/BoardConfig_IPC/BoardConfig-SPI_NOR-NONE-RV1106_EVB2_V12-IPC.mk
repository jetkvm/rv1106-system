#!/bin/bash

# Target arch
export RK_ARCH=arm

# Target CHIP
export RK_CHIP=rv1106

# Target Toolchain Cross Compile
export RK_TOOLCHAIN_CROSS=arm-rockchip830-linux-uclibcgnueabihf

# Target boot medium: emmc/spi_nor/spi_nand
export RK_BOOT_MEDIUM=spi_nor

# Uboot defconfig
export RK_UBOOT_DEFCONFIG=rv1106-spi-nor_defconfig

# Uboot defconfig fragment
export RK_UBOOT_DEFCONFIG_FRAGMENT="rk-sfc.config"

# Uboot Loader ini overlay
#export RK_UBOOT_RKBIN_INI_OVERLAY=RKBOOT/RV1106MINIALL_TB_SC3336_SC3338.ini
# Kernel defconfig
export RK_KERNEL_DEFCONFIG=rv1106_defconfig

# Kernel defconfig fragment
export RK_KERNEL_DEFCONFIG_FRAGMENT="rv1106-ipc.config rv1106-pm.config rv1106-wakeup.config"

# Kernel dts
export RK_KERNEL_DTS=rv1106g-evb2-v12-wakeup.dts

# Config sensor IQ files
# RK_CAMERA_SENSOR_IQFILES format:
#     "iqfile1 iqfile2 iqfile3 ..."
# ./build.sh media and copy <SDK root dir>/output/out/media_out/isp_iqfiles/$RK_CAMERA_SENSOR_IQFILES
export RK_CAMERA_SENSOR_IQFILES="./ainr/sc200ai_CMK-OT2115-PC1_30IRC-F16.bin"

# Config CMA size in environment
export RK_BOOTARGS_CMA_SIZE="40M"

# config partition in environment
# RK_PARTITION_CMD_IN_ENV format:
#     <partdef>[,<partdef>]
#       <partdef> := <size>[@<offset>](part-name)
# Note:
#   If the first partition offset is not 0x0, it must be added. Otherwise, it needn't adding.
export RK_PARTITION_CMD_IN_ENV="64K(env),256K@64K(idblock),512K(uboot),2M(boot),1792K(rootfs),11M(oem),-(userdata)"

# config partition's filesystem type (squashfs is readonly)
# emmc:    squashfs/ext4
# nand:    squashfs/ubifs
# spi nor: squashfs/jffs2
# RK_PARTITION_FS_TYPE_CFG format:
#     AAAA:/BBBB/CCCC@ext4
#         AAAA ----------> partition name
#         /BBBB/CCCC ----> partition mount point
#         ext4 ----------> partition filesystem type
export RK_PARTITION_FS_TYPE_CFG=rootfs@IGNORE@squashfs,oem@/oem@squashfs,userdata@/userdata@jffs2

# config filesystem compress (Just for squashfs or ubifs)
# squashfs: lz4/lzo/lzma/xz/gzip, default xz
# ubifs:    lzo/zlib, default lzo
export RK_SQUASHFS_COMP=xz

# app config
export RK_APP_TYPE=RK_SAMPLE_AOV
export RK_ENABLE_AOV=y

# enable rockchip test
export RK_ENABLE_ROCKCHIP_TEST=n

# disable adb
export RK_ENABLE_ADBD=y
export CONFIG_LIBV4L=y
# disable gdb
export RK_ENABLE_GDB=n

# disable udev
export RK_ENABLE_EUDEV=n
# build ipc web backend
export RK_APP_IPCWEB_BACKEND=n

# enable install app to oem partition
export RK_BUILD_APP_TO_OEM_PARTITION=y

# enable build wifi
#export RK_ENABLE_WIFI=y
#export RK_ENABLE_WIFI_CHIP="HI3861L"

export RK_PRE_BUILD_OEM_SCRIPT=rv1106-spi_nor-post.sh

# config AUDIO model
export RK_AUDIO_MODEL=NONE

# config AI-ISP model
export RK_AIISP_MODEL=rkmodel_1080_1920.aiisp

# config NPU model
export RK_NPU_MODEL="object_detection_pfp_896x512.data"
