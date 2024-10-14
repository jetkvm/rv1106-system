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
export RK_UBOOT_DEFCONFIG=rv1106-emmc-tb-nofastae_defconfig

# Uboot defconfig fragment
export RK_UBOOT_DEFCONFIG_FRAGMENT="rk-emmc.config"

# Uboot Loader ini overlay
export RK_UBOOT_RKBIN_INI_OVERLAY=RKBOOT/RV1106MINIALL_EMMC_TB_NOMCU.ini

# Kernel defconfig
export RK_KERNEL_DEFCONFIG=rv1106_defconfig

# Kernel defconfig fragment
export RK_KERNEL_DEFCONFIG_FRAGMENT="rv1106-sdiowifi.config rv1106-tb-nofastae.config"

# Kernel dts
export RK_KERNEL_DTS=rv1106g-evb1-v11-fastboot-emmc.dts

#misc image
export RK_MISC=wipe_all-misc.img

# Config sensor IQ files
# RK_CAMERA_SENSOR_IQFILES format:
#     "iqfile1 iqfile2 iqfile3 ..."
# ./build.sh media and copy <SDK root dir>/output/out/media_out/isp_iqfiles/$RK_CAMERA_SENSOR_IQFILES
export RK_CAMERA_SENSOR_IQFILES="sc4336_OT01_40IRC_F16.bin sc3336_CMK-OT2119-PC1_30IRC-F16.bin sc530ai_CMK-OT2115-PC1_30IRC-F16.bin"

# Config sensor lens CAC calibrattion bin file
export RK_CAMERA_SENSOR_CAC_BIN="CAC_sc4336_OT01_40IRC_F16 CAC_sc530ai_CMK-OT2115-PC1_30IRC-F16"

# Config CMA size in environment
export RK_BOOTARGS_CMA_SIZE="48M"

# config partition in environment
# RK_PARTITION_CMD_IN_ENV format:
#     <partdef>[,<partdef>]
#       <partdef> := <size>[@<offset>](part-name)
# Note:
#   If the first partition offset is not 0x0, it must be added. Otherwise, it needn't adding.
export RK_PARTITION_CMD_IN_ENV="32K(env),128K@32K(idblock),192K(uboot),15M(boot),2G(userdata),-(reserve)"

# config partition's filesystem type (squashfs is readonly)
# emmc:    squashfs/ext4
# nand:    squashfs/ubifs
# spi nor: squashfs/jffs2
# RK_PARTITION_FS_TYPE_CFG format:
#     AAAA:/BBBB/CCCC@ext4
#         AAAA ----------> partition name
#         /BBBB/CCCC ----> partition mount point
#         ext4 ----------> partition filesystem type
export RK_PARTITION_FS_TYPE_CFG=boot@IGNORE@erofs,userdata@/userdata@ext4

# config filesystem compress (Just for squashfs or ubifs)
# squashfs: lz4/lzo/lzma/xz/gzip, default xz
# ubifs:    lzo/zlib, default lzo
# export RK_SQUASHFS_COMP=xz
# export RK_UBIFS_COMP=lzo

# app config
export RK_APP_TYPE=RK_WIFI_APP

# specify post.sh for delete/overlay files
export RK_POST_BUILD_SCRIPT=rv1106-nofastae-simple-post.sh

# enable fastboot
export RK_ENABLE_FASTBOOT=y

# enable wifi
export RK_ENABLE_WIFI=y
export RK_ENABLE_WIFI_CHIP=RTL8189FS
