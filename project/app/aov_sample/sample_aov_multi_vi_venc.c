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

#define VI_CHN_MAX 2
#define VENC_CHN_MAX 2
#define MAIN_CAM_INDEX 0
#define SUB_CAM_INDEX 1
#define VENC_MAIN_CHN 0
#define VENC_SUB_CHN 1

#define TRACE_BEGIN() RK_LOGW("Enter\n")
#define TRACE_END() RK_LOGW("Exit\n")

typedef struct _rkCmdArgs {
	RK_U32 u32Main0Width;
	RK_U32 u32Main0Height;
	RK_U32 u32Main1Width;
	RK_U32 u32Main1Height;
	RK_U32 u32ViBuffCnt;
	RK_U32 u32Gop;
	RK_U32 u32ViFps;
	RK_CHAR *pOutPathVenc;
	RK_CHAR *pIqFileDir;
	RK_BOOL bMultictx;
	CODEC_TYPE_E enCodecType;
	VENC_RC_MODE_E enRcMode;
	RK_CHAR *pCodecName;
	RK_S32 s32CamId;
	RK_BOOL bEnableSaveToSdcard;
	RK_BOOL bEnableMultiMode;
	RK_S32 s32BitRate;
	RK_U32 u32VencFps;
	rk_aiq_working_mode_t eHdrMode;
	RK_S32 s32AeMode;
	RK_S32 s32AovLoopCount;
	RK_S32 s32SuspendTime;
	RK_U32 u32BootFrame;
	RK_U32 u32QuickStart;
	RK_BOOL bWrapIfEnable;
	RK_U32 u32WrapLine;
} RkCmdArgs;

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi[VI_CHN_MAX];
	SAMPLE_VENC_CTX_S venc[VENC_CHN_MAX];
} SAMPLE_MPI_CTX_S;

typedef struct _rkThreadStatus {
	RK_BOOL bIfMainThreadQuit;
	RK_BOOL bIfVencThreadQuit;
	pthread_t s32VencThreadId;
} ThreadStatus;

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

static void drop_all_venc_chn_frame() {
	VENC_STREAM_S stFrame_tmp;
	RK_S32 s32ChnId;
	RK_S32 s32LoopCount = 0;
	stFrame_tmp.pstPack = (VENC_PACK_S *)(malloc(sizeof(VENC_PACK_S)));
	for (s32ChnId = 0; s32ChnId != VENC_CHN_MAX; ++s32ChnId) {
		s32LoopCount = 0;
		while (RK_MPI_VENC_GetStream(s32ChnId, &stFrame_tmp, 1000) == RK_SUCCESS) {
			RK_MPI_VENC_ReleaseStream(s32ChnId, &stFrame_tmp);
			RK_LOGD("drop frame now, chn:%d, len:%u, pts:%llu, seq:%u", s32ChnId,
			        stFrame_tmp.pstPack->u32Len, stFrame_tmp.pstPack->u64PTS,
			        stFrame_tmp.u32Seq);
			++s32LoopCount;
			if (s32LoopCount > 30)
				RK_LOGW("venc %d drop too much frame!!!", s32ChnId);
		}
	}
	free(stFrame_tmp.pstPack);
}

