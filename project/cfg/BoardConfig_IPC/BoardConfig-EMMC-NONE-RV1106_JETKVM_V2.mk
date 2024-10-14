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
export RK_UBOOT_DEFCONFIG=rv1106-jetkvm-v2_defconfig

# Uboot defconfig fragment
export RK_UBOOT_DEFCONFIG_FRAGMENT=rk-emmc.config

# Kernel defconfig
export RK_KERNEL_DEFCONFIG=rv1106-jetkvm-v2_defconfig

# # Kernel defconfig fragment
# export RK_KERNEL_DEFCONFIG_FRAGMENT=rv1106-jetkvm-v2.config

# Kernel dts
export RK_KERNEL_DTS=rv1106g-jetkvm-v2.dts

#misc image
export RK_MISC=wipe_all-misc.img

# Config sensor IQ files
# RK_CAMERA_SENSOR_IQFILES format:
#     "iqfile1 iqfile2 iqfile3 ..."
# ./build.sh media and copy <SDK root dir>/output/out/media_out/isp_iqfiles/$RK_CAMERA_SENSOR_IQFILES
export RK_CAMERA_SENSOR_IQFILES=""

# Config sensor lens CAC calibrattion bin file
export RK_CAMERA_SENSOR_CAC_BIN=""

# Config CMA size in environment
export RK_BOOTARGS_CMA_SIZE="48M"

# config partition in environment
# RK_PARTITION_CMD_IN_ENV format:
#     <partdef>[,<partdef>]
#       <partdef> := <size>[@<offset>](part-name)
# Note:
#   If the first partition offset is not 0x0, it must be added. Otherwise, it needn't adding.
#export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot),32M(boot),1G(rootfs),1G(oem),1G(userdata),-(media)"

# export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot_a),256K(uboot_b),256K(misc),32M(boot_a),32M(boot_b),512M(system_a),512M(system_b),256M(oem),-(userdata)"
export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot_a),256K(uboot_b),256K(misc),32M(boot_a),32M(boot_b),512M(system_a),512M(system_b),14152416K(userdata)"
# export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot_a),256K(uboot_b),256K(misc),32M(boot_a),32M(boot_b),512M(system_a),512M(system_b),-(userdata)"

# config partition's filesystem type (squashfs is readonly)
# emmc:    squashfs/ext4
# nand:    squashfs/ubifs
# spi nor: squashfs/jffs2
# RK_PARTITION_FS_TYPE_CFG format:
#     AAAA:/BBBB/CCCC@ext4
#         AAAA ----------> partition name
#         /BBBB/CCCC ----> partition mount point
#         ext4 ----------> partition filesystem type
#export RK_PARTITION_FS_TYPE_CFG=rootfs@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4
# export RK_PARTITION_FS_TYPE_CFG=system_a@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4
export RK_PARTITION_FS_TYPE_CFG=system_a@IGNORE@ext4,userdata@/userdata@ext4

# config filesystem compress (Just for squashfs or ubifs)
# squashfs: lz4/lzo/lzma/xz/gzip, default xz
# ubifs:    lzo/zlib, default lzo
# export RK_SQUASHFS_COMP=xz
# export RK_UBIFS_COMP=lzo

# app config
export RK_APP_TYPE=JETKVM

# enable install app to oem partition
export RK_BUILD_APP_TO_OEM_PARTITION=n

# Enable OTA tool
export RK_ENABLE_OTA=y
# OTA package
export RK_OTA_RESOURCE="uboot.img boot.img system.img"

export RK_ENABLE_ADBD=n

export RK_ENABLE_WIFI=n

export RK_ENABLE_EUDEV=n

export RK_ENABLE_ROCKCHIP_TEST=n

export RK_ENABLE_SAMPLE=n

