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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

#include "rtsp_demo.h"
#include "sample_comm.h"
#include "sample_comm_aov.h"

static sem_t g_iva_semaphore;
static RK_S32 g_enable_sleep = 1;
static SAMPLE_IVA_CTX_S iva_ctx;
static char *path = NULL;
static RK_S32 loop_count = 0;

static bool quit = false;
static int quit_result = RK_SUCCESS;

static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	quit = true;
	quit_result = RK_SUCCESS;
}

static void program_handle_error(const char *func, RK_U32 line) {
	printf("func: <%s> line: <%d> error exit!", func, line);
	quit = true;
	quit_result = RK_FAILURE;
}

static void program_normal_exit(const char *func, RK_U32 line) {
	printf("func: <%s> line: <%d> normal exit!", func, line);
	quit = true;
	quit_result = RK_SUCCESS;
}

static RK_CHAR optstr[] = "?:w:h:l:p:r:e:s:";
static const struct option long_options[] = {
    {"loop_count", required_argument, NULL, 'l'},
    {"path", required_argument, NULL, 'p'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"framerate", optional_argument, NULL, 'r'},
    {"enable_sleep", optional_argument, NULL, 'e' + 's'},
    {"help", optional_argument, NULL, '?'},
    {NULL, 0, NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("usage example:\n");
	printf("\t%s -w 720 -h 480 -l 1 -p /mnt/sdcard/test_image.yuv\n", name);
	printf("\t-w | --width: input image with, Default 720\n");
	printf("\t-h | --height: input image height, Default 480\n");
	printf("\t-l | --loop_count: test loop count, 1\n");
	printf("\t-p | --path: input image path, Default NULL\n");
	printf("\t-r | --framerate: iva detect framerate, Default 10\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--enable_sleep: enable enter sleep, Default: 1\n");
}

static void rkIvaEvent_callback(const RockIvaDetectResult *result,
                                const RockIvaExecuteStatus status, void *userData) {

	RK_LOGI("objnum %d, status %d", result->objNum, status);
	for (int i = 0; i < result->objNum; i++) {
		RK_LOGI("topLeft:[%d,%d], bottomRight:[%d,%d],"
		        "objId is %d, frameId is %d, score is %d, type is %d\n",
		        result->objInfo[i].rect.topLeft.x, result->objInfo[i].rect.topLeft.y,
		        result->objInfo[i].rect.bottomRight.x,
		        result->objInfo[i].rect.bottomRight.y, result->objInfo[i].objId,
		        result->objInfo[i].frameId, result->objInfo[i].score,
		        result->objInfo[i].type);
	}
}

static void rkIvaFrame_releaseCallBack(const RockIvaReleaseFrames *releaseFrames,
                                       void *userdata) {
	/* when iva handle out of the video frame，this func will be called*/
	RK_LOGD("release iva frame success!");
	sem_post(&g_iva_semaphore);
}

static void *send_frame_to_iva_thread(void *pArgs) {
	prctl(PR_SET_NAME, "send_frame_to_iva_thread");
	RK_S32 s32Ret = RK_FAILURE;
	RK_S32 input_file_fd = path ? open(path, O_RDONLY) : -1;
	RockIvaImage input_image;
	RK_U32 size = iva_ctx.u32ImageWidth * iva_ctx.u32ImageHeight * 3 / 2;
	RK_S32 i = 0;
	RK_S32 pool_id;
	MB_POOL_CONFIG_S pool_cfg;
	MB_BLK blk;
	void *input_image_vaddr;
	RK_S32 input_image_fd;
	struct timespec iva_start_time, iva_end_time;
	long delay_time = (1000 / iva_ctx.u32IvaDetectFrameRate);
	long cost_time = 0;

	memset(&pool_cfg, 0, sizeof(MB_POOL_CONFIG_S));
	pool_cfg.u64MBSize = size;
	pool_cfg.u32MBCnt = 1;
	pool_cfg.enAllocType = MB_ALLOC_TYPE_DMA;
	pool_cfg.bPreAlloc = RK_FALSE;
	pool_id = RK_MPI_MB_CreatePool(&pool_cfg);
	if (pool_id == MB_INVALID_POOLID) {
		RK_LOGE("create mb pool failed");
		program_handle_error(__func__, __LINE__);
		return NULL;
	}
	// read test image
	blk = RK_MPI_MB_GetMB(pool_id, size, RK_TRUE);
	if (blk == MB_INVALID_HANDLE) {
		RK_LOGE("get mb block failed");
		program_handle_error(__func__, __LINE__);
		return NULL;
	}
	input_image_vaddr = RK_MPI_MB_Handle2VirAddr(blk);
	input_image_fd = RK_MPI_MB_Handle2Fd(blk);

	if (input_file_fd < 0) {
		RK_LOGE("open %s failed because %s, use empty image as input", path,
		        strerror(errno));
		memset(input_image_vaddr, 0, size);
		RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);
	} else {
		s32Ret = read(input_file_fd, input_image_vaddr, size);
		RK_LOGI("input image size %d", s32Ret);
		RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);
	}
	sem_init(&g_iva_semaphore, 0, 0);

	while (!quit && i < loop_count) {
		RK_LOGI("loop count %d", i++);
		clock_gettime(CLOCK_MONOTONIC, &iva_start_time);
		// send test image to iva
		input_image.info.transformMode = iva_ctx.eImageTransform;
		input_image.info.width = iva_ctx.u32ImageWidth;
		input_image.info.height = iva_ctx.u32ImageHeight;
		input_image.info.format = iva_ctx.eImageFormat;
		input_image.frameId = i;
		input_image.dataAddr = NULL;
		input_image.dataPhyAddr = NULL;
		input_image.dataFd = input_image_fd;
		s32Ret = ROCKIVA_PushFrame(iva_ctx.ivahandle, &input_image, NULL);
		if (s32Ret < 0) {
			RK_LOGE("ROCKIVA_PushFrame failed %#X", s32Ret);
			program_handle_error(__func__, __LINE__);
			break;
		}
		// wait for iva resule
		sem_wait(&g_iva_semaphore);

		clock_gettime(CLOCK_MONOTONIC, &iva_end_time);
		cost_time = (iva_end_time.tv_sec * 1000L + iva_end_time.tv_nsec / 1000000L) -
			(iva_start_time.tv_sec * 1000L + iva_start_time.tv_nsec / 1000000L);
		RK_LOGI("iva cost time %ld ms, delay for %ld ms"
				, cost_time
				, delay_time > cost_time ? (delay_time - cost_time) : 0);
		if (delay_time > cost_time)
			usleep((delay_time - cost_time) * 1000);
		// suspend and resume
		if (g_enable_sleep)
			SAMPLE_COMM_AOV_EnterSleep();
	}

	sem_destroy(&g_iva_semaphore);
	if (input_file_fd)
		close(input_file_fd);
	RK_MPI_MB_ReleaseMB(blk);
	RK_MPI_MB_DestroyPool(pool_id);
	program_normal_exit(__func__, __LINE__);
	RK_LOGE("send_frame_to_iva_thread exit !!!");
	return RK_NULL;
}

