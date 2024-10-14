#!/bin/bash

PARTITION_MOUNT_SCRIPT=_S20linkmount
cd $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/bin/
shopt -s extglob
rm !(simple_vi_bind_venc_rtsp|rk_mpi_uvc|usb_config.sh)
shopt -u extglob
cd -
mv $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/librkaiq.so $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib
mv $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/librockit.so $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib
mv $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/lib*mpp* $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib
mv $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/librga.so $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/*.so*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/*.md
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/flash*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/services
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/protocols
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/lib/libstdc++.so.6.0.25-gdb.py
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/lib/libitm.so*
mv $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/S20linkmount $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/${PARTITION_MOUNT_SCRIPT}
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/S21appinit*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/rkipc
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/iqfiles/*.json
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/iqfiles/*.bin
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/hpmcu_wrap.bin
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/*.data

rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_cli
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_cli_rtk
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_supplicant
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_supplicant_nl80211_rtk
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/wpa_supplicant_rtk
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/wpa_supplicant.conf
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/libwpa_client.so
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/libdrm.so*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/bin/modetest
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib/libwpa_client.so
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/lib/libintl.so*
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/iperf
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/hostapd
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/dnsmasq

rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/insmod_ko.sh
# delete nouse ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/gcm.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/ccm.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/sha256_generic.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/libaes.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/libsha256.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/gf128mul.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/cmac.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/rve.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/libarc4.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/aes_generic.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/ctr.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/mac80211.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/atmb_iot_supplicant_demo
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/rk_vendor_storage.ko
rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/ko/mtd_vendor_storage.ko
cp $RK_PROJECT_PATH_MEDIA/isp_iqfiles/$RK_CAMERA_SENSOR_IQFILES $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/iqfiles/

cat > $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/rcS <<EOF
#!/bin/sh
export PATH=\$PATH:/oem/usr/ko/
echo "start service" > /dev/kmsg
/oem/usr/bin/simple_vi_bind_venc_rtsp &

sh /etc/init.d/${PARTITION_MOUNT_SCRIPT} linkdev &

function run_delay_task(){
	cd /oem/usr/ko

	test ! -f jbd2.ko   || insmod jbd2.ko
	test ! -f crc16.ko  || insmod crc16.ko
	test ! -f mbcache.ko|| insmod mbcache.ko
	test ! -f ext4.ko   || insmod ext4.ko
	test ! -f dw_mmc.ko          || insmod dw_mmc.ko
	test ! -f dw_mmc-pltfm.ko    || insmod dw_mmc-pltfm.ko
	test ! -f dw_mmc-rockchip.ko || insmod dw_mmc-rockchip.ko
	test ! -f mmc_block.ko       ||insmod mmc_block.ko

	test ! -f mtd_blkdevs.ko || insmod mtd_blkdevs.ko
	test ! -f mtdblock.ko || insmod mtdblock.ko
	test ! -f spi-nor.ko || insmod spi-nor.ko
	test ! -f dw_mmc.ko          || insmod dw_mmc.ko
	test ! -f dw_mmc-pltfm.ko    || insmod dw_mmc-pltfm.ko
	test ! -f dw_mmc-rockchip.ko || insmod dw_mmc-rockchip.ko
	sh /etc/init.d/${PARTITION_MOUNT_SCRIPT} start &

	test ! -f snd-soc-simple-card-utils.ko || insmod snd-soc-simple-card-utils.ko
	test ! -f snd-soc-simple-card.ko || insmod snd-soc-simple-card.ko
	test ! -f snd-soc-rockchip-i2s-tdm.ko || insmod snd-soc-rockchip-i2s-tdm.ko
	test ! -f snd-soc-rv1106.ko || insmod snd-soc-rv1106.ko

	test ! -f mmc_block.ko || insmod mmc_block.ko
	test ! -f nls_cp936.ko || insmod nls_cp936.ko
	test ! -f fat.ko       || insmod fat.ko
	test ! -f vfat.ko      || insmod vfat.ko

	test ! -f /etc/init.d/S80idb_bootconfig   || /etc/init.d/S80idb_bootconfig start
	test ! -f /oem/usr/ko/insmod_wifi.sh  || /oem/usr/ko/insmod_wifi.sh ${RK_ENABLE_FASTBOOT} ${RK_ENABLE_WIFI_CHIP}

	# Just for test
	(sleep 5 && test ! -f /data/do-test.sh || sh /data/do-test.sh) &
}

(sleep 3 && run_delay_task) &

EOF
chmod +x $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/rcS

cat > $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/rcK <<EOF
#!/bin/sh
echo "Start to killall task!!!"

while true
do
	if ps |grep -v grep  |grep rkipc;then
		echo "killall -9 rkipc"
		killall -9 rkipc
	elif ps |grep -v grep  |grep fastboot_client;then
		echo "killall -9 fastboot_client"
		killall -9 fastboot_client
	elif ps |grep -v grep  |grep fastboot_server;then
		echo "killall -9 fastboot_server"
		killall -9 fastboot_server
	else
		break
	fi
	sleep .5
done

killall rkwifi_server
umount /data
echo "rcK done"
EOF
chmod +x $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d/rcK
