#!/bin/sh

file_flag_test_arm_aov_system="/userdata/test_arm_aov_system"
file_flag_test_arm_reboot_after_aov="/userdata/test_arm_reboot_after_aov"
file_flag_test_arm_dd_tmp="/userdata/test_arm_dd_tmp"
file_flag_test_arm_venc="/userdata/test_arm_venc"
file_flag_test_arm_vi="/userdata/test_arm_vi"
file_flag_test_arm_vi_restart_app="/userdata/test_arm_vi_restart_app"
file_flag_test_arm_vi_multi_frame="/userdata/test_arm_multi_vi"
file_flag_test_arm_vi_venc="/userdata/test_arm_vi_venc"
file_flag_test_arm_vi_iva_venc="/userdata/test_arm_vi_iva_venc"
file_flag_test_arm_aiisp_iva_venc="/userdata/test_arm_aiisp_iva_venc"
file_flag_test_arm_multi_vi="/userdata/test_arm_multi_vi"
file_flag_test_arm_multi_vi_restart_app="/userdata/test_arm_multi_vi_restart_app"
file_flag_test_arm_multi_vi_reboot_after_aov="/userdata/test_arm_multi_vi_reboot_after_aov"
file_flag_test_arm_multi_vi_multi_frame="/userdata/test_arm_multi_vi_multi_frame"
file_flag_test_arm_multi_vi_venc="/userdata/test_arm_multi_vi_venc"
file_flag_test_arm_multi_vi_iva_venc="/userdata/test_arm_multi_vi_iva_venc"
file_flag_test_arm_multi_vi_iva_venc_restart_app="/userdata/test_arm_multi_vi_iva_venc_restart_app"
file_flag_test_npu="/userdata/test_arm_npu"

g_test_count=10000
sensor_width=$rk_cam_w
sensor_height=$rk_cam_h
sensor2_width=$rk_cam2_w
sensor2_height=$rk_cam2_h

g_platform="RV1106"

str=`cat /sys/firmware/devicetree/base/model | grep RV1126`
if [ -n "$str" ]; then
	g_platform="RV1126"
	echo "resume suspend test in RV1126 Platform"
else
	echo "resume suspend test in RV1106 Platform"
fi


## 休眠唤醒10万次后，reboot重启机器
test_arm_aov_system(){
	if [ ! -e "$file_flag_test_arm_aov_system" ]; then
		return;
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	test_count=$((g_test_count * 10))
	while [ $test_count -gt 0 ]
	do
		echo mem > /sys/power/state
		sleep 0.01
		test_count=$((test_count - 1))
	done

	rm -rf "$file_flag_test_arm_aov_system"
	sync
	reboot -f
}

## 休眠唤醒10次后, reboot 重启机器
test_arm_reboot_after_aov(){
	if [ ! -e "$file_flag_test_arm_reboot_after_aov" ]; then
		return;
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	if [ ! -e "$file_flag_test_arm_reboot_after_aov" ]; then
		echo "$file_flag_test_arm_reboot_after_aov not found, example: echo 10000 $file_flag_test_arm_reboot_after_aov"
		return;
	fi

	reboot_count=$(cat "$file_flag_test_arm_reboot_after_aov")
	counter=0
	while [ $counter -lt 10 ]
	do
		echo mem > /sys/power/state
		counter=$(( counter + 1 ))
	done

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$(( reboot_count - 1 ))
		echo "$reboot_count" > "$file_flag_test_arm_reboot_after_aov"
		sync
		reboot -f
	fi
	rm -rf "$file_flag_test_arm_reboot_after_aov"
	sync
	reboot -f
}

