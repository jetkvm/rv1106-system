/*
 * Copyright 2023 Rockchip Electronics Co. LTD
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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

#include "sample_comm.h"
#include "sample_comm_aov.h"

#define VI_MAIN_CHANNEL 0
#define VI_CHN_MAX 2
#define MAIN_CAM_INDEX 0
#define SUB_CAM_INDEX 1

#define TRACE_BEGIN() RK_LOGW("Enter\n")
#define TRACE_END() RK_LOGW("Exit\n")

typedef struct _rkCmdArgs {
	RK_U32 u32Main0Width;
	RK_U32 u32Main0Height;
	RK_U32 u32Main1Width;
	RK_U32 u32Main1Height;
	RK_U32 u32ViBuffCnt;
	RK_CHAR *pOutPath;
	RK_CHAR *pIqFileDir;
	RK_BOOL bMultictx;
	RK_CHAR *pCodecName;
	RK_S32 s32CamId;
	RK_BOOL bEnableSaveToSdcard;
	rk_aiq_working_mode_t eHdrMode;
	RK_S32 s32AeMode;
	RK_S32 s32AovLoopCount;
	RK_S32 s32SuspendTime;
	RK_S32 s32ViFrameMode;
	RK_U32 u32BootFrame;
	RK_BOOL bEnableDummyFrame;
	RK_U32 u32DummyFrameCnt;
} RkCmdArgs;

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi[VI_CHN_MAX];
} SAMPLE_MPI_CTX_S;

typedef struct _rkThreadStatus {
	RK_BOOL bIfMainThreadQuit;
	RK_BOOL bIfViThreadQuit;
	pthread_t s32ViThreadId;
} ThreadStatus;

enum ISP_MODE {
	SINGLE_FRAME_MODE,
	MULTI_FRAME_MODE,
};
static RkCmdArgs *g_cmd_args = RK_NULL;
static SAMPLE_MPI_CTX_S *g_mpi_ctx = RK_NULL;
static RK_S32 g_exit_result = RK_SUCCESS;
static ThreadStatus *g_thread_status = RK_NULL;

static void program_handle_error(const char *func, RK_U32 line) {
	RK_LOGE("func: <%s> line: <%d> error exit!", func, line);
	g_exit_result = RK_FAILURE;
	g_thread_status->bIfMainThreadQuit = RK_TRUE;
}

static void program_normal_exit(const char *func, RK_U32 line) {
	RK_LOGE("func: <%s> line: <%d> normal exit!", func, line);
	g_thread_status->bIfMainThreadQuit = RK_TRUE;
}

static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	program_normal_exit(__func__, __LINE__);
}

/******************************************************************************
 * function : vi thread
 ******************************************************************************/
