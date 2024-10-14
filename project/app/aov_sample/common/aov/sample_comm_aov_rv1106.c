// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "sample_comm_aov.h"
#include "sample_comm.h"
#include <net/if.h>   // must be included later than <net/if.h>
#include <linux/if.h> // must be included later than <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>

#define MAX_CAMERA_NUM 3
#define SOC_SLEEP_STR "mem"
#define SOC_SLEEP_PATH "/sys/power/state"

#define SUSPEND_TIME_REG 0xff300048

#ifdef AOV_FASTBOOT_ENABLE

#include "rk_meta.h"
#include "rk_meta_wakeup_param.h"
#include "sensor_iq_info.h"

// Memory region definitions
#define META_PHY_ADDR 0x800000
// Meta region
#define META_SIZE (384 * 1024)

struct meta_para_vir {
	int mem_fd;
	void *mapped_base;
	void *meta_head;
	int meta_head_size;
	void *param_share2kernel_offset;
	int param_share2kernel_offset_size;
	void *sensor_init_offset;
	int sensor_init_offset_size;
	void *cmdline_offset;
	int cmdline_offset_size;
	void *ae_table_offset;
	int ae_table_offset_size;
	void *app_param_offset;
	int app_param_offset_size;
	void *secondary_sensor_init_offset;
	int secondary_sensor_init_offset_size;
	void *wakeup_param_offset;
	int wakeup_param_offset_size;
	void *reserve;
	int reserve_size;
	void *backup;
	int backup_size;
	void *sensor_iq_bin_offset;
	int sensor_iq_bin_offset_size;
	void *secondary_sensor_iq_bin_offset;
	int secondary_sensor_iq_bin_offset_size;

	void *wakeup_aov_param_offset;
	int wakeup_aov_param_max_size;
};

static struct meta_para_vir *g_meta_vir = NULL;
#endif // #ifdef AOV_FASTBOOT_ENABLE

#define MAX_NL_BUF_SIZE (1024 * 16)
#define MAX_SELECT_TIMEOUT (2 * 1000 * 1000)

static pthread_mutex_t g_wakeup_run_mutex = PTHREAD_MUTEX_INITIALIZER;

static RK_S32 g_input_device_fd = -1;

#ifdef AOV_FASTBOOT_ENABLE
static void SAMPLE_COMM_AOV_MetaDump() {
	if (!g_meta_vir) {
		printf("[%s()] empty meta!\n", __func__);
		return;
	}
	printf("g_meta_vir->mem_fd: %d\n", g_meta_vir->mem_fd);
	printf("g_meta_vir->mapped_base: %p\n", g_meta_vir->mapped_base);
	printf("g_meta_vir->meta_head: %p\n", g_meta_vir->meta_head);
	printf("g_meta_vir->meta_head_size: %d\n", g_meta_vir->meta_head_size);
	printf("g_meta_vir->param_share2kernel_offset: %p\n",
	       g_meta_vir->param_share2kernel_offset);
	printf("g_meta_vir->param_share2kernel_offset_size: %d\n",
	       g_meta_vir->param_share2kernel_offset_size);
	printf("g_meta_vir->sensor_init_offset: %p\n", g_meta_vir->sensor_init_offset);
	printf("g_meta_vir->sensor_init_offset_size: %d\n",
	       g_meta_vir->sensor_init_offset_size);
	printf("g_meta_vir->cmdline_offset: %p\n", g_meta_vir->cmdline_offset);
	printf("g_meta_vir->cmdline_offset_size: %d\n", g_meta_vir->cmdline_offset_size);
	printf("g_meta_vir->ae_table_offset: %p\n", g_meta_vir->ae_table_offset);
	printf("g_meta_vir->ae_table_offset_size: %d\n", g_meta_vir->ae_table_offset_size);
	printf("g_meta_vir->app_param_offset: %p\n", g_meta_vir->app_param_offset);
	printf("g_meta_vir->app_param_offset_size: %d\n", g_meta_vir->app_param_offset_size);
	printf("g_meta_vir->secondary_sensor_init_offset: %p\n",
	       g_meta_vir->secondary_sensor_init_offset);
	printf("g_meta_vir->secondary_sensor_init_offset_size: %d\n",
	       g_meta_vir->wakeup_param_offset_size);
	printf("g_meta_vir->wakeup_param_offset: %p\n", g_meta_vir->wakeup_param_offset);
	printf("g_meta_vir->wakeup_param_offset_size: %d\n",
	       g_meta_vir->secondary_sensor_init_offset_size);
	printf("g_meta_vir->reserve: %p\n", g_meta_vir->reserve);
	printf("g_meta_vir->reserve_size: %d\n", g_meta_vir->reserve_size);
	printf("g_meta_vir->backup: %p\n", g_meta_vir->backup);
	printf("g_meta_vir->backup_size: %d\n", g_meta_vir->backup_size);
	printf("g_meta_vir->sensor_iq_bin_offset: %p\n", g_meta_vir->sensor_iq_bin_offset);
	printf("g_meta_vir->sensor_iq_bin_offset_size: %d\n",
	       g_meta_vir->sensor_iq_bin_offset_size);
	printf("g_meta_vir->secondary_sensor_iq_bin_offset: %p\n",
	       g_meta_vir->secondary_sensor_iq_bin_offset);
	printf("g_meta_vir->secondary_sensor_iq_bin_offset_size: %d\n",
	       g_meta_vir->secondary_sensor_iq_bin_offset_size);
	printf("g_meta_vir->wakeup_aov_param_offset: %p\n",
	       g_meta_vir->wakeup_aov_param_offset);
	printf("g_meta_vir->wakeup_aov_param_max_size: %d\n",
	       g_meta_vir->wakeup_aov_param_max_size);
	// ... Print other fields ...
}

#define se32_to_be32(x)                                                                  \
	((0x000000FF & x) << 24) | ((0x0000FF00 & x) << 8) | ((0x00FF0000 & x) >> 8) |       \
	    ((0xFF000000 & x) >> 24)