static RK_S32 venc_get_frame_and_save2sdcard(SAMPLE_VENC_CTX_S *ctx, char *const buffer,
                                             RK_U32 *psize, RK_S32 frame_num) {
	VENC_STREAM_S frame;
	RK_S32 s32Ret;
	void *data = RK_NULL;

	memset(&frame, 0, sizeof(frame));
	frame.pstPack = (VENC_PACK_S *)(malloc(sizeof(VENC_PACK_S)));
	if (frame.pstPack == NULL) {
		RK_LOGE("malloc failed!");
		program_handle_error(__func__, __LINE__);
		return RK_FAILURE;
	}
	s32Ret = RK_MPI_VENC_GetStream(ctx->s32ChnId, &frame, 2000);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_VENC_GetStream failed %#X", s32Ret);
		free(frame.pstPack);
		return s32Ret;
	}

	if (g_cmd_args->bEnableSaveToSdcard) {
		if (buffer == NULL || *psize > AOV_STREAM_SIZE_WRITE_TO_SDCARD) {
			RK_LOGE("Error buffer ptr %p, size %d!", buffer, *psize);
			program_handle_error(__func__, __LINE__);
			free(frame.pstPack);
			return RK_FAILURE;
		}

		data = RK_MPI_MB_Handle2VirAddr(frame.pstPack->pMbBlk);
		if ((*psize + frame.pstPack->u32Len) <= AOV_STREAM_SIZE_WRITE_TO_SDCARD) {
			if (*psize == 0 && frame.pstPack->DataType.enH265EType == H265E_NALU_PSLICE) {
				// force idr frame
				RK_LOGD("work round force idr, skip...\n");
			} else {
				memcpy(buffer + *psize, data, frame.pstPack->u32Len);
				*psize += frame.pstPack->u32Len;
				RK_LOGD("cache stream in buffer\n");
			}
		} else {
			RK_LOGD("save stream to sdcard\n");
			RK_MPI_VENC_RequestIDR(ctx->s32ChnId, RK_FALSE);
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(ctx->s32ChnId, buffer, *psize, data,
			                                      frame.pstPack->u32Len);
			*psize = 0;
		}
	}
	RK_LOGD("chn:%d, frame %d, len:%u, pts:%llu, seq:%u", ctx->s32ChnId, frame_num,
	        frame.pstPack->u32Len, frame.pstPack->u64PTS, frame.u32Seq);
	RK_MPI_VENC_ReleaseStream(ctx->s32ChnId, &frame);
	free(frame.pstPack);
	return RK_SUCCESS;
}
/******************************************************************************
 * function : venc thread
 ******************************************************************************/
