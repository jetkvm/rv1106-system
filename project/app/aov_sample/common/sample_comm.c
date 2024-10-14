/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "sample_comm.h"

#define RUN_FRAME_NUM (100)
void PrintStreamDetails(int chnId, int framesize) {
	static int strmfrmCnt = 0;
	static int sumframesize = 0;
	static struct timeval startTime, endTime, passTime;
	double calcTime;

	sumframesize += framesize;

	if (strmfrmCnt == 0)
		gettimeofday(&startTime, NULL);
	if (strmfrmCnt == RUN_FRAME_NUM) {
		gettimeofday(&endTime, NULL);
		printf("\n================= CH%d STREAMING DETAILS ==================\n", chnId);
		printf("Start Time : %ldsec %06ldusec\n", (long)startTime.tv_sec,
		       (long)startTime.tv_usec);
		printf("End Time   : %ldsec %06ldusec\n", (long)endTime.tv_sec,
		       (long)endTime.tv_usec);
		timersub(&endTime, &startTime, &passTime);
		calcTime = (double)passTime.tv_sec * 1000.0 + (double)passTime.tv_usec / 1000.0;
		printf("Total Time to stream %d frames: %.3f msec TotalBytes/sec: %.3f "
		       "Mbps\n",
		       RUN_FRAME_NUM, calcTime,
		       ((float)sumframesize * 8 * 1000) / calcTime / 1024 / 1024);
		printf("Time per frame: %3.4f msec\n", calcTime / RUN_FRAME_NUM);
		printf("Streaming Performance in FPS: %3.4f\n",
		       RUN_FRAME_NUM / (calcTime / 1000));
		// if(RUN_FRAME_NUM/(calcTime/1000) > 31) {
		//    printf("FPS error!!!\n");
		//}
		printf("===========================================================\n");
		strmfrmCnt = 0;
		sumframesize = 0;
	} else {
		strmfrmCnt++;
	}
}

