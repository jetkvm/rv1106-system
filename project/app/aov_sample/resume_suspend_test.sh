#!/bin/sh
##  MCU 空跑, ARM 唤醒后马上进入休眠, 拷机10次进入重启一次, reboot 10000次##
file_flag_test_arm_reboot_mcu_run_sleep10ms="/userdata/test_arm_reboot_mcu_run_sleep10ms"
file_flag_test_arm_dd_mcu_run_sleep10ms="/userdata/test_arm_dd_mcu_run_sleep10ms"
file_flag_test_arm_null_mcu_run_sleep10ms="/userdata/test_arm_null_mcu_run_sleep10ms"
file_flag_test_arm_npu_mcu_run_sleep10ms="/userdata/test_arm_npu_mcu_run_sleep10ms"
file_flag_test_arm_venc_mcu_run_sleep10ms="/userdata/test_arm_venc_mcu_run_sleep10ms"
file_flag_test_arm_vi_mcu_run_ae="/userdata/test_arm_vi_mcu_run_ae"
file_flag_test_arm_multi_vi_mcu_run_ae="/userdata/test_arm_multi_vi_mcu_run_ae"
file_flag_test_arm_vi_venc_npu_mcu_run_ae="/userdata/test_arm_vi_venc_npu_mcu_run_ae"
file_flag_test_arm_vi_venc_npu_mcu_run_ae_wakeup="/userdata/test_arm_vi_venc_npu_mcu_run_ae_wakeup"
file_flag_test_arm_aiisp_iva_venc="/userdata/test_arm_aiisp_iva_venc"
g_test_count=10000

test_arm_reboot_mcu_run_sleep10ms(){
	if [ ! -e "$file_flag_test_arm_reboot_mcu_run_sleep10ms" ]; then
		return;
	fi
	io -4 0xff300048 3200
	io -4 0x00801c00 1
	io -4 0x00801c04 10
	io -4 0x00801c08 0
	io -4 0x00801c0c "$g_test_count"
	io -w -f /oem/usr/share/rtthread_only_sleep.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	if [ ! -e "$file_flag_test_arm_reboot_mcu_run_sleep10ms" ]; then
		echo "$file_flag_test_arm_reboot_mcu_run_sleep10ms not found, example: echo 10000 $file_flag_test_arm_reboot_mcu_run_sleep10ms"
		return;
	fi
	reboot_count=$(cat "$file_flag_test_arm_reboot_mcu_run_sleep10ms")
	counter=0
	while [ $counter -lt 10 ]
	do
		echo mem > /sys/power/state
		counter=$(( counter + 1 ))
	done

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$(( reboot_count - 1 ))
		echo "$reboot_count" > "$file_flag_test_arm_reboot_mcu_run_sleep10ms"
		sync
		reboot -f
	fi
	rm -rf "$file_flag_test_arm_reboot_mcu_run_sleep10ms"
	sync
	reboot -f
}

##  MCU 空跑，ARM 唤醒后马上进入休眠，拷机35万次，并检查/tmp/test.bin文件内容 ##
test_arm_dd_mcu_run_sleep10ms(){
	if [ ! -e "$file_flag_test_arm_dd_mcu_run_sleep10ms" ]; then
		return;
	fi
	io -4 0xff300048 3200
	io -4 0x00801c00 1
	io -4 0x00801c04 10
	io -4 0x00801c08 0
	io -4 0x00801c0c "$g_test_count"
	io -w -f /oem/usr/share/rtthread_only_sleep.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	
	dd if=/dev/zero of=/tmp/test.bin bs=1M count=10
	test_md5_result=`md5sum /tmp/test.bin | awk '{print $1}'`
	
	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		md5_result=`md5sum /tmp/test.bin | awk '{print $1}'`
		echo "md5sum result = $test_md5_result, $md5_result"
		if [ "$md5_result"x != "$test_md5_result"x ]; then
			echo "/tmp/test_0.bin is error, break test"
			touch "/userdata/failed_test_arm_dd_mcu_run_sleep10ms"
			break;
		fi
		md5sum /tmp/test.bin
		dd if=/dev/zero of=/tmp/test.bin bs=1M count=10
		echo mem > /sys/power/state
		counter=$(( counter + 1 ))
	done
	rm -rf "$file_flag_test_arm_dd_mcu_run_sleep10ms"
	sync
	reboot -f
}

##  MCU 空跑，MCU 唤醒后马上进入休眠，拷机5万次 ##
test_arm_null_mcu_run_sleep10ms(){
	if [ ! -e "$file_flag_test_arm_null_mcu_run_sleep10ms" ]; then
		return;
	fi

	io -4 0xff300048 3200
	io -4 0x00801c00 2
	io -4 0x00801c04 10
	io -4 0x00801c08 0
	io -4 0x00801c0c "$g_test_count"
	io -w -f /oem/usr/share/rtthread_only_sleep.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	echo mem > /sys/power/state
	rm -rf "$file_flag_test_arm_null_mcu_run_sleep10ms"
	sync
	reboot -f
}