static void *vi_get_stream_multi_mode(void *pArgs) {
	RK_S32 s32Ret = RK_FAILURE;
	SAMPLE_VI_CTX_S *main_ctx = &g_mpi_ctx->vi[0];
	SAMPLE_VI_CTX_S *sub_ctx = &g_mpi_ctx->vi[1];
	FILE *main_output_fp, *sub_output_fp = RK_NULL;
	void *main_output_ptr, *sub_output_ptr = RK_NULL;
	RK_U32 main_size, sub_size;
	RK_S32 loopCount = 0;
	enum ISP_MODE eCurISPMode = MULTI_FRAME_MODE;
	char name[256];
	VIDEO_FRAME_INFO_S tmp_frame;

	TRACE_BEGIN();

	if (g_cmd_args->pOutPath) {
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", g_cmd_args->pOutPath,
		         MAIN_CAM_INDEX);
		main_output_fp = fopen(name, "wb");
		if (main_output_fp == RK_NULL)
			RK_LOGE("Can't open file %s!\n", name);
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", g_cmd_args->pOutPath,
		         SUB_CAM_INDEX);
		sub_output_fp = fopen(name, "wb");
		if (sub_output_fp == RK_NULL)
			RK_LOGE("Can't open file %s!\n", name);
	}

	// befor enter AOV
	for (int i = 0; i < g_cmd_args->u32BootFrame; i++) {
		SAMPLE_COMM_VI_GetChnFrame(main_ctx, &main_output_ptr);
		SAMPLE_COMM_VI_GetChnFrame(sub_ctx, &sub_output_ptr);
		main_size = main_ctx->stViFrame.stVFrame.u32VirWidth *
		            main_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		if (main_output_fp) {
			fwrite(main_output_ptr, 1, main_size, main_output_fp);
			fflush(main_output_fp);
			RK_LOGD("main sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("main sensor get frame %d", loopCount);
		}

		sub_size = sub_ctx->stViFrame.stVFrame.u32VirWidth *
		           sub_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		if (sub_output_fp) {
			fwrite(sub_output_ptr, 1, sub_size, sub_output_fp);
			fflush(sub_output_fp);
			RK_LOGD("sub sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("sub sensor get frame %d", loopCount);
		}

		SAMPLE_COMM_VI_ReleaseChnFrame(main_ctx);
		SAMPLE_COMM_VI_ReleaseChnFrame(sub_ctx);
		loopCount++;
	}

	SAMPLE_COMM_ISP_SingleFrame(MAIN_CAM_INDEX);
	SAMPLE_COMM_ISP_SingleFrame(SUB_CAM_INDEX);

	while (RK_MPI_VI_GetChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId, &tmp_frame,
	                             200) == RK_SUCCESS) {
		RK_MPI_VI_ReleaseChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId, &tmp_frame);
		RK_LOGI("enter single ISP mode, main sensor drop frame");
	}
	while (RK_MPI_VI_GetChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId, &tmp_frame, 200) ==
	       RK_SUCCESS) {
		RK_MPI_VI_ReleaseChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId, &tmp_frame);
		RK_LOGI("enter single ISP mode, sub sensor drop frame");
	}
	eCurISPMode = SINGLE_FRAME_MODE;
	SAMPLE_COMM_AOV_EnterSleep();

	while (!g_thread_status->bIfViThreadQuit) {
		SAMPLE_COMM_VI_GetChnFrame(main_ctx, &main_output_ptr);
		SAMPLE_COMM_VI_GetChnFrame(sub_ctx, &sub_output_ptr);
		main_size = main_ctx->stViFrame.stVFrame.u32VirWidth *
		            main_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		sub_size = sub_ctx->stViFrame.stVFrame.u32VirWidth *
		           sub_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		if (main_output_fp) {
			fwrite(main_output_ptr, 1, main_size, main_output_fp);
			fflush(main_output_fp);
			RK_LOGD("main sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("main sensor get frame %d", loopCount);
		}

		if (sub_output_fp) {
			fwrite(sub_output_ptr, 1, sub_size, sub_output_fp);
			fflush(sub_output_fp);
			RK_LOGD("sub sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("sub sensor get frame %d", loopCount);
		}

		SAMPLE_COMM_VI_ReleaseChnFrame(main_ctx);
		SAMPLE_COMM_VI_ReleaseChnFrame(sub_ctx);

#if defined(RV1106)
		if (g_cmd_args->bEnableDummyFrame && eCurISPMode == SINGLE_FRAME_MODE) {
			for (int i = 0; i != g_cmd_args->u32DummyFrameCnt; ++i) {
				RK_MPI_VI_DevEnableSinglelFrame(MAIN_CAM_INDEX, 1);
				RK_MPI_VI_DevEnableSinglelFrame(SUB_CAM_INDEX, 1);
				s32Ret = RK_MPI_VI_GetChnFrame(main_ctx->u32PipeId, main_ctx->s32ChnId,
				                               &main_ctx->stViFrame, 1000);
				if (s32Ret == RK_SUCCESS) {
					RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
					        main_ctx->s32DevId, main_ctx->stViFrame.stVFrame.u32TimeRef,
					        main_ctx->stViFrame.stVFrame.u64PTS / 1000);
					RK_MPI_VI_ReleaseChnFrame(main_ctx->u32PipeId, main_ctx->s32ChnId,
					                          &main_ctx->stViFrame);
				} else {
					RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
					program_handle_error(__FUNCTION__, __LINE__);
					break;
				}
				s32Ret = RK_MPI_VI_GetChnFrame(sub_ctx->u32PipeId, sub_ctx->s32ChnId,
				                               &sub_ctx->stViFrame, 1000);
				if (s32Ret == RK_SUCCESS) {
					RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
					        sub_ctx->s32DevId, sub_ctx->stViFrame.stVFrame.u32TimeRef,
					        sub_ctx->stViFrame.stVFrame.u64PTS / 1000);
					RK_MPI_VI_ReleaseChnFrame(sub_ctx->u32PipeId, sub_ctx->s32ChnId,
					                          &sub_ctx->stViFrame);
				} else {
					RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
					program_handle_error(__FUNCTION__, __LINE__);
					break;
				}
			}
		}
#endif

		if (eCurISPMode == MULTI_FRAME_MODE) {
			RK_LOGI("#Pause isp, Enter single frame\n");
			SAMPLE_COMM_ISP_SingleFrame(MAIN_CAM_INDEX);
			SAMPLE_COMM_ISP_SingleFrame(SUB_CAM_INDEX);
			// drop frame before switch ISP mode.
			while (RK_MPI_VI_GetChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId,
			                             &tmp_frame, 1000) == RK_SUCCESS) {
				RK_MPI_VI_ReleaseChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId,
				                          &tmp_frame);
				RK_LOGI("enter single ISP mode, main sensor drop frame");
			}
			while (RK_MPI_VI_GetChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId, &tmp_frame,
			                             1000) == RK_SUCCESS) {
				RK_MPI_VI_ReleaseChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId,
				                          &tmp_frame);
				RK_LOGI("enter single ISP mode, sub sensor drop frame");
			}
			eCurISPMode = SINGLE_FRAME_MODE;
		}

		if (loopCount % 5 == 0) {
			RK_LOGI("#Resume isp, Enter multi frame\n");
			SAMPLE_COMM_ISP_MultiFrame(MAIN_CAM_INDEX);
			SAMPLE_COMM_ISP_MultiFrame(SUB_CAM_INDEX);

			for (int i = 0; i < 30; i++) {
				int ret;
				ret = RK_MPI_VI_GetChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId,
				                            &tmp_frame, -1);
				if (ret != RK_SUCCESS) {
					RK_LOGE("RK_MPI_VI_GetChnFrame fail %x", ret);
					abort();
				}
				ret = RK_MPI_VI_ReleaseChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId,
				                                &tmp_frame);
				ret = RK_MPI_VI_GetChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId,
				                            &tmp_frame, -1);
				if (ret != RK_SUCCESS) {
					RK_LOGE("RK_MPI_VI_GetChnFrame fail %x", ret);
					abort();
				}
				ret = RK_MPI_VI_ReleaseChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId,
				                                &tmp_frame);
				RK_LOGD("#Multi frame mode, drop frame count %d", i);
			}

			eCurISPMode = MULTI_FRAME_MODE;
		}

		if (g_cmd_args->s32AovLoopCount != 0 && eCurISPMode == SINGLE_FRAME_MODE) {
			if (g_cmd_args->s32AovLoopCount > 0)
				--g_cmd_args->s32AovLoopCount;
			SAMPLE_COMM_AOV_EnterSleep();
		} else if (g_cmd_args->s32AovLoopCount == 0) {
			RK_LOGI("Exit AOV!");
			program_normal_exit(__func__, __LINE__);
			break;
		}
		loopCount++;
	}

	if (eCurISPMode == SINGLE_FRAME_MODE) {
		SAMPLE_COMM_ISP_MultiFrame(MAIN_CAM_INDEX);
		SAMPLE_COMM_ISP_MultiFrame(SUB_CAM_INDEX);
		eCurISPMode = MULTI_FRAME_MODE;
	}

	if (main_output_fp) {
		fflush(main_output_fp);
		fclose(main_output_fp);
	}
	if (sub_output_fp) {
		fflush(sub_output_fp);
		fclose(sub_output_fp);
	}
	TRACE_END();

	return RK_NULL;
}