static void *venc_get_stream(void *pArgs) {
	SAMPLE_VENC_CTX_S *main_ctx = &g_mpi_ctx->venc[0];
	SAMPLE_VENC_CTX_S *sub_ctx = &g_mpi_ctx->venc[1];
	RK_S32 s32Ret = RK_FAILURE;
	char name[256] = {0};
	RK_S32 loop_count = 0;
	char *main_buffer = NULL;
	char *sub_buffer = NULL;
	RK_U32 main_buffer_size = 0;
	RK_U32 sub_buffer_size = 0;
	RK_U32 venc_drop_frame_count = 0;

	TRACE_BEGIN();

	if (g_cmd_args->bEnableSaveToSdcard) {
		// Allocate buffer to cache venc stream.
		main_buffer =
		    (char *)malloc(AOV_STREAM_SIZE_WRITE_TO_SDCARD); // 10M form 200 frame
		if (main_buffer == NULL) {
			RK_LOGE("malloc failed!");
			program_handle_error(__func__, __LINE__);
			return NULL;
		}
		main_buffer_size = 0;

		sub_buffer =
		    (char *)malloc(AOV_STREAM_SIZE_WRITE_TO_SDCARD); // 10M form 200 frame
		if (sub_buffer == NULL) {
			RK_LOGE("malloc failed!");
			program_handle_error(__func__, __LINE__);
			return NULL;
		}
		sub_buffer_size = 0;
	}

	for (int i = 0; i < g_cmd_args->u32BootFrame; i++) {
		venc_get_frame_and_save2sdcard(main_ctx, main_buffer, &main_buffer_size, i);
		venc_get_frame_and_save2sdcard(sub_ctx, sub_buffer, &sub_buffer_size, i);
	}
	if (g_cmd_args->bEnableSaveToSdcard) {
		if (main_buffer_size) {
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(VENC_MAIN_CHN, main_buffer,
			                                      main_buffer_size, NULL, 0);
			main_buffer_size = 0;
		}
		if (sub_buffer_size) {
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(VENC_SUB_CHN, sub_buffer,
			                                      sub_buffer_size, NULL, 0);
			sub_buffer_size = 0;
		}
	}

	// Enter single frame mode
	SAMPLE_COMM_ISP_SingleFrame(MAIN_CAM_INDEX);
	SAMPLE_COMM_ISP_SingleFrame(SUB_CAM_INDEX);
	// drop frame
	drop_all_venc_chn_frame();
	// request idr
	RK_MPI_VENC_RequestIDR(VENC_MAIN_CHN, RK_FALSE);
	RK_MPI_VENC_RequestIDR(VENC_SUB_CHN, RK_FALSE);
	SAMPLE_COMM_AOV_EnterSleep();

	while (!g_thread_status->bIfVencThreadQuit) {
		s32Ret = venc_get_frame_and_save2sdcard(main_ctx, main_buffer, &main_buffer_size,
		                                        loop_count);
		s32Ret |= venc_get_frame_and_save2sdcard(sub_ctx, sub_buffer, &sub_buffer_size,
		                                         loop_count);
		++loop_count;

		if (s32Ret == RK_SUCCESS) {
			if (g_cmd_args->s32AovLoopCount != 0) {
				if (g_cmd_args->s32AovLoopCount > 0)
					--g_cmd_args->s32AovLoopCount;
				SAMPLE_COMM_AOV_EnterSleep();
			} else {
				program_normal_exit(__func__, __LINE__);
				RK_LOGI("Exit AOV!");
				break;
			}
			venc_drop_frame_count = 0;
		} else {
			++venc_drop_frame_count;
			RK_LOGE("venc drop frame, force to Sleep, venc_drop_frame_count = %d\n",
			        venc_drop_frame_count);
			venc_drop_frame_count++;
			if (venc_drop_frame_count < 10)
				SAMPLE_COMM_AOV_EnterSleep();
			else
				RK_LOGE("venc drop too much frame!!!");
		}
	}

	if (g_cmd_args->bEnableSaveToSdcard) {
		if (main_buffer_size) {
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(VENC_MAIN_CHN, main_buffer,
			                                      main_buffer_size, NULL, 0);
			main_buffer_size = 0;
		}
		if (sub_buffer_size) {
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(VENC_SUB_CHN, sub_buffer,
			                                      sub_buffer_size, NULL, 0);
			sub_buffer_size = 0;
		}
	}
	if (main_buffer)
		free(main_buffer);
	if (sub_buffer)
		free(sub_buffer);

	SAMPLE_COMM_ISP_MultiFrame(MAIN_CAM_INDEX);
	SAMPLE_COMM_ISP_MultiFrame(SUB_CAM_INDEX);
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
	ctx->vi[0].bIfQuickStart = pArgs->u32QuickStart;
	ctx->vi[0].u32Width = pArgs->u32Main0Width;
	ctx->vi[0].u32Height = pArgs->u32Main0Height;
	ctx->vi[0].s32DevId = MAIN_CAM_INDEX;
	ctx->vi[0].u32PipeId = MAIN_CAM_INDEX;
	ctx->vi[0].s32ChnId = 0;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main0Width;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main0Height;
	ctx->vi[0].stChnAttr.stIspOpt.u32BufCount = pArgs->u32ViBuffCnt;
	ctx->vi[0].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[0].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[0].stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
	ctx->vi[0].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[0].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main0Width;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main0Height;
	if (pArgs->bWrapIfEnable) {
		ctx->vi[0].bWrapIfEnable = RK_TRUE;
		ctx->vi[0].u32BufferLine = ctx->vi[0].u32Height / pArgs->u32WrapLine;
	}
	s32Ret = SAMPLE_COMM_VI_CreateChn(&(ctx->vi[0]));
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VI_CreateChn 0 failure:%d", s32Ret);
	/* Init VI[1] */
	ctx->vi[1].bIfQuickStart = pArgs->u32QuickStart;
	ctx->vi[1].u32Width = pArgs->u32Main1Width;
	ctx->vi[1].u32Height = pArgs->u32Main1Height;
	ctx->vi[1].s32DevId = SUB_CAM_INDEX;
	ctx->vi[1].u32PipeId = SUB_CAM_INDEX;
	ctx->vi[1].s32ChnId = 0;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main1Width;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main1Height;
	ctx->vi[1].stChnAttr.stIspOpt.u32BufCount = pArgs->u32ViBuffCnt;
	ctx->vi[1].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[1].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[1].stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
	ctx->vi[1].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[1].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Width = pArgs->u32Main1Width;
	ctx->vi[1].stChnAttr.stIspOpt.stMaxSize.u32Height = pArgs->u32Main1Height;
	if (pArgs->bWrapIfEnable) {
		ctx->vi[1].bWrapIfEnable = RK_TRUE;
		ctx->vi[1].u32BufferLine = ctx->vi[1].u32Height / pArgs->u32WrapLine;
	}
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

static RK_S32 venc_chn_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;

	TRACE_BEGIN();
	// Init VENC[0]
	ctx->venc[0].s32ChnId = VENC_MAIN_CHN;
	ctx->venc[0].u32Width = pArgs->u32Main0Width;
	ctx->venc[0].u32Height = pArgs->u32Main0Height;
	ctx->venc[0].u32Fps = pArgs->u32VencFps;
	ctx->venc[0].u32Gop = pArgs->u32Gop;
	ctx->venc[0].u32BitRate = pArgs->s32BitRate;
	ctx->venc[0].enCodecType = pArgs->enCodecType;
	ctx->venc[0].enRcMode = pArgs->enRcMode;
	ctx->venc[0].getStreamCbFunc = NULL;
	ctx->venc[0].dstFilePath = pArgs->pOutPathVenc;
	ctx->venc[0].u32BuffSize = pArgs->u32Main0Width * pArgs->u32Main0Height / 2;
	ctx->venc[0].enable_buf_share = RK_TRUE;
	// H264  66：Baseline  77：Main Profile 100：High Profile
	// H265  0：Main Profile  1：Main 10 Profile
	// MJPEG 0：Baseline
	ctx->venc[0].stChnAttr.stGopAttr.enGopMode =
	    VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
	if (RK_CODEC_TYPE_H264 != pArgs->enCodecType) {
		ctx->venc[0].stChnAttr.stVencAttr.u32Profile = 0;
	} else {
		ctx->venc[0].stChnAttr.stVencAttr.u32Profile = 100;
	}

	if (pArgs->bWrapIfEnable) {
		ctx->venc[0].bWrapIfEnable = RK_TRUE;
		ctx->venc[0].u32BufferLine = ctx->venc[0].u32Height / pArgs->u32WrapLine;
	}
	s32Ret = SAMPLE_COMM_VENC_CreateChn(&ctx->venc[0]);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VENC_CreateChn venc0 failed %#X\n", s32Ret);

	// Init VENC[1]
	ctx->venc[1].s32ChnId = VENC_SUB_CHN;
	ctx->venc[1].u32Width = pArgs->u32Main1Width;
	ctx->venc[1].u32Height = pArgs->u32Main1Height;
	ctx->venc[1].u32Fps = pArgs->u32VencFps;
	ctx->venc[1].u32Gop = pArgs->u32Gop;
	ctx->venc[1].u32BitRate = pArgs->s32BitRate;
	ctx->venc[1].enCodecType = pArgs->enCodecType;
	ctx->venc[1].enRcMode = pArgs->enRcMode;
	ctx->venc[1].getStreamCbFunc = NULL;
	ctx->venc[1].dstFilePath = pArgs->pOutPathVenc;
	ctx->venc[1].u32BuffSize = pArgs->u32Main1Width * pArgs->u32Main1Height / 2;
	ctx->venc[1].enable_buf_share = RK_TRUE;
	// H264  66：Baseline  77：Main Profile 100：High Profile
	// H265  0：Main Profile  1：Main 10 Profile
	// MJPEG 0：Baseline
	ctx->venc[1].stChnAttr.stGopAttr.enGopMode =
	    VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
	if (RK_CODEC_TYPE_H264 != pArgs->enCodecType) {
		ctx->venc[1].stChnAttr.stVencAttr.u32Profile = 0;
	} else {
		ctx->venc[1].stChnAttr.stVencAttr.u32Profile = 100;
	}
	if (pArgs->bWrapIfEnable) {
		ctx->venc[1].bWrapIfEnable = RK_TRUE;
		ctx->venc[1].u32BufferLine = ctx->venc[1].u32Height / pArgs->u32WrapLine;
	}
	s32Ret = SAMPLE_COMM_VENC_CreateChn(&ctx->venc[1]);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VENC_CreateChn venc1 failed %#X\n", s32Ret);

	TRACE_END();

	return s32Ret;
}