##  MCU 空跑，ARM 唤醒后马上进入休眠，NPU 动态离线帧后台运行，拷机35万次 ##
test_arm_npu_mcu_run_sleep10ms(){
	if [ ! -e "$file_flag_test_arm_npu_mcu_run_sleep10ms" ]; then
		return;
	fi
	io -4 0xff300048 3200
	io -4 0x00801c00 1
	io -4 0x00801c04 10
	io -4 0x00801c08 0
	io -4 0x00801c0c "$g_test_count"
	io -w -f /oem/usr/share/rtthread_only_sleep.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	sample_iva_stresstest -w 512 -h 288 &
	counter=0
	while [ $counter -lt "$g_test_count" ]
	do
		echo mem > /sys/power/state
		sleep 0.1
		counter=$(( counter + 1 ))
	done
	rm -rf "$file_flag_test_arm_npu_mcu_run_sleep10ms"
	sync
	reboot -f
}

##  MCU 空跑，MCU 唤醒后马上进入休眠，且后台rk_mpi_venc_test 在编码，拷机35万次 ##
test_arm_venc_mcu_run_sleep10ms(){
	if [ ! -e "$file_flag_test_arm_venc_mcu_run_sleep10ms" ]; then
		return;
	fi
	io -4 0xff300048 3200
	io -4 0x00801c00 1
	io -4 0x00801c04 10
	io -4 0x00801c08 0
	io -4 0x00801c0c "$g_test_count"
	io -w -f /oem/usr/share/rtthread_only_sleep.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	
	rk_mpi_venc_test -w 2304 -h 1296 -C 8 -s 200 -o /tmp/
	if [ ! -f /tmp/test_0.bin ]; then
		echo "rk_mpi_venc_test -w 2304 -h 1296 -C 8 -s 200 -o /tmp/ start faild"
		touch "/userdata/failed_test_arm_venc_mcu_run_sleep10ms"
		rm -rf "$file_flag_test_arm_venc_mcu_run_sleep10ms"
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
				touch "/userdata/failed_test_arm_venc_mcu_run_sleep10ms"
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
	rm -rf "$file_flag_test_arm_venc_mcu_run_sleep10ms"
	sync
	reboot -f
}

## 只跑vi, 单帧流程 ##
test_arm_vi_mcu_run_ae(){
	if [ ! -e "$file_flag_test_arm_vi_mcu_run_ae" ]; then
		return;
	fi
	io -w -f /oem/usr/share/rtthread_"$sensor_name"_log.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ -l -1 --ae_mode 1 --arm_max_run_count $g_test_count --suspend_time 100 --meta_path $meta_path &
	while [ true ]
	do
		sleep 10
		arm_run=`io -4 0x00801c18 | awk '{print $2}'`
		arm_run_max=`io -4 0x00801c1c | awk '{print $2}'`
		arm_run=$(printf %d 0x"$arm_run")
		arm_run_max=$(printf %d 0x"$arm_run_max")
		if [ "$arm_run" -gt "$arm_run_max" ]; then
			break;
		fi
	done
	rkipc_pid=$(ps |grep sample_aov_vi |grep -v grep |awk '{print $1}')
	kill -9 "$rkipc_pid"
	while [ true ]
	do
		sleep 10
		ps|grep sample_aov_vi |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		echo "one slee one"
	done
	rm -rf "$file_flag_test_arm_vi_mcu_run_ae"
	sync
	reboot -f
}

## 只跑vi, 单帧多帧切换流程 ##
test_arm_multi_vi_mcu_run_ae(){
	if [ ! -e "$file_flag_test_arm_multi_vi_mcu_run_ae" ]; then
		return;
	fi
	io -w -f /oem/usr/share/rtthread_"$sensor_name"_log.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ -l -1 --ae_mode 1 --arm_max_run_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --meta_path $meta_path &

	while [ true ]
	do
		sleep 10
		arm_run=`io -4 0x00801c18 | awk '{print $2}'`
		arm_run_max=`io -4 0x00801c1c | awk '{print $2}'`
		arm_run=$(printf %d 0x"$arm_run")
		arm_run_max=$(printf %d 0x"$arm_run_max")
		if [ "$arm_run" -gt "$arm_run_max" ]; then
			break;
		fi
	done
	rkipc_pid=$(ps |grep sample_aov_vi|grep -v grep |awk '{print $1}')
	kill -9 "$rkipc_pid"
	while [ true ]
	do
		sleep 10
		ps|grep sample_aov_vi |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		echo "one slee one"
	done
	rm -rf "$file_flag_test_arm_multi_vi_mcu_run_ae"
	sync
	reboot -f
}


