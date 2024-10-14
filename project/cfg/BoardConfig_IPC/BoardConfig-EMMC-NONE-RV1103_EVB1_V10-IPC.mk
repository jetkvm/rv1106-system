#!/bin/bash

# Target arch
export RK_ARCH=arm

# Target CHIP
export RK_CHIP=rv1106

# Target Toolchain Cross Compile
export RK_TOOLCHAIN_CROSS=arm-rockchip830-linux-uclibcgnueabihf

# Target boot medium: emmc/spi_nor/spi_nand
export RK_BOOT_MEDIUM=emmc

# Uboot defconfig
export RK_UBOOT_DEFCONFIG=rv1106_defconfig

# Uboot defconfig fragment
export RK_UBOOT_DEFCONFIG_FRAGMENT=rk-emmc.config

# Kernel defconfig
export RK_KERNEL_DEFCONFIG=rv1106_defconfig

# Kernel defconfig fragment
export RK_KERNEL_DEFCONFIG_FRAGMENT=rv1106-evb.config

# Kernel dts
export RK_KERNEL_DTS=rv1103g-evb-v10.dts

#misc image
export RK_MISC=wipe_all-misc.img

# Config sensor IQ files
# RK_CAMERA_SENSOR_IQFILES format:
#     "iqfile1 iqfile2 iqfile3 ..."
# ./build.sh media and copy <SDK root dir>/output/out/media_out/isp_iqfiles/$RK_CAMERA_SENSOR_IQFILES
export RK_CAMERA_SENSOR_IQFILES="sc4336_OT01_40IRC_F16.bin sc3336_CMK-OT2119-PC1_30IRC-F16.bin"

# Config sensor lens CAC calibrattion bin files
export RK_CAMERA_SENSOR_CAC_BIN="CAC_sc4336_OT01_40IRC_F16"

# Config CMA size in environment
export RK_BOOTARGS_CMA_SIZE="24M"

# config partition in environment
# RK_PARTITION_CMD_IN_ENV format:
#     <partdef>[,<partdef>]
#       <partdef> := <size>[@<offset>](part-name)
# Note:
#   If the first partition offset is not 0x0, it must be added. Otherwise, it needn't adding.
export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot),32M(boot),2G(rootfs),1G(oem),2G(userdata),-(media)"

# config partition's filesystem type (squashfs is readonly)
# emmc:    squashfs/ext4
# nand:    squashfs/ubifs
# spi nor: squashfs/jffs2
# RK_PARTITION_FS_TYPE_CFG format:
#     AAAA:/BBBB/CCCC@ext4
#         AAAA ----------> partition name
#         /BBBB/CCCC ----> partition mount point
#         ext4 ----------> partition filesystem type
export RK_PARTITION_FS_TYPE_CFG=rootfs@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4

# config filesystem compress (Just for squashfs or ubifs)
# squashfs: lz4/lzo/lzma/xz/gzip, default xz
# ubifs:    lzo/zlib, default lzo
# export RK_SQUASHFS_COMP=xz
# export RK_UBIFS_COMP=lzo

# app config
export RK_APP_TYPE=RKIPC_RV1103

# specify post.sh for delete/overlay files
export RK_PRE_BUILD_OEM_SCRIPT=rv1103-emmc-post.sh

# build ipc web backend
export RK_APP_IPCWEB_BACKEND=y

# enable install app to oem partition
export RK_BUILD_APP_TO_OEM_PARTITION=y

# enable rockchip test
export RK_ENABLE_ROCKCHIP_TEST=y

# config AUDIO model
export RK_AUDIO_MODEL=NONE

# config AI-ISP model
export RK_AIISP_MODEL=NONE

# config NPU model
export RK_NPU_MODEL="object_detection_pfp_512x288.data"