static RK_S32 venc_chn_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;

	TRACE_BEGIN();
	s32Ret = SAMPLE_COMM_VENC_DestroyChn(&ctx->venc[1]);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VENC_CreateChn venc1 failed %#X\n", s32Ret);

	s32Ret = SAMPLE_COMM_VENC_DestroyChn(&ctx->venc[0]);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("SAMPLE_COMM_VENC_CreateChn venc0 failed %#X\n", s32Ret);
	TRACE_END();

	return s32Ret;
}

static RK_S32 bind_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	MPP_CHN_S stSrcChn, stDestChn;

	TRACE_BEGIN();
	// Bind VI[0] and VENC[0]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[MAIN_CAM_INDEX].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[MAIN_CAM_INDEX].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = VENC_MAIN_CHN;
	s32Ret = SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("bind vi0 to vpss0 failed");

	// Bind VI[1] and VENC[1]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[SUB_CAM_INDEX].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[SUB_CAM_INDEX].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = VENC_SUB_CHN;
	s32Ret = SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("bind vi1 to vpss1 failed");

	TRACE_END();

	return s32Ret;
}

static RK_S32 bind_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	RK_S32 s32Ret = RK_SUCCESS;
	MPP_CHN_S stSrcChn, stDestChn;

	TRACE_BEGIN();
	// UnBind VI[1] and VENC[1]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[SUB_CAM_INDEX].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[SUB_CAM_INDEX].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = VENC_SUB_CHN;
	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

	// UnBind VI[0] and VENC[0]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[MAIN_CAM_INDEX].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[MAIN_CAM_INDEX].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = VENC_MAIN_CHN;
	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);
	TRACE_END();

	return s32Ret;
}
static RK_S32 sub_threads_init(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	TRACE_BEGIN();
	pthread_create(&g_thread_status->s32VencThreadId, NULL, venc_get_stream, NULL);
	TRACE_END();
	return RK_SUCCESS;
}