## 休眠唤醒流程, 每帧固定唤醒 ##
test_arm_vi_venc_npu_mcu_run_ae(){
	if [ ! -e "$file_flag_test_arm_vi_venc_npu_mcu_run_ae" ]; then
		return;
	fi
	io -w -f /oem/usr/share/rtthread_"$sensor_name"_log.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind

	sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ -I 0 -e h264cbr -b 4096 --ae_mode 1 --arm_max_run_count $g_test_count --suspend_time 100 --meta_path $meta_path &
	while [ true ]
	do
		sleep 10
		arm_run=`io -4 0x00801c18 | awk '{print $2}'`
		arm_run_max=`io -4 0x00801c1c | awk '{print $2}'`
		arm_run=$(printf %d 0x"$arm_run")
		arm_run_max=$(printf %d 0x"$arm_run_max")
		if [ "$arm_run" -gt "$arm_run_max" ]; then
			break;
		fi
	done
	rkipc_pid=$(ps |grep sample_aov_vi_iva_venc|grep -v grep |awk '{print $1}')
	kill -9 "$rkipc_pid"
	while [ true ]
	do
		sleep 10
		ps|grep sample_aov_vi_iva_venc |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		echo "one slee one"
	done
	rm -rf "$file_flag_test_arm_vi_venc_npu_mcu_run_ae"
	sync
	reboot -f
}

test_arm_vi_venc_npu_mcu_run_ae_wakeup(){
	if [ ! -e "$file_flag_test_arm_vi_venc_npu_mcu_run_ae_wakeup" ]; then
		return;
	fi
	io -w -f /oem/usr/share/rtthread_"$sensor_name"_log.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind

	sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ -I 0 -e h264cbr -b 4096 --arm_max_run_count $g_test_count --suspend_time 100 --meta_path $meta_path &

	while [ true ]
	do
		sleep 10
		arm_run=`io -4 0x00801c18 | awk '{print $2}'`
		arm_run_max=`io -4 0x00801c1c | awk '{print $2}'`
		arm_run=$(printf %d 0x"$arm_run")
		arm_run_max=$(printf %d 0x"$arm_run_max")
		if [ "$arm_run" -gt "$arm_run_max" ]; then
			break;
		fi
	done
	rkipc_pid=$(ps |grep sample_aov_vi_iva_venc|grep -v grep |awk '{print $1}')
	kill -9 "$rkipc_pid"
	while [ true ]
	do
		sleep 10
		ps|grep sample_aov_vi_iva_venc |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		echo "one slee one"
	done
	rm -rf "$file_flag_test_arm_vi_venc_npu_mcu_run_ae_wakeup"
	sync
	reboot -f
}

test_arm_aiisp_iva_venc(){
	if [ ! -e "$file_flag_test_arm_aiisp_iva_venc" ]; then
		return;
	fi
	io -w -f /oem/usr/share/rtthread_"$sensor_name"_log.bin  0x40000
	echo "ffaa0000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind

	sample_aov_aiisp_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ -I 0 -e h264cbr -b 4096 --arm_max_run_count $g_test_count --suspend_time 100 --meta_path $meta_path &

	while [ true ]
	do
		sleep 10
		arm_run=`io -4 0x00801c18 | awk '{print $2}'`
		arm_run_max=`io -4 0x00801c1c | awk '{print $2}'`
		arm_run=$(printf %d 0x"$arm_run")
		arm_run_max=$(printf %d 0x"$arm_run_max")
		if [ "$arm_run" -gt "$arm_run_max" ]; then
			break;
		fi
	done
	rkipc_pid=$(ps |grep sample_aov_aiisp_iva_venc|grep -v grep |awk '{print $1}')
	kill -9 "$rkipc_pid"
	while [ true ]
	do
		sleep 10
		ps|grep sample_aov_aiisp_iva_venc |grep -v grep
		if [ $? -ne 0 ]; then
			break;
		fi
		echo "one slee one"
	done
	rm -rf "$file_flag_test_arm_aiisp_iva_venc"
	sync
	reboot -f

}


if [ "$1"x = "start"x ]; then
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

	echo "$g_test_count" > "$file_flag_test_arm_reboot_mcu_run_sleep10ms"
	touch "$file_flag_test_arm_dd_mcu_run_sleep10ms"
	touch "$file_flag_test_arm_null_mcu_run_sleep10ms"
	touch "$file_flag_test_arm_npu_mcu_run_sleep10ms"
	touch "$file_flag_test_arm_venc_mcu_run_sleep10ms"
	touch "$file_flag_test_arm_vi_mcu_run_ae"
	touch "$file_flag_test_arm_multi_vi_mcu_run_ae"
	touch "$file_flag_test_arm_vi_venc_npu_mcu_run_ae"
	touch "$file_flag_test_arm_vi_venc_npu_mcu_run_ae_wakeup"
	touch "$file_flag_test_arm_aiisp_iva_venc"
	sync
    reboot
fi

sleep 5
test_arm_reboot_mcu_run_sleep10ms
test_arm_dd_mcu_run_sleep10ms
test_arm_null_mcu_run_sleep10ms
#test_arm_npu_mcu_run_sleep10ms
test_arm_venc_mcu_run_sleep10ms
test_arm_vi_mcu_run_ae
test_arm_multi_vi_mcu_run_ae
test_arm_vi_venc_npu_mcu_run_ae
test_arm_vi_venc_npu_mcu_run_ae_wakeup
test_arm_aiisp_iva_venc

rm -rf /userdata/auto_userdata_test
echo "wakeup test ok"
ls /userdata/failed*