static int SAMPLE_COMM_AOV_MetaMmap() {
	int mem_fd = -1;
	void *mapped_base, *virt_addr;
	uint32_t meta_addr, meta_size;
	int dts_fd;

	if (g_meta_vir != NULL) {
		printf("[%s()] g_meta_vir has been mapped!", __func__);
		return RK_FAILURE;
	}
	g_meta_vir = malloc(sizeof(struct meta_para_vir));
	if (g_meta_vir == NULL) {
		printf("[%s()] g_meta_vir malloc failed!", __func__);
		return RK_FAILURE;
	}
	memset(g_meta_vir, 0, sizeof(struct meta_para_vir));

	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("Error opening /dev/mem");
		return EXIT_FAILURE;
	}

	dts_fd =
	    open("/sys/firmware/devicetree/base/reserved-memory/meta@800000/reg", O_RDONLY);
	if (dts_fd > 0) {
		int read_bytes = read(dts_fd, &meta_addr, sizeof(meta_addr));
		read_bytes = read(dts_fd, &meta_size, sizeof(meta_size));
		if (read_bytes > 0) {
			meta_size = se32_to_be32(meta_size);
			meta_addr = se32_to_be32(meta_addr);
			printf("# read firmware meta addr 0X%08x size 0X%08x!\n", meta_addr,
			       meta_size);
		} else {
			meta_size = META_SIZE;
			meta_addr = META_PHY_ADDR;
			printf("# read dts reg failed!\n");
		}
		close(dts_fd);
	} else {
		meta_size = META_SIZE;
		meta_addr = META_PHY_ADDR;
		printf("# read dts failed!\n");
	}

	mapped_base =
	    mmap(0, meta_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, meta_addr);
	if (mapped_base == MAP_FAILED) {
		perror("Error mapping physical address to virtual address");
		close(mem_fd);
		return EXIT_FAILURE;
	}

	g_meta_vir->mem_fd = mem_fd;
	g_meta_vir->mapped_base = mapped_base;
	g_meta_vir->meta_head = mapped_base + META_INFO_HEAD_OFFSET;
	g_meta_vir->meta_head_size = META_INFO_SIZE;
	g_meta_vir->param_share2kernel_offset = mapped_base + PARAM_SHARE2KERNEL_OFFSET;
	g_meta_vir->param_share2kernel_offset_size = PARAM_SHARE2KERNEL_SIZE;
	g_meta_vir->sensor_init_offset = mapped_base + SENSOR_INIT_OFFSET;
	g_meta_vir->sensor_init_offset_size = SENSOR_INIT_MAX_SIZE;
	g_meta_vir->cmdline_offset = mapped_base + CMDLINE_OFFSET;
	g_meta_vir->cmdline_offset_size = CMDLINE_MAX_SIZE;
	g_meta_vir->ae_table_offset = mapped_base + AE_TABLE_OFFSET;
	g_meta_vir->ae_table_offset_size = AE_TABLE_MAX_SIZE;
	g_meta_vir->app_param_offset = mapped_base + APP_PARAM_OFFSET;
	g_meta_vir->app_param_offset_size = APP_PARAM_MAX_SIZE;
	g_meta_vir->secondary_sensor_init_offset = mapped_base + SECONDARY_SENSOR_INIT_OFFSET;
	g_meta_vir->secondary_sensor_init_offset_size = SECONDARY_SENSOR_INIT_MAX_SIZE;
	g_meta_vir->wakeup_param_offset = mapped_base + WAKEUP_PARAM_OFFSET;
	g_meta_vir->wakeup_param_offset_size = WAKEUP_PARAM_MAX_SIZE;
	// g_meta_vir->reserve = mapped_base + RESERVE;
	// g_meta_vir->reserve_size = RESERVE_SIZE;
	// g_meta_vir->backup = mapped_base + BACKUP;
	// g_meta_vir->backup_size = BACKUP_SIZE;
	g_meta_vir->sensor_iq_bin_offset = mapped_base + SENSOR_IQ_BIN_OFFSET;
	g_meta_vir->sensor_iq_bin_offset_size = SENSOR_IQ_BIN_MAX_SIZE;
	g_meta_vir->secondary_sensor_iq_bin_offset =
	    mapped_base + SECONDARY_SENSOR_IQ_BIN_OFFSET;
	g_meta_vir->secondary_sensor_iq_bin_offset_size = SENSOR_IQ_BIN_MAX_SIZE;
	// g_meta_vir->secondary_sensor_iq_bin_offset_size = 0; // not support
	g_meta_vir->wakeup_aov_param_offset = mapped_base + WAKEUP_AOV_PARAM_OFFSET;
	g_meta_vir->wakeup_aov_param_max_size = WAKEUP_AOV_PARAM_MAX_SIZE;

	SAMPLE_COMM_AOV_MetaDump(g_meta_vir);

	// init mcu mode
	struct wakeup_param_info *wakeup_param =
	    (struct wakeup_param_info *)g_meta_vir->wakeup_param_offset;
	wakeup_param->wakeup_mode = 0;
	wakeup_param->ae_wakeup_mode = 0;
	wakeup_param->arm_run_count = 0;
	wakeup_param->arm_max_run_count = -1;
	wakeup_param->mcu_run_count = 0;
	wakeup_param->mcu_max_run_count = -1;

	return RK_SUCCESS;
}

static int SAMPLE_COMM_AOV_MetaMunmap() {
	if (g_meta_vir) {
		if (g_meta_vir->mapped_base)
			munmap(g_meta_vir->mapped_base, META_SIZE);
		if (g_meta_vir->mem_fd >= 0)
			close(g_meta_vir->mem_fd);
		free(g_meta_vir);
		g_meta_vir = NULL;
	} else {
		printf("[%s()] empty g_meta_vir!\n", __func__);
		return RK_FAILURE;
	}
	return RK_SUCCESS;
}

#endif // #ifdef AOV_FASTBOOT_ENABLE