RK_S32 SAMPLE_COMM_Bind(const MPP_CHN_S *pstSrcChn, const MPP_CHN_S *pstDestChn) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_SYS_Bind(pstSrcChn, pstDestChn);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_SYS_Bind failed with %#x!\n", s32Ret);
		return s32Ret;
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_UnBind(const MPP_CHN_S *pstSrcChn, const MPP_CHN_S *pstDestChn) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_SYS_UnBind(pstSrcChn, pstDestChn);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_SYS_UnBind failed with %#x!\n", s32Ret);
		return s32Ret;
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_GetBmpResolution(RK_CHAR *pBmpFile, RK_U32 *width, RK_U32 *height) {

	FILE *fp = RK_NULL;
	RK_U16 bfType;
	OSD_BITMAPINFO pBmpInfo;

	*width = 256;
	*height = 256;
	if (pBmpFile == RK_NULL) {
		RK_LOGE("bmp file not exist");
		return RK_FAILURE;
	}

	fp = fopen(pBmpFile, "rb");
	if (fp == RK_NULL) {
		RK_LOGE("open file:%s failure", pBmpFile);
		return RK_FAILURE;
	}

	(void)fread(&bfType, 1, sizeof(bfType), fp);
	if (bfType != 0x4d42) {
		RK_LOGE("is not bmp file");
		fclose(fp);
		return RK_FAILURE;
	}
	fseek(fp, sizeof(OSD_BITMAPFILEHEADER), SEEK_CUR);
	(void)fread(&pBmpInfo, 1, sizeof(OSD_BITMAPINFO), fp);

	*width = pBmpInfo.bmiHeader.biWidth;
	*height = pBmpInfo.bmiHeader.biHeight;
	RK_LOGE("SAMPLE_GET_bmpResolution w:%d  h:%d", *width, *height);

	if (fp) {
		fclose(fp);
		fp = RK_NULL;
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_DumpMeminfo(RK_CHAR *callFunc, RK_S32 moduleTestType) {
	char systemCmd[256];
	system("echo 3 > /proc/sys/vm/drop_caches");
	sprintf(systemCmd, "echo %s %d >> /tmp/testLog.txt", callFunc, moduleTestType);
	system(systemCmd);
	system("cat /proc/meminfo | grep MemAvailable >> /tmp/testLog.txt");
	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_GetLdchMesh(RK_CHAR *cam0LdchPath, RK_CHAR *cam1LdchPath,
                               RK_S32 s32MeshDataSize, RK_U16 **pLdchMesh) {
	FILE *fpCam0 = RK_NULL;
	FILE *fpCam1 = RK_NULL;

	fpCam0 = fopen(cam0LdchPath, "rb");
	fpCam1 = fopen(cam1LdchPath, "rb");
	if (!fpCam0 || !fpCam1) {
		RK_LOGE("open %s or %s failure fpCam0:%p fpCam1:%p", cam0LdchPath, cam1LdchPath,
		        fpCam0, fpCam1);
		goto READ_FILE_FAIL;
	}

	if (s32MeshDataSize != fread(pLdchMesh[0], 1, s32MeshDataSize, fpCam0) ||
	    s32MeshDataSize != fread(pLdchMesh[1], 1, s32MeshDataSize, fpCam1)) {
		RK_LOGE("fread %s or %s data failure", cam0LdchPath, cam1LdchPath);
		goto READ_FILE_FAIL;
	}

	if (fpCam0 && fpCam1) {
		fclose(fpCam0);
		fclose(fpCam1);
		fpCam0 = RK_NULL;
		fpCam1 = RK_NULL;
	}

	return RK_SUCCESS;

READ_FILE_FAIL:
	if (fpCam0) {
		fclose(fpCam0);
		fpCam0 = RK_NULL;
	}
	if (fpCam1) {
		fclose(fpCam1);
		fpCam1 = RK_NULL;
	}
	return RK_FAILURE;
}

RK_VOID SAMPLE_COMM_CheckFd(RK_BOOL bStart) {
	static RK_BOOL bQuiltCheckFd = RK_FALSE;
	static pthread_t checkFd_thread_id;
	if (bStart) {
		if (bQuiltCheckFd)
			return;
		else
			bQuiltCheckFd = RK_TRUE;
	} else {
		if (bQuiltCheckFd) {
			bQuiltCheckFd = RK_FALSE;
			if (pthread_join(checkFd_thread_id, NULL) != 0) {
				printf("Failed to join thread!\n");
			}
		}
		return;
	}

	void *checkFd(void *arg) {
		int fd[3];
		prctl(PR_SET_NAME, "check_fd_thread");
		srand(time(NULL));
		while (bQuiltCheckFd) {
			for (int i = 0; i < 3; i++) {
				fd[i] = open("/dev/null", O_RDONLY);
				if (fd[i] == -1) {
					printf("Error opening file descriptor!\n");
				}
			}
			unsigned int delay = rand() % 100;
			usleep(delay * 1000);
			for (int i = 0; i < 3; i++) {
				if (close(fd[i]) == -1) {
					printf("Error closing file descriptor!\n");
					abort();
				}
			}
			usleep(delay * 1000);
		}
		return NULL;
	}

	if (pthread_create(&checkFd_thread_id, NULL, checkFd, NULL) != 0) {
		printf("Failed to create thread!\n");
		return;
	}
}

RK_S32 SAMPLE_COMM_ECHO(RK_CHAR *file_path, RK_CHAR *buf, RK_U32 length) {
	int fd = -1;
	ssize_t ret = -1;

	fd = open(file_path, O_WRONLY | O_TRUNC);
	if (fd == -1) {
		perror("open error\n");
		return -1;
	}

	ret = write(fd, buf, length);
	if (ret == -1) {
		perror("write error\n");
		close(fd);
		return -1;
	}

	RK_LOGD("echo \"%s\" > %s successfully\n", buf, file_path);

	close(fd);

	return 0;
}

#if _SAMPLE_AOV_ENABLE_KLOG_
RK_VOID SAMPLE_COMM_KLOG(const RK_CHAR *log) {
	int fd = open("/dev/kmsg", O_WRONLY | O_APPEND);
	if (fd != -1) {
		dprintf(fd, "[app]: %s\n", log);
		close(fd);
	}
}
#endif

#define MAX_CMDLINE_SIZE 4096
RK_S32 SAMPLE_COMM_ExtractValueFromCmdline(const char *param) {
	int cmdline_file = open("/proc/cmdline", O_RDONLY);
	if (cmdline_file == -1) {
		perror("Error opening /proc/cmdline");
		return -1;
	}

	char cmdline[MAX_CMDLINE_SIZE];
	ssize_t bytesRead = read(cmdline_file, cmdline, sizeof(cmdline));
	if (bytesRead == -1) {
		perror("Error reading /proc/cmdline");
		close(cmdline_file);
		return -1;
	}
	cmdline[bytesRead] = '\0'; // Null-terminate the string

	close(cmdline_file);

	RK_S32 value = -1;

	// Find the position of the parameter in the string
	const char *param_str = strstr(cmdline, param);
	if (param_str != NULL) {
		sscanf(param_str, "%*[^=]=%d", &value);
	}

	return value;
}

static void copyOrAppendFile(const char *sourcePath, const char *destinationPath,
                             const char *mode) {
	if (sourcePath == NULL || destinationPath == NULL || mode == NULL) {
		return;
	}

	int sourceFile = open(sourcePath, O_RDONLY);
	if (sourceFile == -1) {
		perror("Error opening source file");
		return;
	}

	int destinationFile =
	    open(destinationPath,
	         O_WRONLY | O_CREAT | (strcmp(mode, "a") == 0 ? O_APPEND : O_TRUNC),
	         S_IRUSR | S_IWUSR);
	if (destinationFile == -1) {
		perror("Error opening destination file");
		close(sourceFile);
		return;
	}

	dprintf(destinationFile, "============ %s ============\n", sourcePath);

	char buffer[1024];
	ssize_t bytesRead;

	while ((bytesRead = read(sourceFile, buffer, sizeof(buffer))) > 0) {
		ssize_t bytesWritten = write(destinationFile, buffer, bytesRead);
		if (bytesWritten != bytesRead) {
			perror("Error writing to destination file");
			close(sourceFile);
			close(destinationFile);
			return;
		}
	}

	close(sourceFile);
	close(destinationFile);
}

RK_S32 SAMPLE_COMM_DumpDebugInfoToTmp(RK_VOID) {
	char *debugRockit = getenv("debug_rockit");
	if (debugRockit == NULL) {
		return RK_FAILURE;
	}
	copyOrAppendFile("/proc/rkcif-mipi-lvds", "/tmp/debugInfo.txt", "w");
	copyOrAppendFile("/proc/rkcif-mipi-lvds1", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/proc/rkisp-vir0", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/proc/rkisp-vir1", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/dev/mpi/vsys", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/dev/mpi/valloc", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/dev/mpi/vlog", "/tmp/debugInfo.txt", "a");
	copyOrAppendFile("/proc/vcodec/enc/venc_info", "/tmp/debugInfo.txt", "a");
	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_WriteReg(RK_S32 addr, RK_S32 value) {
	int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("Error opening /dev/mem");
		return -1;
	}

	// 获取页大小
	size_t page_size = getpagesize();

	// 计算页对齐的地址和偏移量
	off_t page_base = (addr & ~(page_size - 1));
	off_t page_offset = addr - page_base;

	// 映射内存
	void *mem_map =
	    mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, page_base);
	if (mem_map == MAP_FAILED) {
		perror("Error mapping memory");
		close(mem_fd);
		return -1;
	}

	// 计算寄存器地址
	int *target_reg = (int *)((char *)mem_map + page_offset);

	// 写入数据
	*target_reg = value;

	// 解除内存映射
	munmap(mem_map, page_size);
	close(mem_fd);

	return 0;
}

RK_S32 SAMPLE_COMM_ReadReg(RK_S32 addr, RK_U8 *buf, RK_U32 len) {
	int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("Error opening /dev/mem");
		return RK_FAILURE;
	}

	// 获取页大小
	size_t page_size = getpagesize();
	if (len > page_size) {
		printf("Dump length is too long!\n");
		return RK_FAILURE;
	}

	// 计算页对齐的地址和偏移量
	off_t page_base = (addr & ~(page_size - 1));
	off_t page_offset = addr - page_base;

	// 映射内存
	void *mem_map =
	    mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, page_base);
	if (mem_map == MAP_FAILED) {
		perror("Error mapping memory");
		close(mem_fd);
		return RK_FAILURE;
	}

	// 计算寄存器地址
	const RK_U8 *target_reg = (RK_U8 *)((char *)mem_map + page_offset);

	// 读取数据
	memcpy(buf, target_reg, len);

	// 解除内存映射
	munmap(mem_map, page_size);
	close(mem_fd);

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_DumpReg(RK_S32 addr, RK_U32 len) {
	RK_U8 *data;
	data = (RK_U8 *)malloc(len);
	if (!data) {
		printf("malloc failed!\n");
		return RK_FAILURE;
	}
	memset(data, 0, len);
	if (SAMPLE_COMM_ReadReg(addr, data, len) != RK_SUCCESS) {
		free(data);
		printf("read register file failed!\n");
		return RK_FAILURE;
	}
	for (RK_U32 i = 0; i < len; ++i)
		printf("[DUMP REG] reg %#X value %#X\n", addr + i, data[i]);
	free(data);
	return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