/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
int main(int argc, char *argv[]) {
	RK_U32 u32IvaWidth = 720;
	RK_U32 u32IvaHeight = 480;
	RK_U32 u32IvaDetectFrameRate = 10;
	RK_CHAR *pIvaModelPath = "/oem/usr/lib/";
	RK_S32 s32Ret;
	RK_S32 s32SuspendTime = 1000;
	pthread_t iva_thread_id;

	if (argc < 2) {
		print_usage(argv[0]);
		return 0;
	}

	signal(SIGINT, sigterm_handler);

	printf("%s initial start\n", __func__);
	int c;
	while ((c = getopt_long(argc, argv, optstr, long_options, NULL)) != -1) {
		const char *tmp_optarg = optarg;
		switch (c) {
		case 'w':
			u32IvaWidth = atoi(optarg);
			break;
		case 'h':
			u32IvaHeight = atoi(optarg);
			break;
		case 'l':
			loop_count = atoi(optarg);
			break;
		case 'r':
			u32IvaDetectFrameRate = atoi(optarg);
			break;
		case 's' + 't':
			s32SuspendTime = atoi(optarg);
			break;
		case 'e' + 's':
			g_enable_sleep = atoi(optarg);
			break;
		case 'p':
			path = optarg;
			break;
		case '?':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	RK_MPI_SYS_Init();
	SAMPLE_COMM_AOV_SetSuspendTime(s32SuspendTime);

	/* Init iva */
	iva_ctx.pModelDataPath = pIvaModelPath;
	iva_ctx.u32ImageWidth = u32IvaWidth;
	iva_ctx.u32ImageHeight = u32IvaHeight;
	iva_ctx.u32DetectStartX = 0;
	iva_ctx.u32DetectStartY = 0;
	iva_ctx.u32DetectWidth = u32IvaWidth;
	iva_ctx.u32DetectHight = u32IvaHeight;
	iva_ctx.eImageTransform = ROCKIVA_IMAGE_TRANSFORM_NONE;
	iva_ctx.eImageFormat = ROCKIVA_IMAGE_FORMAT_YUV420SP_NV12;
	iva_ctx.eModeType = ROCKIVA_DET_MODEL_PFP;
	iva_ctx.u32IvaDetectFrameRate = u32IvaDetectFrameRate;
	iva_ctx.detectResultCallback = rkIvaEvent_callback;
	iva_ctx.releaseCallback = rkIvaFrame_releaseCallBack;
	iva_ctx.eIvaMode = ROCKIVA_MODE_DETECT;
	s32Ret = SAMPLE_COMM_IVA_Create(&iva_ctx);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("SAMPLE_COMM_IVA_Create failure:%#X", s32Ret);
		return RK_FAILURE;
	}
	// /* VI[1] IVA thread launch */
	pthread_create(&iva_thread_id, 0, send_frame_to_iva_thread, NULL);

	printf("%s initial finish\n", __func__);
	while (!quit) {
		sleep(1);
	}

	printf("%s exit!\n", __func__);
	/* Destroy IVA */
	pthread_join(iva_thread_id, RK_NULL);
	SAMPLE_COMM_IVA_Destroy(&iva_ctx);

	RK_MPI_SYS_Exit();

	return quit_result;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