#ifdef RK_ENABLE_RTT
int SAMPLE_COMM_Get_WakeupBin_Info(uint32_t *addr, uint32_t *size) {

#define RTT_BIN_PARAM "/proc/device-tree/reserved-memory/rtos@40000/reg"
#define uswap_32(x)                                                                      \
	((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) |      \
	 (((x)&0x000000ff) << 24))

	int rtt_bin_param_fd = -1;
	struct RttBinParam {
		unsigned int addr;
		unsigned int size;
	};

	struct RttBinParam bin_param = {0, 0};
	if ((rtt_bin_param_fd = open(RTT_BIN_PARAM, O_RDONLY)) < 0) {
		printf("cannot open [%s]\n", RTT_BIN_PARAM);
		return RK_FAILURE;
	}

	if (read(rtt_bin_param_fd, (void *)&bin_param, sizeof(bin_param)) == -1) {
		printf("read log param error.\n");
		if (rtt_bin_param_fd != -1)
			close(rtt_bin_param_fd);
		return RK_FAILURE;
	}

	if (rtt_bin_param_fd != -1)
		close(rtt_bin_param_fd);

	bin_param.addr = uswap_32(bin_param.addr);
	bin_param.size = uswap_32(bin_param.size);
	printf("rtt wakeup bin addr [%#x]\n", bin_param.addr);
	printf("rtt wakeup bin size [%#x]\n", bin_param.size);

	*addr = bin_param.addr;
	*size = bin_param.size;
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_WakeupBinMmap(const char *rtthread_wakeup_bin_path) {
	int mem_fd = -1;
	uint32_t rtt_bin_addr, rtt_bin_max_size;
	void *mapped_base, *virt_addr;
	if (access(rtthread_wakeup_bin_path, F_OK) != 0) {
		perror("read ree wakeup bin info error");
		return EXIT_FAILURE;
	}

	if (SAMPLE_COMM_Get_WakeupBin_Info(&rtt_bin_addr, &rtt_bin_max_size) != 0) {
		return EXIT_FAILURE;
	}

	FILE *rtt_bin_file = fopen(rtthread_wakeup_bin_path, "r");
	if (rtt_bin_file == NULL) {
		perror("Error opening meta file");
		close(mem_fd);
		return EXIT_FAILURE;
	}
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("Error opening /dev/mem");
		fclose(rtt_bin_file);
		return EXIT_FAILURE;
	}
	mapped_base = mmap(0, rtt_bin_max_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd,
	                   rtt_bin_addr);
	if (mapped_base == MAP_FAILED) {
		perror("Error mapping physical address to virtual address");
		fclose(rtt_bin_file);
		close(mem_fd);
		return EXIT_FAILURE;
	}

	memset(mapped_base, 0, rtt_bin_max_size);

	char buffer[1024];
	size_t read_bytes;
	while ((read_bytes = fread(buffer, 1, sizeof(buffer), rtt_bin_file)) > 0) {
		memcpy(mapped_base, buffer, read_bytes);
		mapped_base += read_bytes;
	}
	fclose(rtt_bin_file);
	munmap(mapped_base, rtt_bin_max_size);
	close(mem_fd);
	return RK_SUCCESS;
}
#endif // #ifdef RK_ENABLE_RTT