static void *vi_get_stream_single_mode(void *pArgs) {
	RK_S32 s32Ret = RK_FAILURE;
	SAMPLE_VI_CTX_S *main_ctx = &g_mpi_ctx->vi[0];
	SAMPLE_VI_CTX_S *sub_ctx = &g_mpi_ctx->vi[1];
	FILE *main_output_fp, *sub_output_fp = RK_NULL;
	void *main_output_ptr, *sub_output_ptr = RK_NULL;
	RK_U32 main_size, sub_size;
	RK_S32 loopCount = 0;
	enum ISP_MODE eCurISPMode = MULTI_FRAME_MODE;
	char name[256];
	VIDEO_FRAME_INFO_S tmp_frame;

	TRACE_BEGIN();

	if (g_cmd_args->pOutPath) {
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", g_cmd_args->pOutPath,
		         MAIN_CAM_INDEX);
		main_output_fp = fopen(name, "wb");
		if (main_output_fp == RK_NULL)
			RK_LOGE("Can't open file %s!\n", name);
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", g_cmd_args->pOutPath,
		         SUB_CAM_INDEX);
		sub_output_fp = fopen(name, "wb");
		if (sub_output_fp == RK_NULL)
			RK_LOGE("Can't open file %s!\n", name);
	}

	// befor enter AOV
	for (int i = 0; i < g_cmd_args->u32BootFrame; i++) {
		SAMPLE_COMM_VI_GetChnFrame(main_ctx, &main_output_ptr);
		SAMPLE_COMM_VI_GetChnFrame(sub_ctx, &sub_output_ptr);
		main_size = main_ctx->stViFrame.stVFrame.u32VirWidth *
		            main_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		sub_size = sub_ctx->stViFrame.stVFrame.u32VirWidth *
		           sub_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		if (main_output_fp) {
			fwrite(main_output_ptr, 1, main_size, main_output_fp);
			fflush(main_output_fp);
			RK_LOGD("main sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("main sensor get frame %d", loopCount);
		}

		if (sub_output_fp) {
			fwrite(sub_output_ptr, 1, sub_size, sub_output_fp);
			fflush(sub_output_fp);
			RK_LOGD("sub sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("sub sensor get frame %d", loopCount);
		}
		RK_LOGD("SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u "
		        "loop:%d seq:%d pts:%lld ms\n",
		        main_ctx->s32DevId, main_output_ptr, main_size, loopCount,
		        main_ctx->stViFrame.stVFrame.u32TimeRef,
		        main_ctx->stViFrame.stVFrame.u64PTS / 1000);
		RK_LOGD("SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u "
		        "loop:%d seq:%d pts:%lld ms\n",
		        sub_ctx->s32DevId, main_output_ptr, sub_size, loopCount,
		        sub_ctx->stViFrame.stVFrame.u32TimeRef,
		        sub_ctx->stViFrame.stVFrame.u64PTS / 1000);

		SAMPLE_COMM_VI_ReleaseChnFrame(main_ctx);
		SAMPLE_COMM_VI_ReleaseChnFrame(sub_ctx);
		loopCount++;
	}

	SAMPLE_COMM_ISP_SingleFrame(MAIN_CAM_INDEX);
	SAMPLE_COMM_ISP_SingleFrame(SUB_CAM_INDEX);

	while (RK_MPI_VI_GetChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId, &tmp_frame,
	                             1000) == RK_SUCCESS) {
		RK_MPI_VI_ReleaseChnFrame(main_ctx->s32DevId, main_ctx->s32ChnId, &tmp_frame);
		RK_LOGI("enter single ISP mode, main sensor drop frame");
	}
	while (RK_MPI_VI_GetChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId, &tmp_frame,
	                             1000) == RK_SUCCESS) {
		RK_MPI_VI_ReleaseChnFrame(sub_ctx->s32DevId, sub_ctx->s32ChnId, &tmp_frame);
		RK_LOGI("enter single ISP mode, sub sensor drop frame");
	}
	eCurISPMode = SINGLE_FRAME_MODE;
	SAMPLE_COMM_AOV_EnterSleep();

	while (!g_thread_status->bIfViThreadQuit) {
		SAMPLE_COMM_VI_GetChnFrame(main_ctx, &main_output_ptr);
		SAMPLE_COMM_VI_GetChnFrame(sub_ctx, &sub_output_ptr);
		main_size = main_ctx->stViFrame.stVFrame.u32VirWidth *
		            main_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		sub_size = sub_ctx->stViFrame.stVFrame.u32VirWidth *
		           sub_ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
		if (main_output_fp) {
			fwrite(main_output_ptr, 1, main_size, main_output_fp);
			fflush(main_output_fp);
			RK_LOGD("main sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("main sensor get frame %d", loopCount);
		}

		if (sub_output_fp) {
			fwrite(sub_output_ptr, 1, sub_size, sub_output_fp);
			fflush(sub_output_fp);
			RK_LOGD("sub sensor write frame %d to sdcard", loopCount);
		} else {
			RK_LOGI("sub sensor get frame %d", loopCount);
		}

		RK_LOGD("SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u "
		        "loop:%d seq:%d pts:%lld ms\n",
		        main_ctx->s32DevId, main_output_ptr, main_size, loopCount,
		        main_ctx->stViFrame.stVFrame.u32TimeRef,
		        main_ctx->stViFrame.stVFrame.u64PTS / 1000);
		RK_LOGD("SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u "
		        "loop:%d seq:%d pts:%lld ms\n",
		        sub_ctx->s32DevId, main_output_ptr, sub_size, loopCount,
		        sub_ctx->stViFrame.stVFrame.u32TimeRef,
		        sub_ctx->stViFrame.stVFrame.u64PTS / 1000);
		SAMPLE_COMM_VI_ReleaseChnFrame(main_ctx);
		SAMPLE_COMM_VI_ReleaseChnFrame(sub_ctx);

#if defined(RV1106)
		if (g_cmd_args->bEnableDummyFrame && eCurISPMode == SINGLE_FRAME_MODE) {
			for (int i = 0; i != g_cmd_args->u32DummyFrameCnt; ++i) {
				RK_MPI_VI_DevEnableSinglelFrame(MAIN_CAM_INDEX, 1);
				RK_MPI_VI_DevEnableSinglelFrame(SUB_CAM_INDEX, 1);
				s32Ret = RK_MPI_VI_GetChnFrame(main_ctx->u32PipeId, main_ctx->s32ChnId,
				                               &main_ctx->stViFrame, 1000);
				if (s32Ret == RK_SUCCESS) {
					RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
					        main_ctx->s32DevId, main_ctx->stViFrame.stVFrame.u32TimeRef,
					        main_ctx->stViFrame.stVFrame.u64PTS / 1000);
					RK_MPI_VI_ReleaseChnFrame(main_ctx->u32PipeId, main_ctx->s32ChnId,
					                          &main_ctx->stViFrame);
				} else {
					RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
					program_handle_error(__FUNCTION__, __LINE__);
					break;
				}
				s32Ret = RK_MPI_VI_GetChnFrame(sub_ctx->u32PipeId, sub_ctx->s32ChnId,
				                               &sub_ctx->stViFrame, 1000);
				if (s32Ret == RK_SUCCESS) {
					RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
					        sub_ctx->s32DevId, sub_ctx->stViFrame.stVFrame.u32TimeRef,
					        sub_ctx->stViFrame.stVFrame.u64PTS / 1000);
					RK_MPI_VI_ReleaseChnFrame(sub_ctx->u32PipeId, sub_ctx->s32ChnId,
					                          &sub_ctx->stViFrame);
				} else {
					RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
					program_handle_error(__FUNCTION__, __LINE__);
					break;
				}
			}
		}
