#!/bin/sh

rcS()
{
	for i in /oem/usr/etc/init.d/S??* ;do

		# Ignore dangling symlinks (if any).
		[ ! -f "$i" ] && continue

		case "$i" in
			*.sh)
				# Source shell script for speed.
				(
					trap - INT QUIT TSTP
					set start
					. $i
				)
				;;
			*)
				# No sh extension, so fork subprocess.
				$i start
				;;
		esac
	done
}

check_linker()
{
        [ ! -L "$2" ] && ln -sf $1 $2
}

get_mac_from_i2c() {
    mac=""
    for reg in fa fb fc fd fe ff; do
        value=$(i2cget -y 1 50 0x$reg)
        value=$(echo "$value" | sed 's/0x//')
        mac="${mac}${value}"
    done
    mac=$(echo "$mac" | tr '[:lower:]' '[:upper:]')
    mac=$(echo "$mac" | sed 's/.\{2\}/&:/g; s/:$//')
    echo "$mac"
}

network_init()
{
	mac_address=$(get_mac_from_i2c)
	ifconfig eth0 down
	ifconfig eth0 hw ether $mac_address
	# ethaddr1=`ifconfig -a | grep "eth.*HWaddr" | awk '{print $5}'`

	# if [ -f /data/ethaddr.txt ]; then
	# 	ethaddr2=`cat /data/ethaddr.txt`
	# 	if [ $ethaddr1 == $ethaddr2 ]; then
	# 		echo "eth HWaddr cfg ok"
	# 	else
	# 		ifconfig eth0 down
	# 		ifconfig eth0 hw ether $ethaddr2
	# 	fi
	# else
	# 	echo $ethaddr1 > /data/ethaddr.txt
	# fi
	ifconfig eth0 up && udhcpc -i eth0
}

post_chk()
{
	g_platform="RV1106"

	str=`cat /sys/firmware/devicetree/base/model | grep RV1126`
	if [ -n "$str" ]; then
		g_platform="RV1126"
		echo "AOV init in RV1126 Platform"
	else
		echo "AOV init in RV1106 Platform"
	fi

	#TODO: ensure /userdata mount done
	cnt=0
	while [ $cnt -lt 30 ];
	do
		cnt=$(( cnt + 1 ))
		if mount | grep -w userdata; then
			break
		fi
		sleep .1
	done

	# if ko exist, install ko first
	default_ko_dir=/ko
	if [ -f "/oem/usr/ko/insmod_ko.sh" ];then
		default_ko_dir=/oem/usr/ko
	fi
	if [ -f "$default_ko_dir/insmod_ko.sh" ];then
		cd $default_ko_dir && sh insmod_ko.sh && cd -
	fi

	network_init &

	if [ "$g_platform"x = "RV1126"x ];then
		echo userspace >/sys/devices/platform/ffbc0000.npu/devfreq/ffbc0000.npu/governor
		echo 600000000 >/sys/devices/platform/ffbc0000.npu/devfreq/ffbc0000.npu/userspace/set_freq
		echo 396000000 > /proc/mpp_service/rkvenc/clk_core
		echo 960 > /sys/devices/platform/rkcif_mipi_lvds/wait_line
		echo 960 > /sys/module/video_rkisp/parameters/wait_line
	else
		cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors
		echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
		cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
		echo 1416000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
	fi

	export sensor_name="sc200ai"
	if [ "$sensor_name"x = "sc200ai"x ]; then
		export rk_cam_w=1920
		export rk_cam_h=1080
	fi

	if [ "$sensor_name"x = "sc3338"x ]; then
		export rk_cam_w=2304
		export rk_cam_h=1296
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		io -4 0xff3e0048 91000
	else
		io -4 0xff300048 32000
	fi


	if [ -e "/userdata/auto_userdata_test" ];then
		/userdata/auto_test.sh &
	fi
	telnetd &
}

rcS

ulimit -c unlimited
echo "/data/core-%p-%e" > /proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 > /proc/sys/vm/overcommit_memory

post_chk &