static RK_S32 sub_threads_deinit(SAMPLE_MPI_CTX_S *ctx, RkCmdArgs *pArgs) {
	TRACE_BEGIN();
	g_thread_status->bIfVencThreadQuit = true;
	pthread_join(g_thread_status->s32VencThreadId, NULL);
	TRACE_END();
	return RK_SUCCESS;
}
static RK_CHAR optstr[] = "?::a::w:h:o:l:b:f:r:g:v:e:i:s:I:";

static const struct option long_options[] = {
    {"aiq", optional_argument, RK_NULL, 'a'},
    {"sensor", required_argument, RK_NULL, 's'},
    {"width", required_argument, RK_NULL, 'w'},
    {"height", required_argument, RK_NULL, 'h'},
    {"encode", required_argument, RK_NULL, 'e'},
    {"output_path", required_argument, RK_NULL, 'o'},
    {"bitrate", required_argument, NULL, 'b'},
    {"fps", required_argument, RK_NULL, 'f'},
    {"wrap", required_argument, RK_NULL, 'r'},
    {"vi_buff_cnt", required_argument, RK_NULL, 'v'},
    {"gop", required_argument, RK_NULL, 'g'},
    {"enable_multi_frame", required_argument, RK_NULL, 'e' + 'm' + 'f'},
    {"enable_save_sdcard", required_argument, RK_NULL, 'e' + 'm' + 'h'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"aov_loop_count", required_argument, NULL, 'a' + 'm' + 'c'},
    {"help", optional_argument, RK_NULL, '?'},
    {"boot_frame", required_argument, NULL, 'b' + 'f'},
    {"quick_start", required_argument, NULL, 'q' + 'k' + 's'},
    {"wrap_lines", required_argument, RK_NULL, 'w' + 'l'},
    {RK_NULL, 0, RK_NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("\t%s -s 0 -w 2048 -h 1536 -s 1 -w 1920 -h 1080 --aov_loop_count 10\n", name);
	printf("\t-a | --aiq : enable aiq with dirpath provided, eg:-a "
	       "/etc/iqfiles/, \n"
	       "\t		set dirpath empty to using path by default, without "
	       "this option aiq \n"
	       "\t		should run in other application\n");
	printf("\t-w | --width : mainStream width, must is sensor width\n");
	printf("\t-h | --height : mainStream height, must is sensor height\n");
	printf("\t-s | --sensor : 0 means main camera, 1 means sub camera\n");
	printf("\t-e | --encode: encode type, Default:h264cbr, Value:h264cbr, "
	       "h264vbr, h264avbr "
	       "h265cbr, h265vbr, h265avbr, mjpegcbr, mjpegvbr\n");
	printf("\t-b | --bitrate: encode bitrate, Default 4096\n");
	printf("\t-o | --output_path : encode output file path, Default: RK_NULL\n");
	printf("\t-v | --vi_buff_cnt : main stream vi buffer num, Default: 2\n");
	printf("\t--vi_chnid : vi channel id, default: 0\n");
	printf("\t-f | --fps : set fps, default: 10\n");
	printf("\t--aov_loop_count: When the value of aov_loop_count is greater \n"
	       "\t\t than 0, "
	       "this value represents the number of AOV cycles. A \n"
	       "\t\t negative value indicates an infinite loop, Default: "
	       "-1(unlimit)\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--boot_frame: How long will it take to enter AOV mode after boot"
	       ", Default: 60 frames\n");
	printf("\t--quick_start: quick start stream, Default: 0\n");
	printf("\t-r | --wrap : wrap for mainStream, 0: close 1: open, Default: 0\n");
	printf("\t--wrap_lines : 0: height/2, 1: height/4, 2: height/8. default: 1\n");
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
	pArgs->u32Gop = 20;
	pArgs->u32ViFps = 10;
	pArgs->pOutPathVenc = NULL;
	pArgs->pIqFileDir = "/oem/usr/share/iqfiles";
	pArgs->bMultictx = RK_TRUE;
	pArgs->enCodecType = RK_CODEC_TYPE_H264;
	pArgs->enRcMode = VENC_RC_MODE_H264CBR;
	pArgs->pCodecName = "H264";
	pArgs->s32CamId = 0;
	pArgs->s32BitRate = 4 * 1024;
	pArgs->u32VencFps = 10;
	pArgs->eHdrMode = RK_AIQ_WORKING_MODE_NORMAL;
	pArgs->s32AeMode = 0;
	pArgs->s32AovLoopCount = -1;
	pArgs->s32SuspendTime = 1000;
	pArgs->bEnableSaveToSdcard = RK_TRUE;
	pArgs->bEnableMultiMode = RK_TRUE;
	pArgs->u32BootFrame = 60;
	pArgs->u32QuickStart = 0;
	pArgs->bWrapIfEnable = RK_FALSE;
	pArgs->u32WrapLine = 4;
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
		case 'b':
			pArgs->s32BitRate = atoi(optarg);
			break;
		case 'e':
			if (!strcmp(optarg, "h264cbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H264;
				pArgs->enRcMode = VENC_RC_MODE_H264CBR;
			} else if (!strcmp(optarg, "h264vbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H264;
				pArgs->enRcMode = VENC_RC_MODE_H264VBR;
			} else if (!strcmp(optarg, "h264avbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H264;
				pArgs->enRcMode = VENC_RC_MODE_H264AVBR;
			} else if (!strcmp(optarg, "h265cbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H265;
				pArgs->enRcMode = VENC_RC_MODE_H265CBR;
			} else if (!strcmp(optarg, "h265vbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H265;
				pArgs->enRcMode = VENC_RC_MODE_H265VBR;
			} else if (!strcmp(optarg, "h265avbr")) {
				pArgs->enCodecType = RK_CODEC_TYPE_H265;
				pArgs->enRcMode = VENC_RC_MODE_H265AVBR;
			} else {
				RK_LOGE("Invalid encoder type!");
				return RK_FAILURE;
			}
			break;
		case 'o':
			pArgs->pOutPathVenc = optarg;
			break;
		case 'f':
			pArgs->u32VencFps = atoi(optarg);
			break;
		case 'r':
			pArgs->bWrapIfEnable = atoi(optarg);
			break;
		case 'w' + 'l':
			if (0 == atoi(optarg)) {
				pArgs->u32WrapLine = 2;
			} else if (1 == atoi(optarg)) {
				pArgs->u32WrapLine = 4;
			} else if (2 == atoi(optarg)) {
				pArgs->u32WrapLine = 8;
			} else {
				RK_LOGE("ERROR: Invalid WrapLine Value.");
			}
			break;
		case 'v':
			pArgs->u32ViBuffCnt = atoi(optarg);
			break;
		case 'g':
			pArgs->u32Gop = atoi(optarg);
			break;
		case 'e' + 'm' + 'f':
			pArgs->bEnableMultiMode = atoi(optarg);
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
		case 'q' + 'k' + 's':
			pArgs->u32QuickStart = atoi(optarg);
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

	SAMPLE_COMM_AOV_Init(NULL);

	printf("#CameraIdx: %d\n", g_cmd_args->s32CamId);
	printf("#CodecName:%s\n", g_cmd_args->pCodecName);
	printf("#Output Path: %s\n", g_cmd_args->pOutPathVenc);
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
	venc_chn_init(g_mpi_ctx, g_cmd_args);
	bind_init(g_mpi_ctx, g_cmd_args);

	if (!g_cmd_args->u32QuickStart) {
		RK_MPI_VI_StartPipe(MAIN_CAM_INDEX);
		RK_MPI_VI_StartPipe(SUB_CAM_INDEX);
	}

	sub_threads_init(g_mpi_ctx, g_cmd_args);

	SAMPLE_COMM_AOV_SetSuspendTime(g_cmd_args->s32SuspendTime);

	// Keep running ...
	while (!g_thread_status->bIfMainThreadQuit) {
		sleep(1);
	}

	sub_threads_deinit(g_mpi_ctx, g_cmd_args);
	bind_deinit(g_mpi_ctx, g_cmd_args);
	venc_chn_deinit(g_mpi_ctx, g_cmd_args);
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