#endif
		if (g_cmd_args->s32AovLoopCount != 0) {
			if (g_cmd_args->s32AovLoopCount > 0)
				--g_cmd_args->s32AovLoopCount;
			SAMPLE_COMM_AOV_EnterSleep();
		} else {
			RK_LOGI("Exit AOV!");
			program_normal_exit(__func__, __LINE__);
			break;
		}
		loopCount++;
	}
	SAMPLE_COMM_ISP_MultiFrame(MAIN_CAM_INDEX);
	SAMPLE_COMM_ISP_MultiFrame(SUB_CAM_INDEX);

	if (main_output_fp) {
		fflush(main_output_fp);
		fclose(main_output_fp);
	}
	if (sub_output_fp) {
		fflush(sub_output_fp);
		fclose(sub_output_fp);
	}
	TRACE_END();

	return RK_NULL;
}

static RK_S32 global_param_init(void) {
	TRACE_BEGIN();
	g_thread_status = (ThreadStatus *)malloc(sizeof(ThreadStatus));
	if (!g_thread_status) {
		RK_LOGI("malloc for g_thread_status failure\n");
		goto __global_init_fail;
	}
	memset(g_thread_status, 0, sizeof(ThreadStatus));
	// Allocate global ctx.
	g_mpi_ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
	if (!g_mpi_ctx) {
		printf("ctx is null, malloc failure\n");
		goto __global_init_fail;
	}
	memset(g_mpi_ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

	g_cmd_args = malloc(sizeof(RkCmdArgs));
	if (!g_cmd_args) {
		printf("g_cmd_args is null, malloc failure\n");
		goto __global_init_fail;
	}
	memset(g_cmd_args, 0, sizeof(RkCmdArgs));

	TRACE_END();
	return RK_SUCCESS;

__global_init_fail:
	if (g_thread_status) {
		free(g_thread_status);
		g_thread_status = RK_NULL;
	}
	if (g_mpi_ctx) {
		free(g_mpi_ctx);
		g_mpi_ctx = NULL;
	}
	if (g_cmd_args) {
		free(g_cmd_args);
		g_cmd_args = NULL;
	}
	TRACE_END();
	return RK_FAILURE;
}

static RK_S32 global_param_deinit(void) {
	TRACE_BEGIN();
	if (g_thread_status) {
		free(g_thread_status);
		g_thread_status = RK_NULL;
	}
	if (g_mpi_ctx) {
		free(g_mpi_ctx);
		g_mpi_ctx = NULL;
	}
	if (g_cmd_args) {
		free(g_cmd_args);
		g_cmd_args = NULL;
	}

	TRACE_END();
	return RK_SUCCESS;
}

static RK_S32 isp_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	if (g_cmd_args->pIqFileDir) {

		s32Ret = SAMPLE_COMM_ISP_Init(MAIN_CAM_INDEX, g_cmd_args->eHdrMode,
		                              g_cmd_args->bMultictx, g_cmd_args->pIqFileDir);
		s32Ret |= SAMPLE_COMM_ISP_Run(MAIN_CAM_INDEX);
		if (s32Ret != RK_SUCCESS) {
			printf("#ISP cam %d init failed!\n", MAIN_CAM_INDEX);
			return s32Ret;
		}
		s32Ret = SAMPLE_COMM_ISP_Init(SUB_CAM_INDEX, g_cmd_args->eHdrMode,
		                              g_cmd_args->bMultictx, g_cmd_args->pIqFileDir);
		s32Ret |= SAMPLE_COMM_ISP_Run(SUB_CAM_INDEX);
		if (s32Ret != RK_SUCCESS) {
			printf("#ISP cam %d init failed!\n", SUB_CAM_INDEX);
			return s32Ret;
		}
	}
	return s32Ret;
}