int SAMPLE_COMM_AOV_Init() {
#ifdef AOV_FASTBOOT_ENABLE
	SAMPLE_COMM_AOV_MetaMmap();
#endif
	// INFO:
	// Find event device path in /proc/bus/input/devices is a better way.
	g_input_device_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (g_input_device_fd < 0) {
		printf("Open failed /dev/input/event0: %s, you can ignore this"
		       " msg if you don't need gpio wakeup...\n",
		       strerror(errno));
	}

	pthread_mutex_init(&g_wakeup_run_mutex, NULL);
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_Deinit() {
	pthread_mutex_lock(&g_wakeup_run_mutex);
#ifdef AOV_FASTBOOT_ENABLE
	SAMPLE_COMM_AOV_MetaMunmap();
#endif
	pthread_mutex_unlock(&g_wakeup_run_mutex);
	pthread_mutex_destroy(&g_wakeup_run_mutex);

	if (g_input_device_fd >= 0)
		close(g_input_device_fd);
}

void SAMPLE_COMM_AOV_EnterSleep() {
	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (SAMPLE_COMM_ECHO(SOC_SLEEP_PATH, SOC_SLEEP_STR, (strlen(SOC_SLEEP_STR))) == -1) {
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		exit(EXIT_FAILURE);
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}

static bool device_driver_is_bound(const char *device, const char *driver) {
	char path[256] = {'\0'};
	snprintf(path, 256, "%s/%s", driver, device);
	return (access(path, F_OK) == 0);
}

static int device_attach_driver(const char *device, const char *driver) {
	char path[256] = {'\0'};
	int fd = -1;
	int ret = 0;
	// printf("[%s()] Enter\n", __func__);
	snprintf(path, sizeof(path), "%s/bind", driver);
	fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		printf("[%s()] can't open %s because %s\n", __func__, driver, strerror(errno));
		return RK_FAILURE;
	}
	// printf("[%s()] start bind %s to %s\n", __func__, device, driver);
	ret = write(fd, device, strlen(device));
	if (ret < 0) {
		printf("[%s()] bind %s to %s failed because %s\n", __func__, device, driver,
		       strerror(errno));
		close(fd);
		return RK_FAILURE;
	}
	close(fd);
	// printf("[%s()] Exit\n", __func__);
	return RK_SUCCESS;
}

static int device_detach_driver(const char *device, const char *driver) {
	char path[256] = {'\0'};
	int fd = -1;
	int ret = 0;
	// printf("[%s()] Enter\n", __func__);
	snprintf(path, sizeof(path), "%s/unbind", driver);
	fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		printf("[%s()] can't open %s because %s\n", __func__, path, strerror(errno));
		return RK_FAILURE;
	}
	// printf("[%s()] start unbind %s from %s\n", __func__, device, driver);
	ret = write(fd, device, strlen(device));
	if (ret < 0) {
		printf("[%s()] unbind %s from %s failed because %s\n", __func__, device, driver,
		       strerror(errno));
		close(fd);
		return RK_FAILURE;
	}
	close(fd);
	// printf("[%s()] Exit\n", __func__);
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_SetSuspendTime(int wakeup_suspend_time) {
	int result = SAMPLE_COMM_WriteReg(SUSPEND_TIME_REG, wakeup_suspend_time * 32.768);
	if (result != 0) {
		printf("Failed to set suspend time!\n");
		return RK_FAILURE;
	}
	printf("[%s()] wakeup suspend time = %d\n", __func__, wakeup_suspend_time);
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_PreInitIsp(const char *sensor_name, const char *iq_file_dir,
                               int cam_index) {
	int ret = RK_SUCCESS;
	rk_aiq_tb_info_t tb_info;
	struct sensor_iq_info *sensor_iq = NULL;
	void *main_iq_offset = NULL;
	void *secondary_iq_offset = NULL;

	pthread_mutex_lock(&g_wakeup_run_mutex);
	memset(&tb_info, 0, sizeof(rk_aiq_tb_info_t));
	tb_info.magic = sizeof(rk_aiq_tb_info_t) - 2;

#ifdef AOV_FASTBOOT_ENABLE
	if (!g_meta_vir) {
		printf("[%s()] empty meta image!\n", __func__);
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		return RK_FAILURE;
	}

	if (cam_index < MAX_CAMERA_NUM) {
		sensor_iq = (struct sensor_iq_info *)g_meta_vir->sensor_iq_bin_offset;
		main_iq_offset = (uint8_t *)g_meta_vir->mapped_base +
		                 (sensor_iq->main_sensor_iq_offset - META_PHY_ADDR);
		secondary_iq_offset = (uint8_t *)g_meta_vir->mapped_base +
		                      (sensor_iq->secondary_sensor_iq_offset - META_PHY_ADDR);
		printf("[%s()] main addr %p size %d, second addr %p size %d\n", __func__,
		       sensor_iq->main_sensor_iq_offset, sensor_iq->main_sensor_iq_size,
		       sensor_iq->secondary_sensor_iq_offset,
		       sensor_iq->secondary_sensor_iq_size);
	} else {
		printf("[%s()] error camera index %d!\n", __func__, cam_index);
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		return RK_FAILURE;
	}

	tb_info.is_start_once = true;
	tb_info.is_pre_aiq = true;
	tb_info.prd_type = RK_AIQ_PRD_TYPE_TB_BATIPC;
	// tb_info.rtt_share_addr = g_meta_vir->wakeup_aov_param_offset;
	tb_info.rtt_share_addr = 0;
#else
	tb_info.is_start_once = false;
	tb_info.is_pre_aiq = false;
	tb_info.prd_type = RK_AIQ_PRD_TYPE_SINGLE_FRAME;
	tb_info.rtt_share_addr = 0;
	if (iq_file_dir != NULL) {
		printf("rkaiq use iqfiles from %s\n", iq_file_dir);
		tb_info.iq_bin_mode = RK_AIQ_META_NOT_FULL_IQ_BIN;
	}
#endif
	ret = rk_aiq_uapi2_sysctl_preInit_tb_info(sensor_name, &tb_info);
	if (ret != RK_SUCCESS)
		printf("[%s()] rk_aiq_uapi2_sysctl_preInit_tb_info failed %#X!\n", __func__, ret);

#ifdef AOV_FASTBOOT_ENABLE
	if (cam_index == 0)
		ret = rk_aiq_uapi2_sysctl_preInit_iq_addr(sensor_name, main_iq_offset,
		                                          sensor_iq->main_sensor_iq_size);
	else if (cam_index == 1)
		ret = rk_aiq_uapi2_sysctl_preInit_iq_addr(sensor_name, secondary_iq_offset,
		                                          sensor_iq->secondary_sensor_iq_size);
	else
		printf("[%s()] not support camera index %d now!\n", cam_index);
	if (ret != RK_SUCCESS)
		printf("[%s()] rk_aiq_uapi2_sysctl_preInit_iq_addr failed %#X!\n", __func__, ret);
#endif
	pthread_mutex_unlock(&g_wakeup_run_mutex);
	return ret;
}

#define ETHERNET_DEVICE "ffa80000.ethernet"
#define ETHERNET_DRIVER "/sys/bus/platform/drivers/rk_gmac-dwmac/"
// #define NIC_DEVICE "stmmac-0:02"
// #define NIC_DRIVER "/sys/bus/mdio_bus/drivers/RK630 PHY/"
#define ETHERNET_BIND_DONE "/sys/bus/platform/drivers/rk_gmac-dwmac/"
#define ETHERNET_UNBNID_DONE "/sys/bus/platform/drivers/rk_gmac-dwmac/"

int SAMPLE_COMM_AOV_BindEthernet() {
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	char name[256] = {'\0'};
	int ret = 0;
	int fd = -1;
	int len = MAX_NL_BUF_SIZE;
	fd_set read_set;
	struct ifreq ifr;
	struct timeval timeout;
	struct sockaddr_nl addr;
	struct nlmsghdr *nh = NULL;
	struct ifinfomsg *ifinfo = NULL;

	printf("[%s()] Enter\n", __func__);
	if (device_driver_is_bound(ETHERNET_DEVICE, ETHERNET_DRIVER)) {
		printf("[%s()] ethernet device already bind!\n", __func__);
		goto __FAILED;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTNLGRP_LINK;
	addr.nl_pid = getpid();
	if (fd < 0) {
		printf("[%s()] Failed to open network netlink because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind network netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout,
	           (socklen_t)sizeof(struct timeval));

	// bind ethernet driver
	if (device_attach_driver(ETHERNET_DEVICE, ETHERNET_DRIVER) != RK_SUCCESS)
		goto __FAILED;

#if 1
	// open etherner device
	system("ifconfig eth0 up");
	// reset local ip, gateway and dns resolvation
	system("killall -9 udhcpc");                // restart udhcpc
	system("route del default gw 0.0.0.0");     // delete default gateway
	system("cat /dev/null > /etc/resolv.conf"); // reset DNS resolvation
	system("udhcpc -i eth0 -T 1 -A 0 -b -q");   // find a new ip address for eth0
#endif
	// wait for bind success
	// FIXME:
	// there has some bug for select()
	// ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	memset(&buf, 0, sizeof(buf));
	while ((ret = read(fd, buf, sizeof(buf))) > 0) {
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, ret); nh = NLMSG_NEXT(nh, ret)) {
			if (nh->nlmsg_type == NLMSG_DONE) {
				printf("[%s()] NLMSG_DONE\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type == NLMSG_ERROR) {
				printf("[%s()] NLMSG_ERROR\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type != RTM_NEWLINK) {
				printf("[%s()] not RTM_NEWLINK, ignore\n", __func__);
				continue;
			}
			ifinfo = NLMSG_DATA(nh);
			if_indextoname(ifinfo->ifi_index, name);

			// printf("[%s()] %s: up %s, lower up %s, running %s, value 0X%X\n", __func__
			// 	, name
			// 	, (ifinfo->ifi_flags & IFF_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_LOWER_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_RUNNING) ? "true" : "false"
			// 	, ifinfo->ifi_flags
			// );

			if ((ifinfo->ifi_flags & IFF_RUNNING) && (ifinfo->ifi_flags & IFF_LOWER_UP) &&
			    (ifinfo->ifi_flags & IFF_UP)) {
				printf("[%s()] ethernet bind success!\n", __func__);
				goto __SUCCESS;
			}
		}
	}
	if (ret < 0)
		goto __FAILED;

__SUCCESS:
	if (fd >= 0)
		close(fd);
	printf("[%s()] Exit\n", __func__);
	return RK_SUCCESS;
__FAILED:
	if (fd >= 0)
		close(fd);
	printf("[%s()] Exit Error\n", __func__);
	return RK_FAILURE;
}

int SAMPLE_COMM_AOV_UnbindEthernet() {
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	char name[256] = {'\0'};
	int ret = 0;
	int fd = -1;
	int len = MAX_NL_BUF_SIZE;
	fd_set read_set;
	struct sockaddr_nl addr;
	struct timeval timeout;
	struct nlmsghdr *nh = NULL;
	struct ifinfomsg *ifinfo = NULL;

	printf("[%s()] Enter\n", __func__);
	if (!device_driver_is_bound(ETHERNET_DEVICE, ETHERNET_DRIVER)) {
		printf("[%s()] ethernet device already unbind!\n", __func__);
		goto __FAILED;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTNLGRP_LINK;
	addr.nl_pid = getpid();
	if (fd < 0) {
		printf("[%s()] Failed to open network netlink because %s\n", __func__,
		       strerror(errno));
		return RK_FAILURE;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind network netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// close ethernet device
	system("ifconfig eth0 0.0.0.0");
	system("ifconfig eth0 down");
	// unbind ethernet driver
	if (device_detach_driver(ETHERNET_DEVICE, ETHERNET_DRIVER) != RK_SUCCESS)
		goto __FAILED;
	// wait for unbind done
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		ret = read(fd, buf, sizeof(buf));
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, ret); nh = NLMSG_NEXT(nh, ret)) {
			if (nh->nlmsg_type == NLMSG_DONE) {
				printf("[%s()] NLMSG_DONE\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type == NLMSG_ERROR) {
				printf("[%s()] NLMSG_ERROR\n", __func__);
				goto __FAILED;
			}
			ifinfo = NLMSG_DATA(nh);
			if_indextoname(ifinfo->ifi_index, name);

			// printf("[%s()] %s: up %s, lower up %s, running %s, value 0X%X\n", __func__
			// 	, name
			// 	, (ifinfo->ifi_flags & IFF_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_LOWER_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_RUNNING) ? "true" : "false"
			// 	, ifinfo->ifi_flags
			// );

			if (!(ifinfo->ifi_flags & IFF_RUNNING) &&
			    !(ifinfo->ifi_flags & IFF_LOWER_UP) && !(ifinfo->ifi_flags & IFF_UP)) {
				printf("[%s()] ethernet unbind success!\n", __func__);
				goto __SUCCESS;
			}
		}
		// printf("[%s()] Bind msg: %s\n", __func__, buf);
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error: %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	// open etherner device
	printf("[%s()] Exit\n", __func__);
	return RK_SUCCESS;
__FAILED:
	close(fd);
	printf("[%s()] Exit Error\n", __func__);
	return RK_FAILURE;
}

#define SDCARD_DEVICE "ffaa0000.mmc"
#define SDCARD_DRIVER_PREPARED "bind@/devices/platform/ffaa0000.mmc"
#define SDCARD_BIND_DONE "bind@/devices/platform/ffaa0000.mmc/mmc_host/mmc1/mmc1"
#define SDCARD_UNBIND_DONE "unbind@/devices/platform/ffaa0000.mmc"
#define SDCARD_DRIVER "/sys/bus/platform/drivers/dwmmc_rockchip"

static bool detect_sdcard_is_enable() {
	int value = 0;
	if (SAMPLE_COMM_ReadReg(0xffaa0050, (RK_U8 *)&value, sizeof(value)) != RK_SUCCESS)
		return RK_FALSE;
	if (value == 0)
		return RK_TRUE;
	else
		return RK_FALSE;
}

int SAMPLE_COMM_AOV_BindSdcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (device_driver_is_bound(SDCARD_DEVICE, SDCARD_DRIVER)) {
		printf("[%s()] sdcard device already bind!\n", __func__);
		return RK_SUCCESS;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open sdcard netlink because %s\n", __func__,
		       strerror(errno));
		return RK_FAILURE;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind sdcard netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// bind sdcard
	if (device_attach_driver(SDCARD_DEVICE, SDCARD_DRIVER) != RK_SUCCESS)
		goto __FAILED;
	// wait for bind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] bind msg: %s\n", __func__, buf);
		if (strncmp(buf, SDCARD_DRIVER_PREPARED, strlen(SDCARD_DRIVER_PREPARED)) == 0) {
			// printf("[%s()] sdcard driver is prepared\n", __func__);
			if (detect_sdcard_is_enable()) {
				printf("[%s()] sdcard is enable\n", __func__);
			} else {
				printf("[%s()] sdcard is disable\n", __func__);
				goto __FAILED;
			}
		}
		if (strncmp(buf, SDCARD_BIND_DONE, strlen(SDCARD_BIND_DONE)) == 0) {
			printf("[%s()] Bind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return RK_SUCCESS;
__FAILED:
	close(fd);
	return RK_FAILURE;
}

int SAMPLE_COMM_AOV_UnbindSdcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (!device_driver_is_bound(SDCARD_DEVICE, SDCARD_DRIVER)) {
		printf("[%s()] sdcard device already unbind!\n", __func__);
		return RK_SUCCESS;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open sdcard netlink because %s\n", __func__,
		       strerror(errno));
		return RK_FAILURE;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind sdcard netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// unbind sdcard
	if (device_detach_driver(SDCARD_DEVICE, SDCARD_DRIVER) != RK_SUCCESS)
		goto __FAILED;
	// wait for unbind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] unbind msg: %s\n", __func__, buf);
		if (strcmp(buf, SDCARD_UNBIND_DONE) == 0) {
			printf("[%s()] Unbind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return RK_SUCCESS;
__FAILED:
	close(fd);
	return RK_FAILURE;
}

#define MAX_LINE_SIZE 256
static int SAMPLE_COMM_AOV_CheckSDcardMount(void) {
	int fd = open("/proc/mounts", O_RDONLY);
	int ret = -1;
	if (fd == -1) {
		perror("Error opening /proc/mounts");
		return ret;
	}
	char line[MAX_LINE_SIZE];
	ssize_t bytesRead;
	int pos = 0;
	while ((bytesRead = read(fd, &line[pos], 1)) > 0) {
		if (line[pos] == '\n') {
			line[pos] = '\0'; // Null-terminate the string
			if (strstr(line, "/mnt/sdcard")) {
				printf("Found '/mnt/sdcard' in line: %s", line);
				ret = 0;
				break; // No need to continue searching
			}
			pos = 0; // Reset position for next line
		} else {
			pos++;
			if (pos >= MAX_LINE_SIZE - 1) {
				// Line exceeds buffer size, discard it
				pos = 0;
			}
		}
	}
	close(fd);
	return ret;

} /* -----  end of function SAMPLE_COMM_AOV_CheckSDcardMount  ----- */

#define MOUNT_DEV_1 "/dev/mmcblk1p1"
#define MOUNT_DEV_2 "/dev/mmcblk1"

int SAMPLE_COMM_AOV_CopyRawStreamToSdcard(int venc_chn_id, char *data, int data_size,
                                          char *data2, int data2_size) {
	int ret = 0;
	pthread_mutex_lock(&g_wakeup_run_mutex);
	SAMPLE_COMM_AOV_BindSdcard();
	// mount sd
	if (access(MOUNT_DEV_1, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1p1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_1, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else if (access(MOUNT_DEV_2, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_2, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else {
		printf("[%s()] bad mount path!\n", __func__);
		// goto SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end;
	}

	if (0 != SAMPLE_COMM_AOV_CheckSDcardMount()) {
		printf("Not found mount sdcard on /mnt/sdcard\n");
		goto SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end;
	}

	static int count_t = 0;
	static char dstPath[256];
	int s32fd = 0;
	char dst[256];
	// 首次进来存储，遍历目录是否可用，最多100个目录
	if (count_t == 0) {
		int i = 0;
		for (i = 0; i < 100; i++) {
			sprintf(dstPath, "/mnt/sdcard/wakeup_frame_%d", i);
			struct stat st;
			if (stat(dstPath, &st) == -1) {
				if (mkdir(dstPath, 0777) == -1)
					printf("mkdir %s failed\n", dstPath);
				break;
			}
		}
	}
	sprintf(dst, "%s/venc_chn%d_%d.h265", dstPath, venc_chn_id, count_t);
	FILE *fp = fopen(dst, "wb");
	if (fp != NULL) {
		fwrite(data, 1, data_size, fp);
		if (data2)
			fwrite(data2, 1, data2_size, fp);
		s32fd = fileno(fp);
		fsync(s32fd);
		fclose(fp);
		count_t++;
	}
	// unmount sdcard
	umount2("/mnt/sdcard", MNT_DETACH);

SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end:
	SAMPLE_COMM_AOV_UnbindSdcard();
	pthread_mutex_unlock(&g_wakeup_run_mutex);

	return RK_SUCCESS;
}

static void *aenc_get_stream(void *pArgs) {
	SAMPLE_AENC_CTX_S aenc;
	SAMPLE_AI_CTX_S ai;
	RK_S32 s32Ret;
	RK_S32 s32MilliSec = 1000;
	RK_U64 time_diff;
	void *pdata = NULL;
	MPP_CHN_S stSrcChn, stDestChn;
	struct timespec boot_time, mono_time;
	SAMPLE_MPI_MUXER_S *pstMuxer = pArgs;

	RK_LOGD("Enter");
	memset(&ai, 0, sizeof(SAMPLE_AI_CTX_S));
	memset(&aenc, 0, sizeof(SAMPLE_AENC_CTX_S));

	clock_gettime(CLOCK_MONOTONIC, &mono_time);
	clock_gettime(CLOCK_BOOTTIME, &boot_time);
	time_diff = ((boot_time.tv_nsec / 1000) + (RK_U64)boot_time.tv_sec * 1000 * 1000) -
	            ((mono_time.tv_nsec / 1000) + (RK_U64)mono_time.tv_sec * 1000 * 1000);
	RK_LOGD("boot time %d(s) %ld(ns), mono time %d(s) %ld(ns)", boot_time.tv_sec,
	        boot_time.tv_nsec, mono_time.tv_sec, mono_time.tv_nsec);

	SAMPLE_COMM_AI_SetAttr(&ai);
	s32Ret = SAMPLE_COMM_AI_CreateChn(&ai);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("create aenc channel error %#X!", s32Ret);
		return NULL;
	}

	SAMPLE_COMM_AENC_SetAttr(&aenc);
	SAMPLE_COMM_AENC_CreateChn(&aenc);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("create aenc channel error %#X!", s32Ret);
		SAMPLE_COMM_AI_DestroyChn(&ai);
		return NULL;
	}

	stSrcChn.enModId = RK_ID_AI;
	stSrcChn.s32DevId = 0;
	stSrcChn.s32ChnId = ai.s32ChnId;
	stDestChn.enModId = RK_ID_AENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = aenc.s32ChnId;
	SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

	while (true) {
		s32Ret = RK_MPI_AENC_GetStream(aenc.s32ChnId, &aenc.stFrame, s32MilliSec);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_AENC_GetStream failed %#X", s32Ret);
			continue;
		}
		pdata = RK_MPI_MB_Handle2VirAddr(aenc.stFrame.pMbBlk);

		pthread_mutex_lock(&pstMuxer->stMutex);
		if (!pstMuxer->bIsRecordingMp4) {
			pthread_mutex_unlock(&pstMuxer->stMutex);
			RK_MPI_AENC_ReleaseStream(aenc.s32ChnId, &aenc.stFrame);
			RK_LOGE("mp4 recording is stopped, exit thread");
			break;
		}
		if ((pstMuxer->s32VideoDataSize + pstMuxer->s32AudioDataSize +
		     aenc.stFrame.u32Len) > AOV_STREAM_SIZE_WRITE_TO_SDCARD ||
		    pstMuxer->s32AudioFrmCnt >= AUDIOFRAMECNT) {
			RK_LOGE("audio count(%d) or size(%d) is too small", pstMuxer->s32AudioFrmCnt,
			        AOV_STREAM_SIZE_WRITE_TO_SDCARD);
			pthread_mutex_unlock(&pstMuxer->stMutex);
			RK_MPI_AENC_ReleaseStream(aenc.s32ChnId, &aenc.stFrame);
			continue;
		}
		// VENC use boot time to generate PTS, and AENC use monotic time
		// Use time_diff to fix difference of PTS between aenc frame and venc frame
		SAMPLE_COMM_MuxerAFrame(pstMuxer, aenc.stFrame.u32Len,
		                        aenc.stFrame.u64TimeStamp + time_diff, pdata);
		pthread_mutex_unlock(&pstMuxer->stMutex);

		RK_LOGD("[MP4] audio frame len %d, pts %lld", aenc.stFrame.u32Len,
		        aenc.stFrame.u64TimeStamp + time_diff);
		RK_MPI_AENC_ReleaseStream(aenc.s32ChnId, &aenc.stFrame);
	}

	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);
	SAMPLE_COMM_AENC_DestroyChn(&aenc);
	SAMPLE_COMM_AI_DestroyChn(&ai);
	RK_LOGD("Exit");
	return NULL;
}

static int save_mp4_to_sdcard_unlocked(SAMPLE_MPI_MUXER_S *pstMuxer) {
	int ret;

	pthread_mutex_lock(&g_wakeup_run_mutex);
	SAMPLE_COMM_AOV_BindSdcard();
	// mount sd
	if (access(MOUNT_DEV_1, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1p1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_1, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else if (access(MOUNT_DEV_2, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_2, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else {
		printf("[%s()] bad mount path!\n", __func__);
		// goto SAMPLE_COMM_AOV_CopyStreamToSdcard_end;
	}

	if (0 != SAMPLE_COMM_AOV_CheckSDcardMount()) {
		printf("Not found mount sdcard on /mnt/sdcard\n");
		goto __copy_mp4_stream_end;
	}

	static int count_t = 0;
	static char dstPath[256];
	int s32fd = 0;
	char dstMuxer[256];
	FILE *fpMuxer = NULL;
	// 首次进来存储，遍历目录是否可用，最多100个目录
	if (count_t == 0) {
		int i = 0;
		for (i = 0; i < 100; i++) {
			sprintf(dstPath, "/mnt/sdcard/wakeup_video_%d", i);
			struct stat st;
			if (stat(dstPath, &st) == -1) {
				if (mkdir(dstPath, 0777) == -1)
					printf("mkdir %s failed\n", dstPath);
				break;
			}
		}
	}

	if (pstMuxer) {
		sprintf(dstMuxer, "%s/chn_%d_count_%d.mp4", dstPath, pstMuxer->s32VencChnId,
		        count_t);
		fpMuxer = fopen(dstMuxer, "wb");

		if (fpMuxer) {
			if (rkmuxer_init(0, "mp4", dstMuxer, &pstMuxer->stVideoParam,
			                 &pstMuxer->stAudioParam)) {
				RK_LOGE("rkmuxer_init failed");
				fclose(fpMuxer);
				goto __copy_mp4_stream_end;
			}

			SAMPLE_COMM_MuxerWrite(pstMuxer);
			s32fd = fileno(fpMuxer);
			fsync(s32fd);
			rkmuxer_deinit(0);
			fclose(fpMuxer);
			count_t++;
		} else {
			RK_LOGE("fopen rkmuxer failed");
			goto __copy_mp4_stream_end;
		}
	}
	// unmount sdcard
	umount2("/mnt/sdcard", MNT_DETACH);

__copy_mp4_stream_end:
	SAMPLE_COMM_AOV_UnbindSdcard();
	pthread_mutex_unlock(&g_wakeup_run_mutex);

	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_InitMp4(SAMPLE_VENC_CTX_S *ctx, SAMPLE_MPI_MUXER_S **ppstMuxer) {
	int ret = RK_SUCCESS;
	if (ctx == NULL || ppstMuxer == NULL) {
		RK_LOGE("bad parameter ctx %p, ppstMuxer %p", ctx, ppstMuxer);
		return RK_FAILURE;
	}
	// Allocate cache buffer.
	ret = SAMPLE_COMM_MuxerInit(ppstMuxer, ctx);
	if (ret != RK_SUCCESS) {
		RK_LOGE("SAMPLE_COMM_MuxerInit failed");
		return ret;
	}
	pthread_mutex_init(&(*ppstMuxer)->stMutex, NULL);
	(*ppstMuxer)->s32VencChnId = ctx->s32ChnId;
	return ret;
}

int SAMPLE_COMM_AOV_DeinitMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}
	if (pstMuxer->bIsRecordingMp4)
		SAMPLE_COMM_AOV_StopRecordMp4(pstMuxer);

	// Save stream to sdcard when exit.
	pthread_mutex_lock(&pstMuxer->stMutex);
	save_mp4_to_sdcard_unlocked(pstMuxer);
	pthread_mutex_unlock(&pstMuxer->stMutex);

	// Release cache buffer.
	SAMPLE_COMM_MuxerDeinit(&pstMuxer);
	pthread_mutex_destroy(&pstMuxer->stMutex);
}

int SAMPLE_COMM_AOV_StartRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
	RK_S32 s32Ret = RK_SUCCESS;
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}
	RK_LOGI("[MP4] start record mp4");
	// Attach sound card driver.
	SAMPLE_COMM_AOV_BindSoundcard();
	// Start capture audio frame and video frame asynchronously.
	pstMuxer->bIsRecordingMp4 = true;
	pthread_create(&pstMuxer->s32AencThreadId, 0, aenc_get_stream, pstMuxer);
	// First video frame send to rkmuxer must be I-frame.
	RK_MPI_VENC_RequestIDR(pstMuxer->s32VencChnId, RK_FALSE);

	return s32Ret;
}

bool SAMPLE_COMM_AOV_IsRecordingMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FALSE;
	}
	return pstMuxer->bIsRecordingMp4;
}

int SAMPLE_COMM_AOV_StopRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}
	// Aenc thread may running asynchronously.
	pstMuxer->bIsRecordingMp4 = false;

	// Unlock, wait for aenc thread exit.
	pthread_join(pstMuxer->s32AencThreadId, NULL);

	// Detach sound card driver.
	SAMPLE_COMM_AOV_UnbindSoundcard();
	RK_MPI_VENC_RequestIDR(pstMuxer->s32VencChnId, RK_FALSE);
	RK_LOGI("[MP4] end record mp4");
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_CopyMp4StreamToSdcard(SAMPLE_MPI_MUXER_S *pstMuxer,
                                          VENC_STREAM_S *frame, void *pData) {
	int ret = 0;
	bool need_save_sd = false;
	int key_frame = 0;

	if (frame == NULL || pData == NULL || pstMuxer == NULL) {
		RK_LOGE("error parameter frame %p, pData %p, pstMuxer %p", frame, pData,
		        pstMuxer);
		return RK_FAILURE;
	}

	pthread_mutex_lock(&pstMuxer->stMutex);

	RK_LOGD("[MP4] video frame len:%u, pts:%llu, seq:%u", frame->pstPack->u32Len,
	        frame->pstPack->u64PTS, frame->u32Seq);

	if ((pstMuxer->s32VideoDataSize + pstMuxer->s32AudioDataSize +
	     frame->pstPack->u32Len) > AOV_STREAM_SIZE_WRITE_TO_SDCARD ||
	    pstMuxer->s32VideoFrmCnt >= VIDEOFRAMECNT) {
		RK_LOGE("video count(%d) or size(%d) is too small", pstMuxer->s32AudioFrmCnt,
		        AOV_STREAM_SIZE_WRITE_TO_SDCARD);
		need_save_sd = true;
	}

	if (need_save_sd) {
		RK_LOGI("save mp4 stream to sdcard");
		save_mp4_to_sdcard_unlocked(pstMuxer);
		// A MP4 stream is start with a I frame, so request I frame
		// after a stream end.
		RK_MPI_VENC_RequestIDR(pstMuxer->s32VencChnId, RK_FALSE);
	}

	if (frame->pstPack->DataType.enH264EType == H264E_NALU_PSLICE ||
	    frame->pstPack->DataType.enH265EType == H265E_NALU_PSLICE)
		key_frame = 0;
	else
		key_frame = 1;

	SAMPLE_COMM_MuxerVFrame(pstMuxer, frame->pstPack->u32Len, frame->pstPack->u64PTS,
	                        key_frame, pData);
	if (!pstMuxer->bIsRecordingMp4) {
		RK_LOGD("[MP4] send dummp audio frame");
		SAMPLE_COMM_MuxerMuteAFrame(pstMuxer);
	}

done:
	pthread_mutex_unlock(&pstMuxer->stMutex);

	return RK_SUCCESS;
}

void SAMPLE_COMM_AOV_DumpPtsToTMP(uint32_t seq, uint64_t pts, int max_dump_pts_count) {
	static int line_count = 0;
	static FILE *file;
	const char *file_path = "/tmp/pts.txt";

	if (line_count >= max_dump_pts_count) {
		return;
	}

	if (line_count == 0) {
		file = fopen(file_path, "w");
		if (file == NULL) {
			perror("Error opening file");
			return;
		}
	}

	if (file != NULL)
		fprintf(file, "seq: %u, pts: %llums\n", seq, (unsigned long long)pts / 1000);

	line_count++;

	if (line_count >= max_dump_pts_count) {
		printf("Closed file after writing %d lines.\n", max_dump_pts_count);
		fclose(file);
		file = NULL;
	}
}

// Return true is there has input event happened.
bool SAMPLE_COMM_AOV_GetGpioIrqStat() {
	bool input_event_happened = false;
	struct input_event event;

	if (g_input_device_fd < 0) {
		// printf("[%s()] unsupport input event detector\n", __func__);
		return input_event_happened;
	}

	// The read() operation is non-block, so the loop would
	// return EGAGIN if there has no any new input event.
	while (read(g_input_device_fd, &event, sizeof(event)) > 0) {
		// printf("[%s()] detect event type %d, code %d, value %d\n"
		// 		, __func__
		// 		, event.type
		// 		, event.code
		// 		, event.value
		// 		);
		// Translate ISP state if gpio-key is pressed between
		// sleeping time.
		if (event.type == EV_KEY && event.code == KEY_POWER && event.value == 1)
			input_event_happened = true;
	}
	return input_event_happened;
}

#define I2S_DRIVER "/sys/bus/platform/drivers/rockchip-i2s-tdm/"
#define ACODEC_DRIVER "/sys/bus/platform/drivers/rv1106-acodec/"
#define ASOC_DRIVER "/sys/bus/platform/drivers/asoc-simple-card/"
#define I2S_DEVICE "ffae0000.i2s"
#define ACODEC_DEVICE "ff480000.acodec"
#define ASOC_DEVICE "acodec-sound"
#define SOUND_BIND_DONE "bind@/devices/platform/acodec-sound"
#define SOUND_UNBIND_DONE "unbind@/devices/platform/ffae0000.i2s"

int SAMPLE_COMM_AOV_BindSoundcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open netlink because %s\n", __func__, strerror(errno));
		return RK_FAILURE;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind netlink addr failed because %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	ret = device_attach_driver(I2S_DEVICE, I2S_DRIVER);
	ret |= device_attach_driver(ACODEC_DEVICE, ACODEC_DRIVER);
	ret |= device_attach_driver(ASOC_DEVICE, ASOC_DRIVER);
	if (ret != RK_SUCCESS)
		goto __FAILED;
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] msg: %s\n", __func__, buf);
		if (strncmp(buf, SOUND_BIND_DONE, strlen(SOUND_BIND_DONE)) == 0) {
			printf("[%s()] Bind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

__SUCCESS:
	close(fd);
	return RK_SUCCESS;
__FAILED:
	close(fd);
	return RK_FAILURE;
}

int SAMPLE_COMM_AOV_UnbindSoundcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open netlink because %s\n", __func__, strerror(errno));
		return RK_FAILURE;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind netlink addr failed because %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	ret = device_detach_driver(I2S_DEVICE, I2S_DRIVER);
	ret |= device_detach_driver(ACODEC_DEVICE, ACODEC_DRIVER);
	ret |= device_detach_driver(ASOC_DEVICE, ASOC_DRIVER);
	if (ret != RK_SUCCESS)
		goto __FAILED;
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] msg: %s\n", __func__, buf);
		if (strncmp(buf, SOUND_UNBIND_DONE, strlen(SOUND_UNBIND_DONE)) == 0) {
			printf("[%s()] Unbind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

__SUCCESS:
	close(fd);
	return RK_SUCCESS;
__FAILED:
	close(fd);
	return RK_FAILURE;
}

int SAMPLE_COMM_AOV_BindEmmc() { return RK_SUCCESS; }
int SAMPLE_COMM_AOV_UnbindEmmc() { return RK_SUCCESS; }
int SAMPLE_COMM_AOV_DisableNonBootCPUs() { return RK_SUCCESS; }
int SAMPLE_COMM_AOV_EnableNonBootCPUs() { return RK_SUCCESS; }