## 休眠唤醒g_test_count次后, 并检查/tmp/test.bin 内容
test_arm_dd_tmp(){
	if [ ! -e "$file_flag_test_arm_dd_tmp" ]; then
		return;
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	
	dd if=/dev/zero of=/tmp/test.bin bs=1M count=10
	test_md5_result=`md5sum /tmp/test.bin | awk '{print $1}'`
	
	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		md5_result=`md5sum /tmp/test.bin | awk '{print $1}'`
		echo "md5sum result = $test_md5_result, $md5_result"
		if [ "$md5_result"x != "$test_md5_result"x ]; then
			echo "/tmp/test_0.bin is error, break test"
			touch "/userdata/failed_test_arm_dd_tmp"
			break;
		fi
		md5sum /tmp/test.bin
		dd if=/dev/zero of=/tmp/test.bin bs=1M count=10
		echo mem > /sys/power/state
		counter=$(( counter + 1 ))
	done
	rm -rf "$file_flag_test_arm_dd_tmp"
	sync
	reboot -f
}


##  MCU 空跑，MCU 唤醒后马上进入休眠，且后台rk_mpi_venc_test 在编码，拷机35万次 ##
test_arm_venc(){
	if [ ! -e "$file_flag_test_arm_venc" ]; then
		return;
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	
	rk_mpi_venc_test -w 2304 -h 1296 -C 8 -s 200 -o /tmp/
	if [ ! -f /tmp/test_0.bin ]; then
		echo "rk_mpi_venc_test -w 2304 -h 1296 -C 8 -s 200 -o /tmp/ start faild"
		touch "/userdata/failed_test_arm_venc"
		rm -rf "$file_flag_test_arm_venc"
		sync
		reboot -f
	fi
	venc0_md5_result=`md5sum /tmp/test_0.bin | awk '{print $1}'`
	counter=0
	venc_run_counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		ps|grep rk_mpi_venc_test |grep -v grep
		if [ $? -ne 0 ]; then
			echo "rk_mpi_venc_test not exit, restart"
			md5_result=`md5sum /tmp/test_0.bin | awk '{print $1}'`
			echo "md5sum result = $venc0_md5_result, $md5_result"
			if [ "$md5_result"x != "$venc0_md5_result"x ]; then
				echo "/tmp/test_0.bin is error, break test"
				touch "/userdata/failed_test_arm_venc"
				break;
			fi
			sleep 1
			rk_mpi_venc_test -w 2304 -h 1296 -C 8 -s 200 -o /tmp/ &
			venc_run_counter=$(( venc_run_counter + 1 ))
		fi
		echo "LoopCount = $counter, vencRunCounter = $venc_run_counter"
		sleep 1
		echo mem > /sys/power/state
		counter=$(( counter + 1 ))
	done
	while [ true ]
	do
		ps|grep rk_mpi_venc_test |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		sleep 10
		echo "slee one"
	done
	rm -rf "$file_flag_test_arm_venc"
	sync
	reboot -f
}