static RK_S32 isp_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	if (g_cmd_args->pIqFileDir) {
		SAMPLE_COMM_ISP_Stop(MAIN_CAM_INDEX);
		SAMPLE_COMM_ISP_Stop(SUB_CAM_INDEX);
	}
	return s32Ret;
}

static RK_S32 vi_chn_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	TRACE_BEGIN();
	/* Init VI[0] */
	ctx->vi[0].bIfQuickStart = true;
	ctx->vi[0].u32Width = pArgs->u32Main0Width;
	ctx->vi[0].u32Height = pArgs->u32Main0Height;
	ctx->vi[0].s32DevId = MAIN_CAM_INDEX;
	ctx->vi[0].u32PipeId = MAIN_CAM_INDEX;
	ctx->vi[0].s32ChnId = 0;
	ctx->vi[0].dstFilePath = pArgs->pOutPath;
	ctx->vi[0].stChnAttr.u32Depth = 1;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main0Width;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main0Height;
	ctx->vi[0].stChnAttr.stIspOpt.u32BufCount = pArgs->u32ViBuffCnt;
	ctx->vi[0].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[0].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[0].stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
	ctx->vi[0].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[0].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	s32Ret = SAMPLE_COMM_VI_CreateChn(&(ctx->vi[0]));
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VI_CreateChn 0 failure:%d", s32Ret);
	/* Init VI[1] */
	ctx->vi[1].bIfQuickStart = true;
	ctx->vi[1].u32Width = pArgs->u32Main1Width;
	ctx->vi[1].u32Height = pArgs->u32Main1Height;
	ctx->vi[1].s32DevId = SUB_CAM_INDEX;
	ctx->vi[1].u32PipeId = SUB_CAM_INDEX;
	ctx->vi[1].s32ChnId = 0;
	ctx->vi[1].dstFilePath = pArgs->pOutPath;
	ctx->vi[1].stChnAttr.u32Depth = 1;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main1Width;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main1Height;
	ctx->vi[1].stChnAttr.stIspOpt.u32BufCount = pArgs->u32ViBuffCnt;
	ctx->vi[1].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[1].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[1].stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
	ctx->vi[1].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[1].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	s32Ret = SAMPLE_COMM_VI_CreateChn(&(ctx->vi[1]));
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VI_CreateChn 1 failure:%d", s32Ret);
	TRACE_END();
	return s32Ret;
}

static RK_S32 vi_chn_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	TRACE_BEGIN();
	s32Ret = SAMPLE_COMM_VI_DestroyChn(&(ctx->vi[1]));
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VI_DestroyChn failure:%d", s32Ret);
	s32Ret = SAMPLE_COMM_VI_DestroyChn(&(ctx->vi[0]));
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VI_DestroyChn failure:%d", s32Ret);
	TRACE_END();
	return s32Ret;
}

static RK_S32 sub_threads_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	TRACE_BEGIN();
	if (g_cmd_args->s32ViFrameMode == 1)
		pthread_create(&g_thread_status->s32ViThreadId, NULL, vi_get_stream_multi_mode,
		               NULL);
	else
		pthread_create(&g_thread_status->s32ViThreadId, NULL, vi_get_stream_single_mode,
		               NULL);
	TRACE_END();
	return RK_SUCCESS;
}

static RK_S32 sub_threads_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	TRACE_BEGIN();
	g_thread_status->bIfViThreadQuit = RK_TRUE;
	pthread_join(g_thread_status->s32ViThreadId, NULL);
	TRACE_END();
	return RK_SUCCESS;
}
static RK_CHAR optstr[] = "?::a::w:h:o:l:b:f:r:g:v:e:i:s:I:";

static const struct option long_options[] = {
    {"aiq", optional_argument, RK_NULL, 'a'},
    {"width", required_argument, RK_NULL, 'w'},
    {"height", required_argument, RK_NULL, 'h'},
    {"output_path", required_argument, RK_NULL, 'o'},
    {"fps", required_argument, RK_NULL, 'f'},
    {"vi_buff_cnt", required_argument, RK_NULL, 'v'},
    {"sensor", required_argument, RK_NULL, 's'},
    {"enable_save_sdcard", required_argument, RK_NULL, 'e' + 'm' + 'h'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"aov_loop_count", required_argument, NULL, 'a' + 'm' + 'c'},
    {"vi_frame_mode", required_argument, NULL, 'v' + 'f' + 'm'},
    {"help", optional_argument, RK_NULL, '?'},
    {"boot_frame", required_argument, NULL, 'b' + 'f'},
    {"enable_dummy_frame", required_argument, NULL, 'e' + 'd' + 'f'},
    {"dummy_frame_cnt", required_argument, NULL, 'd' + 'f' + 'c'},
    {RK_NULL, 0, RK_NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("\t%s -s 0 -w 2048 -h 1536 -s 1 -w 1920 -h 1080 "
	       "--aov_loop_count 10\n",
	       name);
	printf("\t-a | --aiq : enable aiq with dirpath provided, eg:-a "
	       "/etc/iqfiles/, \n"
	       "\t		set dirpath empty to using path by default, without "
	       "this option aiq \n"
	       "\t		should run in other application\n");
	printf("\t-s | --sensor : 0 means main camera, 1 means sub camera\n");
	printf("\t-w | --width : mainStream width, must is sensor width\n");
	printf("\t-h | --height : mainStream height, must is sensor height\n");
	printf("\t-o | --output_path : encode output file path, Default: "
	       "/mnt/sdcard\n");
	printf("\t-v | --vi_buff_cnt : main stream vi buffer num, Default: 2\n");
	printf("\t-f | --fps : set fps, default: 10\n");
	printf("\t--aov_loop_count: When the value of aov_loop_count is greater \n"
	       "\t\t than 0, "
	       "this value represents the number of AOV cycles. A \n"
	       "\t\t negative value indicates an infinite loop, Default: "
	       "-1(unlimit)\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--vi_frame_mode: set vi frame mode, 0: single mode 1: multi -> "
	       "single -> "
	       "multi, Default: 0\n");
	printf("\t--boot_frame: How long will it take to enter AOV mode after boot"
	       ", Default: 60 frames\n");
	printf("\t--enable_dummy_frame: Enable fetch dummy frame in single frame mode"
	       ", Default: 1\n");
	printf("\t--dummy_frame_cnt: Dummy frame count in single frame mode"
	       ", Default: 3\n");
}

/******************************************************************************
 * function    : parse_cmd_args()
 * Description : Parse command line arguments.
 ******************************************************************************/
static RK_S32 parse_cmd_args(int argc, char **argv, RkCmdArgs *pArgs) {
	pArgs->u32Main0Width = 1920;
	pArgs->u32Main0Height = 1080;
	pArgs->u32Main1Width = 1920;
	pArgs->u32Main1Height = 1080;
	pArgs->u32ViBuffCnt = 2;
	pArgs->pOutPath = "/mnt/sdcard";
	pArgs->pIqFileDir = "/etc/oem/share/iqfiles";
	pArgs->bMultictx = RK_TRUE;
	pArgs->s32CamId = 0;
	pArgs->eHdrMode = RK_AIQ_WORKING_MODE_NORMAL;
	pArgs->s32AeMode = 0;
	pArgs->s32AovLoopCount = -1;
	pArgs->s32SuspendTime = 1000;
	pArgs->bEnableSaveToSdcard = RK_TRUE;
	pArgs->s32ViFrameMode = 0;
	pArgs->u32BootFrame = 60;
	pArgs->bEnableDummyFrame = RK_TRUE;
	pArgs->u32DummyFrameCnt = 3;
	int sensor_index = 0;

	RK_S32 c = 0;
	while ((c = getopt_long(argc, argv, optstr, long_options, RK_NULL)) != -1) {
		const char *tmp_optarg = optarg;
		switch (c) {
		case 'a':
			if (!optarg && RK_NULL != argv[optind] && '-' != argv[optind][0]) {
				tmp_optarg = argv[optind++];
			}
			if (tmp_optarg) {
				pArgs->pIqFileDir = (char *)tmp_optarg;
			} else {
				pArgs->pIqFileDir = RK_NULL;
			}
			break;
		case 's':
			sensor_index = atoi(optarg);
		case 'w':
			if (sensor_index == 0)
				pArgs->u32Main0Width = atoi(optarg);
			else if (sensor_index == 1)
				pArgs->u32Main1Width = atoi(optarg);
			else
				printf("# Error sensor index %d!\n", sensor_index);
			break;
		case 'h':
			if (sensor_index == 0)
				pArgs->u32Main0Height = atoi(optarg);
			else if (sensor_index == 1)
				pArgs->u32Main1Height = atoi(optarg);
			else
				printf("# Error sensor index %d!\n", sensor_index);
			break;
		case 'o':
			pArgs->pOutPath = optarg;
			break;
		case 'v':
			pArgs->u32ViBuffCnt = atoi(optarg);
			break;
		case 'v' + 'f' + 'm':
			pArgs->s32ViFrameMode = atoi(optarg);
			break;
		case 'e' + 'm' + 'h':
			pArgs->bEnableSaveToSdcard = atoi(optarg);
			break;
		case 'a' + 'm' + 'c':
			pArgs->s32AovLoopCount = atoi(optarg);
			break;
		case 's' + 't':
			pArgs->s32SuspendTime = atoi(optarg);
			break;
		case 'b' + 'f':
			pArgs->u32BootFrame = atoi(optarg);
			break;
		case 'e' + 'd' + 'f':
			pArgs->bEnableDummyFrame = atoi(optarg) ? RK_TRUE : RK_FALSE;
			break;
		case 'd' + 'f' + 'c':
			pArgs->u32DummyFrameCnt = atoi(optarg);
			break;
		case '?':
			print_usage(argv[0]);
		default:
			return RK_FAILURE;
		}
	}

	return RK_SUCCESS;
}

/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
int main(int argc, char *argv[]) {
	RK_S32 s32Ret = RK_SUCCESS;

	if (argc < 2) {
		print_usage(argv[0]);
		printf("bad arguments!\n");
		return RK_FAILURE;
	}

	if (global_param_init() != RK_SUCCESS) {
		printf("global_param_init failure!\n");
		return RK_FAILURE;
	}

	// Parse command line.
	if (parse_cmd_args(argc, argv, g_cmd_args) != RK_SUCCESS) {
		printf("parse_cmd_args failure\n");
		goto __ISP_INIT_FAILED;
	}

	signal(SIGINT, sigterm_handler);
	signal(SIGTERM, sigterm_handler);

	SAMPLE_COMM_AOV_Init();

	printf("#CodecName:%s\n", g_cmd_args->pCodecName);
	printf("#Output Path: %s\n", g_cmd_args->pOutPath);
	printf("#bMultictx: %d\n\n", g_cmd_args->bMultictx);

	if (isp_init(g_mpi_ctx, g_cmd_args) != RK_SUCCESS) {
		printf("isp_init failure!\n");
		g_exit_result = RK_FAILURE;
		goto __ISP_INIT_FAILED;
	}

	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		printf("RK_MPI_SYS_Init failure");
		g_exit_result = RK_FAILURE;
		goto __MPI_INIT_FAILED;
	}

	vi_chn_init(g_mpi_ctx, g_cmd_args);
	sub_threads_init(g_mpi_ctx, g_cmd_args);

	SAMPLE_COMM_AOV_SetSuspendTime(g_cmd_args->s32SuspendTime);

	// Keep running ...
	while (!g_thread_status->bIfMainThreadQuit) {
		sleep(1);
	}

	sub_threads_deinit(g_mpi_ctx, g_cmd_args);
	vi_chn_deinit(g_mpi_ctx, g_cmd_args);

	RK_MPI_SYS_Exit();

__MPI_INIT_FAILED:
	isp_deinit(g_mpi_ctx, g_cmd_args);
	SAMPLE_COMM_AOV_Deinit();
__ISP_INIT_FAILED:
	global_param_deinit();

	return g_exit_result;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