## vi 单帧压测 ##
test_arm_vi(){
	if [ ! -e "$file_flag_test_arm_vi" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_vi"
	fi
	rm -rf "$file_flag_test_arm_vi"
	sync
	reboot -f
}

## vi 进程重启测试 ##
test_arm_vi_restart_app(){
	if [ ! -e "$file_flag_test_arm_vi_restart_app" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			touch "/userdata/failed_test_arm_vi_restart_app"
			break;
		fi
		counter=$(( counter + 1 ))
	done

	rm -rf "$file_flag_test_arm_vi_restart_app"
	sync
	reboot -f

}

## vi 单多帧来回切换 ##
test_arm_vi_multi_frame(){
	if [ ! -e "$file_flag_test_arm_vi_multi_frame" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_vi_multi_frame"
	fi
	rm -rf "$file_flag_test_arm_vi_multi_frame"
	sync
	reboot -f
}

## vi-venc 压测 ##
test_arm_vi_venc(){
	if [ ! -e "$file_flag_test_arm_vi_venc" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_vi_venc"
	fi

	rm -rf "$file_flag_test_arm_vi_venc"
	sync
	reboot -f
}

## vi-iva-venc 压测 ##
test_arm_vi_iva_venc(){
	if [ ! -e "$file_flag_test_arm_vi_iva_venc" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_vi_iva_venc"
	fi

	rm -rf "$file_flag_test_arm_vi_iva_venc"
	sync
	reboot -f
}

## vi-aiisp-iva-venc 压测 ##
test_arm_aiisp_iva_venc(){
	if [ ! -e "$file_flag_test_arm_aiisp_iva_venc" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_aiisp_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_aiisp_iva_venc"
	fi

	rm -rf "$file_flag_test_arm_aiisp_iva_venc"
	sync
	reboot -f

}

## multi_vi 单帧压测 ##
test_arm_multi_vi(){
	if [ ! -e "$file_flag_test_arm_multi_vi" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi"
	fi

	rm -rf "$file_flag_test_arm_multi_vi"
	sync
	reboot -f
}

## multi_vi 单帧压测 ##
test_arm_multi_vi_restart_app(){
	if [ ! -e "$file_flag_test_arm_multi_vi_restart_app" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			touch "/userdata/failed_test_arm_multi_vi_restart_app"
			break;
		fi
		counter=$(( counter + 1 ))
	done

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi_restart_app"
	fi

	rm -rf "$file_flag_test_arm_multi_vi_restart_app"
	sync
	reboot -f
}

## 休眠唤醒10次后, reboot 重启机器
test_arm_multi_vi_reboot_after_aov(){
	if [ ! -e "$file_flag_test_arm_multi_vi_reboot_after_aov" ]; then
		return;
	fi

	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi_reboot_after_aov"
		echo 0 > "$file_flag_test_arm_reboot_after_aov"
	fi

	reboot_count=$(cat "$file_flag_test_arm_multi_vi_reboot_after_aov")

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$(( reboot_count - 1 ))
		echo "$reboot_count" > "$file_flag_test_arm_multi_vi_reboot_after_aov"
		sync
		reboot -f
	fi
	rm -rf "$file_flag_test_arm_multi_vi_reboot_after_aov"
	sync
	reboot -f
}

## multi_vi 单多帧来回切换 ##
test_arm_multi_vi_multi_frame(){
	if [ ! -e "$file_flag_test_arm_multi_vi_multi_frame" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_multi_vi -w -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi_multi_frame"
	fi

	rm -rf "$file_flag_test_arm_multi_vi_multi_frame"
	sync
	reboot -f
}

## multi_vi-venc 压测 ##
test_arm_multi_vi_venc(){
	if [ ! -e "$file_flag_test_arm_multi_vi_venc" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_multi_vi_venc -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi_iva_venc"
	fi

	rm -rf "$file_flag_test_arm_multi_vi_venc"
	sync
	reboot -f
}

## multi_vi-iva-venc 压测 ##
test_arm_multi_vi_iva_venc(){
	if [ ! -e "$file_flag_test_arm_multi_vi_iva_venc" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	sample_aov_multi_vi_iva_venc -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_multi_vi_iva_venc"
	fi

	rm -rf "$file_flag_test_arm_multi_vi_iva_venc"
	sync
	reboot -f
}

## multi_vi-iva-venc 测试进程重启
test_arm_multi_vi_iva_venc_restart_app(){
	if [ ! -e "$file_flag_test_arm_multi_vi_iva_venc_restart_app" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi

	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		sample_aov_multi_vi_iva_venc -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			touch "/userdata/failed_test_arm_multi_vi_iva_venc_restart_app"
			break;
		fi
		counter=$(( counter + 1 ))
	done

	rm -rf "$file_flag_test_arm_multi_vi_iva_venc_restart_app"
	sync
	reboot -f

}

## 测试npu ##
test_arm_npu(){
	if [ ! -e "$file_flag_test_npu" ]; then
		return;
	fi
	if [ "$g_platform"x = "RV1126"x ];then
		echo "ffc60000.dwmmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	else
		echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	fi
	sample_aov_npu_test -w 720 -h 480 -l $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		touch "/userdata/failed_test_arm_npu"
	fi
	rm -rf "$file_flag_test_npu"
	sync
	reboot -f
}


if [ "$1"x = "SingleCamTest"x ]; then
	rm -rf /userdata/rkipc.ini
	cp /oem/usr/bin/resume_suspend_test.sh /userdata/auto_test.sh
	chmod a+x userdata/auto_test.sh
	touch /userdata/auto_userdata_test
	touch "$file_flag_test_arm_reboot_mcu_run_sleep10ms"
	if [ -n "$2" ]; then
		g_test_count="$2"
		sed -i "s/g_test_count=10000/g_test_count=$g_test_count/g" /userdata/auto_test.sh
	fi
	echo "wakeup test: g_test_count = $g_test_count"

	echo "$g_test_count" > "$file_flag_test_arm_reboot_after_aov"
	touch "$file_flag_test_arm_aov_system"
	touch "$file_flag_test_arm_reboot_after_aov"
	touch "$file_flag_test_arm_dd_tmp"
	touch "$file_flag_test_arm_venc"
	touch "$file_flag_test_arm_vi"
	#touch "$file_flag_test_arm_vi_restart_app"
	touch "$file_flag_test_arm_vi_multi_frame"
	touch "$file_flag_test_arm_vi_venc"
	touch "$file_flag_test_arm_vi_iva_venc"
	touch "$file_flag_test_arm_aiisp_iva_venc"
	touch "$file_flag_test_npu"

	sync
	reboot -f
fi

if [ "$1"x = "MultiCamTest"x ]; then
	rm -rf /userdata/rkipc.ini
	cp /oem/usr/bin/resume_suspend_test.sh /userdata/auto_test.sh
	chmod a+x userdata/auto_test.sh
	touch /userdata/auto_userdata_test
	touch "$file_flag_test_arm_reboot_mcu_run_sleep10ms"
	if [ -n "$2" ]; then
		g_test_count="$2"
		sed -i "s/g_test_count=10000/g_test_count=$g_test_count/g" /userdata/auto_test.sh
	fi
	echo "wakeup test: g_test_count = $g_test_count"

	touch "$file_flag_test_arm_aov_system"
	echo "$g_test_count" > "$file_flag_test_arm_reboot_after_aov"
	touch "$file_flag_test_arm_reboot_after_aov"
	touch "$file_flag_test_arm_dd_tmp"
	touch "$file_flag_test_arm_venc"
	touch "$file_flag_test_arm_multi_vi"
	#touch "$file_flag_test_arm_multi_vi_restart_app"
	echo "$g_test_count" > "$file_flag_test_arm_multi_vi_reboot_after_aov"
	touch "$file_flag_test_arm_multi_vi_reboot_after_aov"
	touch "$file_flag_test_arm_multi_vi_multi_frame"
	touch "$file_flag_test_arm_multi_vi_venc"
	touch "$file_flag_test_arm_multi_vi_iva_venc"
	#touch "$file_flag_test_arm_multi_vi_iva_venc_restart_app"
	touch "$file_flag_test_npu"
	sync
	reboot -f
fi


sleep 5
if [ "$g_platform"x = "RV1126"x ];then
	io -4 0xff3e0048 9100
else
	io -4 0xff300048 3200
fi
export debug_rockit=1
test_arm_aov_system
test_arm_reboot_after_aov
test_arm_dd_tmp
test_arm_venc
test_arm_npu
test_arm_vi
test_arm_vi_multi_frame
test_arm_vi_restart_app
test_arm_vi_venc
test_arm_vi_iva_venc
test_arm_aiisp_iva_venc
test_arm_multi_vi
test_arm_multi_vi_restart_app
test_arm_multi_vi_reboot_after_aov
test_arm_multi_vi_multi_frame
test_arm_multi_vi_venc
test_arm_multi_vi_iva_venc
test_arm_multi_vi_iva_venc_restart_app

rm -rf /userdata/auto_userdata_test
if ls /userdata/failed*; then
	echo " ===== AOV Testing completed, failed: ====="
else
	echo " ===== AOV Testing completed, ok ====="
fi

