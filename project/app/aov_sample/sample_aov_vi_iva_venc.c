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
#include "utils.h"

#define VI_MAIN_CHANNEL 0
#define VI_IVA_CHANNEL 1
#define VI_BYPASS_CHANNEL 2
#define VENC_MAIN_CHNNAL 0
#define VENC_BYPASS_CHNNAL 1
#define RGN_CHANNEL 0

static rtsp_demo_handle g_rtsplive = NULL;
static rtsp_session_handle g_rtsp_session;
static sem_t g_wakeup_nn_result_semaphore;
static RK_S32 g_wakeup_nn_flag = 1;
static RK_S32 g_multi_frame_enable = 1;
static RK_U32 g_u32BootFrame = 60;
static RK_S32 g_s32AovLoopCount = -1;
static RK_S32 g_enable_save_sd = 1;
static RK_S32 g_enable_save_mp4 = 0;
static RK_S32 g_max_wakeup_frame_count = 3000;
static RK_S32 g_low_power_mode = 0;
static RK_U32 g_u32Fps = 0;
static RK_S32 g_enable_fast_ae = 0;
static RK_S32 s32SuspendTime = 1000;
static RK_S32 s32EnableMultiCore = 1;

#define RED_COLOR 0x0000FF
#define BLUE_COLOR 0xFF0000

enum ISP_MODE {
	SINGLE_FRAME_MODE,
	MULTI_FRAME_MODE,
};

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi[3];
	SAMPLE_VENC_CTX_S venc[2];
	SAMPLE_IVS_CTX_S ivs;
#ifdef ROCKIVA
	SAMPLE_IVA_CTX_S iva;
#endif
	SAMPLE_RGN_CTX_S rgn[2];
} SAMPLE_MPI_CTX_S;

static SAMPLE_MPI_MUXER_S *g_pst_muxer;
static bool quit = false;
static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	quit = true;
}

static RK_CHAR optstr[] = "?::a::b:w:h:l:o:e:d:D:I:i:L:M:r:g:f:";
static const struct option long_options[] = {
    {"aiq", optional_argument, NULL, 'a'},
    {"bitrate", required_argument, NULL, 'b'},
    {"device_name", required_argument, NULL, 'd'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"encode", required_argument, NULL, 'e'},
    {"fps", required_argument, NULL, 'f'},
    {"gop", required_argument, NULL, 'g'},
    {"wrap", required_argument, RK_NULL, 'r'},
    {"hdr_mode", required_argument, NULL, 'h' + 'm'},
    {"ae_mode", required_argument, NULL, 'a' + 'm'},
    {"aov_loop_count", required_argument, NULL, 'a' + 'm' + 'c'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"iva_detect_speed", required_argument, RK_NULL, 'i' + 'd' + 's'},
    {"enable_multi_frame", required_argument, RK_NULL, 'e' + 'm' + 'f'},
    {"boot_frame", required_argument, NULL, 'b' + 'f'},
    {"wrap_lines", required_argument, RK_NULL, 'w' + 'l'},
    {"enable_save_sdcard", required_argument, RK_NULL, 'e' + 's'},
    {"enable_save_mp4", required_argument, RK_NULL, 'e' + 'm'},
    {"max_wakeup_frame_count", required_argument, RK_NULL, 'm' + 'w'},
    {"quick_start", required_argument, NULL, 'q' + 'k' + 's'},
    {"low_power", required_argument, NULL, 'l' + 'w' + 'p'},
    {"camera_mirror_flip", required_argument, NULL, 'm' + 'p'},
    {"fast_ae", required_argument, NULL, 'f' + 'a'},
    {"enable_multi_core", required_argument, NULL, 'e' + 's' + 'p'},
    {"enable_bypass", required_argument, NULL, 'e' + 'b' + 'p'},
    {"help", optional_argument, NULL, '?'},
    {NULL, 0, NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("usage example:\n");
	printf("\t%s -w 2304 -h 1296 -a /etc/iqfiles/ -I 0 -e h264cbr -b 4096 "
	       "-i /usr/share/image.bmp -o /data/\n",
	       name);
	printf("\trtsp://xx.xx.xx.xx/live/0, Default OPEN\n");
#ifdef RKAIQ
	printf("\t-a | --aiq: enable aiq with dirpath provided, eg:-a "
	       "/etc/iqfiles/, "
	       "set dirpath empty to using path by default, without this option aiq "
	       "should run in other application\n");
#endif
	printf("\t-d | --device_name: set pcDeviceName, eg: /dev/video0 Default "
	       "NULL\n");
	printf("\t-w | --width: camera with, Default 1920\n");
	printf("\t-h | --height: camera height, Default 1080\n");
	printf("\t-e | --encode: encode type, Default:h265vbr, Value:h264cbr, "
	       "h264vbr, h264avbr "
	       "h265cbr, h265vbr, h265avbr, mjpegcbr, mjpegvbr\n");
	printf("\t-b | --bitrate: encode bitrate, Default 4096\n");
	printf("\t-i | --input_bmp_name: input file path of logo.bmp, Default NULL\n");
	printf("\t-r | --wrap : wrap for mainStream, 0: close 1: open, Default: 0\n");
	printf("\t-f | --fps: camera framerate. Default: 25\n");
	printf("\t-g | --gop: venc gop. Default: fps x 2\n");
	printf("\t--enable_save_sdcard : enable save venc stream to sdcard, default: 1\n");
	printf("\t--enable_save_mp4 : enable save mp4 stream to sdcard, default: 1\n");
	printf("\t--wrap_lines : 0: height/1, 1: height/2, 2: height/4, 3: height/8. "
	       "default: 2\n");
	printf("\t--iva_detect_speed : iva detect framerate. default: 10\n");
	printf("\t--ae_mode: set aov ae wakeup mode, 0: MD wakupe: 1: always wakeup, 2: no "
	       "wakeup, Default: 0\n");
	printf("\t--aov_loop_count: set aov wakeup loop count, Default: -1(unlimit)\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--boot_frame: How long will it take to enter AOV mode after boot, Default: "
	       "60 frames\n");
	printf("\t--enable_multi_frame : Identify whether the humanoid enters multi frame "
	       "mode, 0: close, 1: enable. default: 1\n");
	printf("\t--max_wakeup_frame_count: max frame count running in multi "
	       "frame mode after wakeup by gpio, Default: 3000\n");
	printf("\t--quick_start: quick start stream, Default: 0\n");
	printf("\t--low_power: After awakening, perform human form detection and encoding. "
	       "1: open, 0: close. default: 0\n");
	printf("\t--camera_mirror_flip: camera mirror flip, 0: 0, 1: 90, 2: 180, 3: 270. "
	       "Defaule 0\n");
	printf("\t--fast_ae: enable faset ae, 0: close, 1: enable. default: 0\n");
	printf("\t--enable_multi_core: enable multi core, Default: 1\n");
	printf("\t--enable_bypass: enable bypass stream, Default: 1\n");
}

/******************************************************************************
 * function : venc thread
 ******************************************************************************/
static void *venc_get_stream(void *pArgs) {
	SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
	RK_S32 s32Ret = RK_FAILURE;
	char name[256] = {0};
	void *pData = RK_NULL;
	RK_S32 loopCount = 0;
	RK_S32 venc_data_size = 0;
	RK_S32 force_flush_to_storage = 0;
	uint64_t pre_pts = 0;
	char *venc_data = NULL;

	if (g_enable_save_mp4) {
		SAMPLE_COMM_AOV_InitMp4(ctx, &g_pst_muxer);
	} else if (g_enable_save_sd) {
		venc_data = (char *)malloc(AOV_STREAM_SIZE_WRITE_TO_SDCARD); // 10M form 200 frame
		venc_data_size = 0;
		RK_MPI_VENC_RequestIDR(ctx->s32ChnId, RK_FALSE);
	}
	while (!quit) {
		s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

			SAMPLE_COMM_AOV_DumpPtsToTMP(ctx->stFrame.u32Seq,
			                             ctx->stFrame.pstPack->u64PTS, g_u32BootFrame);
			if (g_enable_save_mp4) {
				SAMPLE_COMM_AOV_CopyMp4StreamToSdcard(g_pst_muxer, &ctx->stFrame, pData);
			} else if (g_enable_save_sd) {
				if ((venc_data_size + ctx->stFrame.pstPack->u32Len) <=
				    AOV_STREAM_SIZE_WRITE_TO_SDCARD) {
					if (venc_data_size == 0 &&
					    ctx->stFrame.pstPack->DataType.enH265EType == H265E_NALU_PSLICE) {
						// force idr frame
						RK_LOGI("work round force idr, skip...\n");
					} else {
						memcpy(venc_data + venc_data_size, pData,
						       ctx->stFrame.pstPack->u32Len);
						venc_data_size += ctx->stFrame.pstPack->u32Len;
					}
				} else {
					RK_MPI_VENC_RequestIDR(ctx->s32ChnId, RK_FALSE);
					SAMPLE_COMM_AOV_CopyRawStreamToSdcard(ctx->s32ChnId, venc_data,
					                                      venc_data_size, pData,
					                                      ctx->stFrame.pstPack->u32Len);
					venc_data_size = 0;
				}
			}

			RK_LOGE("chn:%d, loopCount:%d, len:%u, pts:%llu, pts count:%llu, seq:%u",
			        ctx->s32ChnId, loopCount, ctx->stFrame.pstPack->u32Len,
			        ctx->stFrame.pstPack->u64PTS,
			        (ctx->stFrame.pstPack->u64PTS - pre_pts - s32SuspendTime * 1000) /
			            1000,
			        ctx->stFrame.u32Seq);
			pre_pts = ctx->stFrame.pstPack->u64PTS;

			SAMPLE_COMM_VENC_ReleaseStream(ctx);

			loopCount++;
		}
	}

	if (g_enable_save_mp4) {
		SAMPLE_COMM_AOV_DeinitMp4(g_pst_muxer);
	} else if (g_enable_save_sd) {
		if (venc_data_size > 0)
			SAMPLE_COMM_AOV_CopyRawStreamToSdcard(ctx->s32ChnId, venc_data,
			                                      venc_data_size, NULL, 0);
		venc_data_size = 0;
		free(venc_data);
	}

	return RK_NULL;
}

static void *venc_get_bypass_stream(void *pArgs) {
	SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
	RK_S32 s32Ret = RK_FAILURE;
	char name[256] = {0};
	void *pData = RK_NULL;
	RK_S32 loopCount = 0;

	RK_LOGI("enter bypass stream thread");
	while (!quit) {
		s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {
			RK_LOGE("chn:%d, loopCount:%d, len:%u, pts:%llu, seq:%u", ctx->s32ChnId,
			        loopCount, ctx->stFrame.pstPack->u32Len, ctx->stFrame.pstPack->u64PTS,
			        ctx->stFrame.u32Seq);
			SAMPLE_COMM_VENC_ReleaseStream(ctx);
			loopCount++;
		}
	}
	RK_LOGI("exit bypass stream thread");

	return RK_NULL;
}

#ifdef ROCKIVA
static void rkIvaEvent_callback(const RockIvaDetectResult *result,
                                const RockIvaExecuteStatus status, void *userData) {

	if (result->objNum == 0) {
		if (g_wakeup_nn_flag > 0)
			g_wakeup_nn_flag--;
	} else {
		g_wakeup_nn_flag = 5;
	}
	RK_LOGI("RKIVA: objNum is %d, g_wakeup_nn_flag = %d\n", result->objNum,
	        g_wakeup_nn_flag);

	if (result->objNum == 0)
		return;
	SAMPLE_COMM_IVA_Push_Result(result);

	for (int i = 0; i < result->objNum; i++) {
		RK_LOGD("topLeft:[%d,%d], bottomRight:[%d,%d],"
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
	RK_S32 s32Ret = RK_SUCCESS;
	for (RK_S32 i = 0; i < releaseFrames->count; i++) {
		if (!releaseFrames->frames[i].extData) {
			RK_LOGE("---------error release frame is null");
			// program_handle_error(__func__, __LINE__);
			continue;
		}
		s32Ret = RK_MPI_VI_ReleaseChnFrame(0, VI_IVA_CHANNEL,
		                                   releaseFrames->frames[i].extData);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VI_ReleaseChnFrame failure:%#X", s32Ret);
			// program_handle_error(__func__, __LINE__);
		}
		free(releaseFrames->frames[i].extData);
	}
	sem_post(&g_wakeup_nn_result_semaphore);
}

static void *vi_iva_low_power_thread(void *pArgs) {
	prctl(PR_SET_NAME, "vi_iva_low_power_thread");
	SAMPLE_MPI_CTX_S *ctx = (SAMPLE_MPI_CTX_S *)pArgs;
	RK_S32 s32Ret = RK_FAILURE;
	RK_CHAR *pData = RK_NULL;
	RK_S32 s32Fd = 0;
	RockIvaImage ivaImage;
	RK_U32 u32Loopcount = 0;
	RK_U32 u32GetOneFrameTime = 1000 / ctx->iva.u32IvaDetectFrameRate;
	RK_U32 u32IvaDetectStep = g_u32Fps / ctx->iva.u32IvaDetectFrameRate;
	VIDEO_FRAME_INFO_S *stViFrame = NULL;
	enum ISP_MODE wakeup_current_mode = MULTI_FRAME_MODE;
	bool is_gpioirq_happened = false;
	RK_S32 wakeup_frame_count = 0;
	struct timespec iva_start_time, iva_end_time;
	VIDEO_FRAME_INFO_S stVencFrame;
	RK_BOOL bExpBigChange = RK_FALSE;

	sem_init(&g_wakeup_nn_result_semaphore, 0, 0);
	g_wakeup_nn_flag = 1;

	// befor enter AOV
	for (int i = 0; i < g_u32BootFrame; i++) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(&ctx->vi[1], (void *)&pData);
		if (s32Ret == RK_SUCCESS) {
			SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
			u32Loopcount++;
		}

		s32Ret = RK_MPI_VI_GetChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId,
		                               &stVencFrame, -1);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VI_GetChnFrame %d failed %#X", ctx->vi[0].s32ChnId, s32Ret);
		}
		s32Ret = RK_MPI_VENC_SendFrame(0, &stVencFrame, 1000);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_SendFrame chnd  %d failed %#X", 0, s32Ret);
		}
		RK_MPI_VI_ReleaseChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId, &stVencFrame);
	}

	SAMPLE_COMM_AOV_GetGpioIrqStat(); // ignore previous input events.
	while (!quit) {
		// 1. Check input event to detect weather gpio irq is happened.
		is_gpioirq_happened = SAMPLE_COMM_AOV_GetGpioIrqStat();
		RK_LOGD("is_gpioirq_happened %d", is_gpioirq_happened);

		// 2. Get frame from VI and send frame to IVA
		SAMPLE_COMM_VI_GetChnFrame(&ctx->vi[1], (void *)&pData);
	RETRY:
		s32Ret = RK_MPI_VI_GetChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId,
		                               &stVencFrame, -1);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VI_GetChnFrame %d failed%#X", ctx->vi[0].s32ChnId, s32Ret);
			goto RETRY;
		}

		// 3. Change ISP state if need.
		switch (wakeup_current_mode) {
		case SINGLE_FRAME_MODE:
			// The condition for switching to multi frame mode is:
			//	1) Have detected a human body.
			//	2) GPIO irq is happend.
			//  3) FastAE mode
			if (g_enable_fast_ae)
				bExpBigChange = SAMPLE_COMM_ISP_IsExpBigChange(0);
			if (g_wakeup_nn_flag > 0 || is_gpioirq_happened || bExpBigChange) {
				// to multi frame
				RK_LOGI("#Resume isp, Enter multi frame, wakeup because %s\n",
				        is_gpioirq_happened ? "gpio irq" : "detected human body");
				SAMPLE_COMM_ISP_MultiFrame(0);
				SAMPLE_COMM_IVA_SetWorkMode(&ctx->iva, ROCKIVA_MODE_VIDEO);
				wakeup_current_mode = MULTI_FRAME_MODE;
				if (is_gpioirq_happened) {
					wakeup_frame_count = g_max_wakeup_frame_count;
					is_gpioirq_happened = false;
				}
			}
			break;
		case MULTI_FRAME_MODE:
			// The condition for switching to single frame mode is:
			//	1) Have not detected human body too much times.
			//	2) GPIO irq is happend.
			//	3) wakeup_frame_count equals to zero.
			if (wakeup_frame_count > 0 && !is_gpioirq_happened) {
				--wakeup_frame_count;
				RK_LOGD("wakeup_frame_count %d", wakeup_frame_count);
			} else if (g_wakeup_nn_flag > 0 && !is_gpioirq_happened || bExpBigChange) {
				// keep multi frame mode
				RK_LOGD("detected human body, loop %d", u32Loopcount);
				if (bExpBigChange && SAMPLE_COMM_ISP_IsExpConverge(0))
					bExpBigChange = RK_FALSE;
			} else if (g_wakeup_nn_flag == 0 || wakeup_frame_count == 0 ||
			           is_gpioirq_happened) {
				// to single frame
				RK_LOGI("#Pause isp, Enter single frame\n");
				SAMPLE_COMM_ISP_SingleFrame(0);
				SAMPLE_COMM_IVA_SetWorkMode(&ctx->iva, ROCKIVA_MODE_PICTURE);
				wakeup_current_mode = SINGLE_FRAME_MODE;
				wakeup_frame_count = 0;
				// drop frame
				VIDEO_FRAME_INFO_S stViFrame_tmp;
				while (RK_MPI_VI_GetChnFrame(ctx->vi[1].s32DevId, ctx->vi[1].s32ChnId,
				                             &stViFrame_tmp, 1000) == RK_SUCCESS) {
					RK_MPI_VI_ReleaseChnFrame(ctx->vi[1].s32DevId, ctx->vi[1].s32ChnId,
					                          &stViFrame_tmp);
				}
				while (RK_MPI_VI_GetChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId,
				                             &stViFrame_tmp, 1000) == RK_SUCCESS) {
					RK_MPI_VI_ReleaseChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId,
					                          &stViFrame_tmp);
				}
			}
			break;
		}

		// 5. Enter sleep mode if in single frame mode.
		if (wakeup_current_mode == SINGLE_FRAME_MODE) {
			if (g_s32AovLoopCount != 0) {
				if (g_s32AovLoopCount > 0)
					--g_s32AovLoopCount;
				SAMPLE_COMM_AOV_EnterSleep();
			} else if (g_s32AovLoopCount == 0) {
				quit = true;
				RK_LOGI("Exit AOV!");
				break;
			}
		}

		s32Ret = RK_MPI_VENC_SendFrame(0, &stVencFrame, 1000);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_SendFrame chnd  %d failed %#X", 0, s32Ret);
		}
		RK_MPI_VI_ReleaseChnFrame(ctx->vi[0].s32DevId, ctx->vi[0].s32ChnId, &stVencFrame);

		switch (wakeup_current_mode) {
		case SINGLE_FRAME_MODE:
			SAMPLE_COMM_RGN_DrawOsd(&ctx->rgn[1], RK_TRUE, 1000 / (float)s32SuspendTime);
			// use ivs
			if (SAMPLE_COMM_IVS_bMove(&ctx->ivs, &ctx->vi[1].stViFrame) != RK_TRUE) {
				s32Ret = SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
				if (s32Ret != RK_SUCCESS)
					RK_LOGE("SAMPLE_COMM_VI_ReleaseChnFrame chnd  %d failed %#X", s32Ret,
					        ctx->vi[1].s32ChnId);
				continue;
			}
			break;
		case MULTI_FRAME_MODE:
			if (ctx->vi[1].stViFrame.stVFrame.u32TimeRef % u32IvaDetectStep != 0) {
				s32Ret = SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
				if (s32Ret != RK_SUCCESS)
					RK_LOGE("SAMPLE_COMM_VI_ReleaseChnFrame chnd  %d failed %#X", s32Ret,
					        ctx->vi[1].s32ChnId);
				continue;
			}
			SAMPLE_COMM_RGN_DrawOsd(&ctx->rgn[1], RK_FALSE,
			                        SAMPLE_COMM_ISP_GetFrameRate(0));
			break;
		}

		stViFrame = (VIDEO_FRAME_INFO_S *)malloc(sizeof(VIDEO_FRAME_INFO_S));
		if (!stViFrame) {
			RK_LOGE("-----error malloc fail for stViFrame");
			SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
			continue;
		}
		memcpy(stViFrame, &ctx->vi[1].stViFrame, sizeof(VIDEO_FRAME_INFO_S));
		s32Fd = RK_MPI_MB_Handle2Fd(stViFrame->stVFrame.pMbBlk);
		memset(&ivaImage, 0, sizeof(RockIvaImage));
		ivaImage.info.transformMode = ctx->iva.eImageTransform;
		ivaImage.info.width = stViFrame->stVFrame.u32Width;
		ivaImage.info.height = stViFrame->stVFrame.u32Height;
		ivaImage.info.format = ctx->iva.eImageFormat;
		ivaImage.frameId = u32Loopcount;
		ivaImage.dataAddr = NULL;
		ivaImage.dataPhyAddr = NULL;
		ivaImage.dataFd = s32Fd;
		ivaImage.extData = stViFrame;
		clock_gettime(CLOCK_MONOTONIC, &iva_start_time);
		s32Ret = ROCKIVA_PushFrame(ctx->iva.ivahandle, &ivaImage, NULL);
		// 6. Wait IVA result callback
		sem_wait(&g_wakeup_nn_result_semaphore);
		SAMPLE_COMM_RGN_DrawRectFromIVA(&ctx->rgn[0], RK_TRUE);
		clock_gettime(CLOCK_MONOTONIC, &iva_end_time);
		RK_LOGE("iva cost time %lld ms\n",
		        (iva_end_time.tv_sec - iva_start_time.tv_sec) * 1000LL +
		            (iva_end_time.tv_nsec - iva_start_time.tv_nsec) / 1000000LL);
	}

	if (wakeup_current_mode == SINGLE_FRAME_MODE) {
		SAMPLE_COMM_ISP_MultiFrame(0);
		wakeup_current_mode = MULTI_FRAME_MODE;
	}
	RK_LOGE("vi_iva_thread exit !!!");
	return RK_NULL;
}

static void *vi_iva_thread(void *pArgs) {
	prctl(PR_SET_NAME, "vi_iva_thread");
	SAMPLE_MPI_CTX_S *ctx = (SAMPLE_MPI_CTX_S *)pArgs;
	RK_S32 s32Ret = RK_FAILURE;
	RK_CHAR *pData = RK_NULL;
	RK_S32 s32Fd = 0;
	RockIvaImage ivaImage;
	RK_U32 u32Loopcount = 0;
	RK_U32 u32GetOneFrameTime = 1000 / ctx->iva.u32IvaDetectFrameRate;
	VIDEO_FRAME_INFO_S *stViFrame = NULL;
	enum ISP_MODE wakeup_current_mode = MULTI_FRAME_MODE;
	bool is_gpioirq_happened = false;
	RK_S32 wakeup_frame_count = 0;
	struct timespec iva_start_time, iva_end_time;
	RK_BOOL bExpBigChange = RK_FALSE;

	sem_init(&g_wakeup_nn_result_semaphore, 0, 0);
	g_wakeup_nn_flag = 1;

	// befor enter AOV
	if (g_enable_save_mp4)
		SAMPLE_COMM_AOV_StartRecordMp4(g_pst_muxer);
	for (int i = 0; i < g_u32BootFrame; i++) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(&ctx->vi[1], (void *)&pData);
		if (s32Ret == RK_SUCCESS) {
			SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
			u32Loopcount++;
		}
	}
	if (g_enable_save_mp4)
		SAMPLE_COMM_AOV_StopRecordMp4(g_pst_muxer);
	SAMPLE_COMM_AOV_GetGpioIrqStat(); // ignore previous input events.

	while (!quit) {
		// 1. Check input event to detect weather gpio irq is happened.
		is_gpioirq_happened = SAMPLE_COMM_AOV_GetGpioIrqStat();
		RK_LOGD("is_gpioirq_happened %d", is_gpioirq_happened);

		// 2. Get frame from VI and send frame to IVA
		SAMPLE_COMM_VI_GetChnFrame(&ctx->vi[1], (void *)&pData);
		stViFrame = (VIDEO_FRAME_INFO_S *)malloc(sizeof(VIDEO_FRAME_INFO_S));
		if (!stViFrame) {
			RK_LOGE("-----error malloc fail for stViFrame");
			SAMPLE_COMM_VI_ReleaseChnFrame(&ctx->vi[1]);
			continue;
		}
		memcpy(stViFrame, &ctx->vi[1].stViFrame, sizeof(VIDEO_FRAME_INFO_S));
		s32Fd = RK_MPI_MB_Handle2Fd(stViFrame->stVFrame.pMbBlk);
		memset(&ivaImage, 0, sizeof(RockIvaImage));
		ivaImage.info.transformMode = ctx->iva.eImageTransform;
		ivaImage.info.width = stViFrame->stVFrame.u32Width;
		ivaImage.info.height = stViFrame->stVFrame.u32Height;
		ivaImage.info.format = ctx->iva.eImageFormat;
		ivaImage.frameId = u32Loopcount;
		ivaImage.dataAddr = NULL;
		ivaImage.dataPhyAddr = NULL;
		ivaImage.dataFd = s32Fd;
		ivaImage.extData = stViFrame;
		clock_gettime(CLOCK_MONOTONIC, &iva_start_time);
		s32Ret = ROCKIVA_PushFrame(ctx->iva.ivahandle, &ivaImage, NULL);
		// 3. Wait IVA result callback
		sem_wait(&g_wakeup_nn_result_semaphore);
		clock_gettime(CLOCK_MONOTONIC, &iva_end_time);
		RK_LOGE("iva cost time %lld ms\n",
		        (iva_end_time.tv_sec - iva_start_time.tv_sec) * 1000LL +
		            (iva_end_time.tv_nsec - iva_start_time.tv_nsec) / 1000000LL);

		if (!g_multi_frame_enable) {
			g_wakeup_nn_flag = 0;
			is_gpioirq_happened = false;
		}

		// 4. Change ISP state if need.
		switch (wakeup_current_mode) {
		case SINGLE_FRAME_MODE:
			// The condition for switching to multi frame mode is:
			//	1) Have detected a human body.
			//	2) GPIO irq is happend.
			//  3) FastAE mode
			if (g_enable_fast_ae)
				bExpBigChange = SAMPLE_COMM_ISP_IsExpBigChange(0);
			if (g_wakeup_nn_flag > 0 || is_gpioirq_happened || bExpBigChange) {
				// to multi frame
				RK_LOGI("#Resume isp, Enter multi frame, wakeup because %s\n",
				        is_gpioirq_happened ? "gpio irq" : "detected human body");
				SAMPLE_COMM_ISP_MultiFrame(0);
				SAMPLE_COMM_IVA_SetWorkMode(&ctx->iva, ROCKIVA_MODE_VIDEO);
				wakeup_current_mode = MULTI_FRAME_MODE;
				if (is_gpioirq_happened) {
					wakeup_frame_count = g_max_wakeup_frame_count;
					is_gpioirq_happened = false;
				}
			}
			SAMPLE_COMM_RGN_DrawOsd(&ctx->rgn[1], RK_TRUE, 1000 / (float)s32SuspendTime);
			break;
		case MULTI_FRAME_MODE:
			// The condition for switching to single frame mode is:
			//	1) Have not detected human body too much times.
			//	2) GPIO irq is happend.
			//	3) wakeup_frame_count equals to zero.
			if (wakeup_frame_count > 0 && !is_gpioirq_happened) {
				--wakeup_frame_count;
				RK_LOGD("wakeup_frame_count %d", wakeup_frame_count);
			} else if (g_wakeup_nn_flag > 0 && !is_gpioirq_happened || bExpBigChange) {
				// keep multi frame mode
				if (!SAMPLE_COMM_AOV_IsRecordingMp4(g_pst_muxer) && g_enable_save_mp4)
					SAMPLE_COMM_AOV_StartRecordMp4(g_pst_muxer);
				usleep(u32GetOneFrameTime * 1000);
				RK_LOGD("detected human body, loop %d, sleep %d ms", u32Loopcount,
				        u32GetOneFrameTime);
				if (bExpBigChange && SAMPLE_COMM_ISP_IsExpConverge(0))
					bExpBigChange = RK_FALSE;
			} else if (g_wakeup_nn_flag == 0 || wakeup_frame_count == 0 ||
			           is_gpioirq_happened) {
				// to single frame
				RK_LOGI("#Pause isp, Enter single frame\n");
				if (SAMPLE_COMM_AOV_IsRecordingMp4(g_pst_muxer) && g_enable_save_mp4)
					SAMPLE_COMM_AOV_StopRecordMp4(g_pst_muxer);
				SAMPLE_COMM_ISP_SingleFrame(0);
				SAMPLE_COMM_IVA_SetWorkMode(&ctx->iva, ROCKIVA_MODE_PICTURE);
				wakeup_current_mode = SINGLE_FRAME_MODE;
				wakeup_frame_count = 0;
				// drop frame
				VIDEO_FRAME_INFO_S stViFrame_tmp;
				while (RK_MPI_VI_GetChnFrame(ctx->vi[1].s32DevId, ctx->vi[1].s32ChnId,
				                             &stViFrame_tmp, 1000) == RK_SUCCESS) {
					RK_MPI_VI_ReleaseChnFrame(ctx->vi[1].s32DevId, ctx->vi[1].s32ChnId,
					                          &stViFrame_tmp);
				}
			}
			SAMPLE_COMM_RGN_DrawOsd(&ctx->rgn[1], RK_FALSE,
			                        SAMPLE_COMM_ISP_GetFrameRate(0));
			break;
		}

		// 5. Enter sleep mode if in single frame mode.
		if (wakeup_current_mode == SINGLE_FRAME_MODE) {
			if (g_s32AovLoopCount != 0) {
				if (g_s32AovLoopCount > 0)
					--g_s32AovLoopCount;
				SAMPLE_COMM_AOV_EnterSleep();
			} else if (g_s32AovLoopCount == 0) {
				quit = true;
				RK_LOGI("Exit AOV!");
				break;
			}
		}
		SAMPLE_COMM_RGN_DrawRectFromIVA(&ctx->rgn[0], RK_TRUE);
		u32Loopcount++;
	}
	if (wakeup_current_mode == SINGLE_FRAME_MODE) {
		SAMPLE_COMM_ISP_MultiFrame(0);
		wakeup_current_mode = MULTI_FRAME_MODE;
	}
	RK_LOGE("vi_iva_thread exit !!!");
	return RK_NULL;
}
#endif

/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
int main(int argc, char *argv[]) {
	SAMPLE_MPI_CTX_S *ctx;
	int video_width = 1920;
	int video_height = 1080;
	RK_CHAR *pDeviceName = NULL;
	CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H265;
	VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H265VBR;
	RK_CHAR *pCodecName = "H264";
	RK_S32 s32CamId = 0;
	RK_S32 s32BitRate = 4 * 1024;
	MPP_CHN_S stSrcChn, stDestChn;
	RK_S32 s32AeMode = 0;
#if defined(RV1126)
	RK_U32 u32IvaWidth = 640;
	RK_U32 u32IvaHeight = 384;
#else
	RK_U32 u32IvaWidth = 512;
	RK_U32 u32IvaHeight = 288;
#endif
	RK_U32 u32IvaDetectFrameRate = 10;
	RK_CHAR *pIvaModelPath = "/oem/usr/lib/";
	RK_S32 s32Ret;
	RK_BOOL bWrapIfEnable = RK_FALSE;
	RK_U32 u32WrapLine = 4;
	RK_S32 u32QuickStart = 0;
	pthread_t vi_iva_thread_id;
	RK_S32 s32MirrorFlip = -1;
	RK_U32 u32Gop = 0;
	RK_S32 s32EnableBypass = 1;

	if (argc < 2) {
		print_usage(argv[0]);
		return 0;
	}

	ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
	memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

	signal(SIGINT, sigterm_handler);

	SAMPLE_COMM_KLOG("main 00");

#ifdef RKAIQ
	RK_BOOL bMultictx = RK_FALSE;
#endif
	int c;
	char *iq_file_dir = NULL;
	while ((c = getopt_long(argc, argv, optstr, long_options, NULL)) != -1) {
		const char *tmp_optarg = optarg;
		switch (c) {
		case 'a':
			if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
				tmp_optarg = argv[optind++];
			}
			if (tmp_optarg) {
				iq_file_dir = (char *)tmp_optarg;
			} else {
				iq_file_dir = NULL;
			}
			break;
		case 'b':
			s32BitRate = atoi(optarg);
			break;
		case 'd':
			pDeviceName = optarg;
			break;
		case 'e':
			if (!strcmp(optarg, "h264cbr")) {
				enCodecType = RK_CODEC_TYPE_H264;
				enRcMode = VENC_RC_MODE_H264CBR;
				pCodecName = "H264";
			} else if (!strcmp(optarg, "h264vbr")) {
				enCodecType = RK_CODEC_TYPE_H264;
				enRcMode = VENC_RC_MODE_H264VBR;
				pCodecName = "H264";
			} else if (!strcmp(optarg, "h264avbr")) {
				enCodecType = RK_CODEC_TYPE_H264;
				enRcMode = VENC_RC_MODE_H264AVBR;
				pCodecName = "H264";
			} else if (!strcmp(optarg, "h265cbr")) {
				enCodecType = RK_CODEC_TYPE_H265;
				enRcMode = VENC_RC_MODE_H265CBR;
				pCodecName = "H265";
			} else if (!strcmp(optarg, "h265vbr")) {
				enCodecType = RK_CODEC_TYPE_H265;
				enRcMode = VENC_RC_MODE_H265VBR;
				pCodecName = "H265";
			} else if (!strcmp(optarg, "h265avbr")) {
				enCodecType = RK_CODEC_TYPE_H265;
				enRcMode = VENC_RC_MODE_H265AVBR;
				pCodecName = "H265";
			} else if (!strcmp(optarg, "mjpegcbr")) {
				enCodecType = RK_CODEC_TYPE_MJPEG;
				enRcMode = VENC_RC_MODE_MJPEGCBR;
				pCodecName = "MJPEG";
			} else if (!strcmp(optarg, "mjpegvbr")) {
				enCodecType = RK_CODEC_TYPE_MJPEG;
				enRcMode = VENC_RC_MODE_MJPEGVBR;
				pCodecName = "MJPEG";
			} else {
				printf("ERROR: Invalid encoder type.\n");
				return 0;
			}
			break;
		case 'w':
			video_width = atoi(optarg);
			break;
		case 'h':
			video_height = atoi(optarg);
			break;
		case 'I':
			s32CamId = atoi(optarg);
			break;
		case 'r':
			bWrapIfEnable = atoi(optarg);
			break;
		case 'f':
			g_u32Fps = atoi(optarg);
			break;
		case 'g':
			u32Gop = atoi(optarg);
			break;
		case 'w' + 'l':
			if (0 == atoi(optarg)) {
				u32WrapLine = 1;
			} else if (1 == atoi(optarg)) {
				u32WrapLine = 2;
			} else if (2 == atoi(optarg)) {
				u32WrapLine = 4;
			} else if (3 == atoi(optarg)) {
				u32WrapLine = 8;
			} else {
				RK_LOGE("ERROR: Invalid WrapLine Value.");
			}
			break;
		case 'a' + 'm':
			s32AeMode = atoi(optarg);
			break;
		case 'a' + 'm' + 'c':
			g_s32AovLoopCount = atoi(optarg);
			break;
		case 's' + 't':
			s32SuspendTime = atoi(optarg);
			break;
		case 'i' + 'd' + 's':
			u32IvaDetectFrameRate = atoi(optarg);
			break;
		case 'e' + 'm' + 'f':
			g_multi_frame_enable = atoi(optarg);
			break;
		case 'b' + 'f':
			g_u32BootFrame = atoi(optarg);
			break;
		case 'e' + 's':
			g_enable_save_sd = atoi(optarg);
			break;
		case 'e' + 'm':
			g_enable_save_mp4 = atoi(optarg);
			break;
		case 'm' + 'w':
			g_max_wakeup_frame_count = atoi(optarg);
			break;
		case 'q' + 'k' + 's':
			u32QuickStart = atoi(optarg);
			break;
		case 'l' + 'w' + 'p':
			g_low_power_mode = atoi(optarg);
			break;
		case 'm' + 'p':
			s32MirrorFlip = atoi(optarg);
		case 'e' + 'b' + 'p':
			s32EnableBypass = atoi(optarg);
			break;
		case 'e' + 's' + 'p':
			s32EnableMultiCore = atoi(optarg);
			break;
		case 'f' + 'a':
			g_enable_fast_ae = atoi(optarg);
			break;
		case '?':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	printf("#CameraIdx: %d\n", s32CamId);
	printf("#pDeviceName: %s\n", pDeviceName);
	printf("#CodecName:%s\n", pCodecName);
	printf("#AOV loop count: %d\n", g_s32AovLoopCount);

	if (!s32EnableMultiCore)
		SAMPLE_COMM_AOV_DisableNonBootCPUs();
	// Unbind sdcard device and sound card device to reduce power consumption
	// in aov mode.
	SAMPLE_COMM_AOV_UnbindSdcard();
	SAMPLE_COMM_AOV_UnbindSoundcard();

	SAMPLE_COMM_AOV_Init();

	SAMPLE_COMM_KLOG("main 01");

	// setting fps and gop
#ifdef AOV_FASTBOOT_ENABLE
	RK_S32 rk_cam_fps = SAMPLE_COMM_ExtractValueFromCmdline("rk_cam_fps");
	if (rk_cam_fps != -1)
		g_u32Fps = rk_cam_fps;
#endif
	if (g_u32Fps == 0)
		g_u32Fps = 25;
	if (u32Gop == 0)
		u32Gop = g_u32Fps * 2;
	printf("Fps = %u, Gop = %u, mirror_flip = %d\n", g_u32Fps, u32Gop, s32MirrorFlip);
#ifdef RKAIQ
	printf("#bMultictx: %d\n\n", bMultictx);
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;

	SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, iq_file_dir);
	SAMPLE_COMM_ISP_SetFrameRate(s32CamId, g_u32Fps);
	if (s32MirrorFlip != -1)
		SAMPLE_COMM_ISP_SetMirrorFlip(s32CamId, s32MirrorFlip);
	SAMPLE_COMM_ISP_Run(s32CamId);
	SAMPLE_COMM_KLOG("main 02");
#endif

	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		goto __FAILED;
	}

#ifdef ROCKIVA
	/* Init iva */
	ctx->iva.pModelDataPath = pIvaModelPath;
	ctx->iva.u32ImageHeight = u32IvaHeight;
	ctx->iva.u32ImageWidth = u32IvaWidth;
	ctx->iva.u32DetectStartX = 0;
	ctx->iva.u32DetectStartY = 0;
	ctx->iva.u32DetectWidth = u32IvaWidth;
	ctx->iva.u32DetectHight = u32IvaHeight;
	ctx->iva.eImageTransform = ROCKIVA_IMAGE_TRANSFORM_NONE;
	ctx->iva.eImageFormat = ROCKIVA_IMAGE_FORMAT_YUV420SP_NV12;
#if defined(RV1126)
	ctx->iva.eModeType = ROCKIVA_DET_MODEL_CLS7;
#else
	ctx->iva.eModeType = ROCKIVA_DET_MODEL_PFP;
#endif
	ctx->iva.u32IvaDetectFrameRate = u32IvaDetectFrameRate;
	ctx->iva.detectResultCallback = rkIvaEvent_callback;
	ctx->iva.releaseCallback = rkIvaFrame_releaseCallBack;
	ctx->iva.eIvaMode = ROCKIVA_MODE_DETECT;
	s32Ret = SAMPLE_COMM_IVA_Create(&ctx->iva);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("SAMPLE_COMM_IVA_Create failure:%#X", s32Ret);
	}
#endif

	SAMPLE_COMM_KLOG("main 03");

	// Init VI[0]
	ctx->vi[0].bIfQuickStart = u32QuickStart;
#if defined(RV1126)
	// RV1126 not support RK_MPI_VI_EnableChnExt
	ctx->vi[0].bIfQuickStart = true;
#endif
	ctx->vi[0].u32Width = video_width;
	ctx->vi[0].u32Height = video_height;
	ctx->vi[0].s32DevId = s32CamId;
	ctx->vi[0].u32PipeId = ctx->vi[0].s32DevId;
	ctx->vi[0].s32ChnId = VI_MAIN_CHANNEL;
	ctx->vi[0].stChnAttr.stIspOpt.u32BufCount = 2;
	ctx->vi[0].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Width = video_width;
	ctx->vi[0].stChnAttr.stIspOpt.stMaxSize.u32Height = video_height;
	ctx->vi[0].stChnAttr.u32Depth = 0;
	if (g_low_power_mode)
		ctx->vi[0].stChnAttr.u32Depth = 2;
	ctx->vi[0].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[0].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[0].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	if (pDeviceName) {
		strcpy(ctx->vi[0].stChnAttr.stIspOpt.aEntityName, pDeviceName);
	}
	if (bWrapIfEnable) {
		ctx->vi[0].bWrapIfEnable = RK_TRUE;
		ctx->vi[0].u32BufferLine = ctx->vi[0].u32Height / u32WrapLine;
	}
	SAMPLE_COMM_VI_CreateChn(&ctx->vi[0]);

	SAMPLE_COMM_KLOG("main 04");

#ifdef ROCKIVA
	// Init VI[1]
	ctx->vi[1].bIfQuickStart = u32QuickStart;
#if defined(RV1126)
	// RV1126 not support RK_MPI_VI_EnableChnExt
	ctx->vi[1].bIfQuickStart = true;
#endif
	ctx->vi[1].u32Width = u32IvaWidth;
	ctx->vi[1].u32Height = u32IvaHeight;
	ctx->vi[1].s32DevId = s32CamId;
	ctx->vi[1].u32PipeId = ctx->vi[1].s32DevId;
	ctx->vi[1].s32ChnId = VI_IVA_CHANNEL;
	ctx->vi[1].stChnAttr.stIspOpt.u32BufCount = 2;
	ctx->vi[1].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi[1].stChnAttr.u32Depth = 2;
	ctx->vi[1].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi[1].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi[1].stChnAttr.stFrameRate.s32DstFrameRate = -1;
	if (pDeviceName) {
		strcpy(ctx->vi[1].stChnAttr.stIspOpt.aEntityName, pDeviceName);
	}
	SAMPLE_COMM_VI_CreateChn(&ctx->vi[1]);
#endif

	if (s32EnableBypass) {
		// Init VI[2]
		ctx->vi[2].bIfQuickStart = u32QuickStart;
		ctx->vi[2].u32Width = video_width;
		ctx->vi[2].u32Height = video_height;
		ctx->vi[2].s32DevId = s32CamId;
		ctx->vi[2].u32PipeId = ctx->vi[2].s32DevId;
		ctx->vi[2].s32ChnId = VI_BYPASS_CHANNEL;
		ctx->vi[2].stChnAttr.stIspOpt.u32BufCount = 1;
		ctx->vi[2].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
		ctx->vi[2].stChnAttr.stIspOpt.stMaxSize.u32Width = video_width;
		ctx->vi[2].stChnAttr.stIspOpt.stMaxSize.u32Height = video_height;
		ctx->vi[2].stChnAttr.u32Depth = 0;
		ctx->vi[2].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
		ctx->vi[2].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
		ctx->vi[2].stChnAttr.stFrameRate.s32DstFrameRate = -1;
		strcpy(ctx->vi[2].stChnAttr.stIspOpt.aEntityName, "rkisp_bypasspath");
		SAMPLE_COMM_VI_CreateChn(&ctx->vi[2]);
	}

	/* Init ivs */
	ctx->ivs.s32ChnId = 0;
	ctx->ivs.stIvsAttr.enMode = IVS_MODE_MD_OD;
	ctx->ivs.stIvsAttr.u32PicWidth = u32IvaWidth;
	ctx->ivs.stIvsAttr.u32PicHeight = u32IvaHeight;
	ctx->ivs.stIvsAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->ivs.stIvsAttr.s32Gop = u32Gop;
	ctx->ivs.stIvsAttr.bSmearEnable = RK_FALSE;
	ctx->ivs.stIvsAttr.bWeightpEnable = RK_FALSE;
	ctx->ivs.stIvsAttr.bMDEnable = RK_TRUE;
	ctx->ivs.stIvsAttr.s32MDInterval = 1;
	ctx->ivs.stIvsAttr.bMDNightMode = RK_TRUE;
	ctx->ivs.stIvsAttr.u32MDSensibility = 3;
	ctx->ivs.stIvsAttr.bODEnable = RK_FALSE;
	ctx->ivs.stIvsAttr.s32ODInterval = 1;
	ctx->ivs.stIvsAttr.s32ODPercent = 7;
	s32Ret = SAMPLE_COMM_IVS_Create(&ctx->ivs);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("SAMPLE_COMM_IVS_Create failure:%X", s32Ret);
	}

	SAMPLE_COMM_KLOG("main 05");

	// Init VENC[0]
	ctx->venc[0].s32ChnId = VENC_MAIN_CHNNAL;
	ctx->venc[0].u32Width = video_width;
	ctx->venc[0].u32Height = video_height;
	ctx->venc[0].u32Fps = g_u32Fps;
	ctx->venc[0].u32Gop = u32Gop;
	ctx->venc[0].u32BitRate = s32BitRate;
	ctx->venc[0].enCodecType = enCodecType;
	ctx->venc[0].enRcMode = enRcMode;
	ctx->venc[0].getStreamCbFunc = venc_get_stream;
#if defined(RV1106)
	ctx->venc[0].enable_buf_share = 1;
#endif
	// H264  66：Baseline  77：Main Profile 100：High Profile
	// H265  0：Main Profile  1：Main 10 Profile
	// MJPEG 0：Baseline
	ctx->venc[0].stChnAttr.stVencAttr.u32Profile = 100;
	ctx->venc[0].stChnAttr.stGopAttr.enGopMode =
	    VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
	if (bWrapIfEnable) {
		ctx->venc[0].bWrapIfEnable = RK_TRUE;
		ctx->venc[0].u32BufferLine = ctx->venc[0].u32Height / u32WrapLine;
	}
	SAMPLE_COMM_VENC_CreateChn(&ctx->venc[0]);

	if (s32EnableBypass) {
		ctx->venc[1].s32ChnId = VENC_BYPASS_CHNNAL;
		ctx->venc[1].u32Width = video_width;
		ctx->venc[1].u32Height = video_height;
		ctx->venc[1].u32Fps = g_u32Fps;
		ctx->venc[1].u32Gop = u32Gop;
		ctx->venc[1].u32BitRate = s32BitRate;
		ctx->venc[1].enCodecType = enCodecType;
		ctx->venc[1].enRcMode = enRcMode;
		ctx->venc[1].getStreamCbFunc = venc_get_bypass_stream;
		ctx->venc[1].enable_buf_share = 1;
		// H264  66：Baseline  77：Main Profile 100：High Profile
		// H265  0：Main Profile  1：Main 10 Profile
		// MJPEG 0：Baseline
		ctx->venc[1].stChnAttr.stVencAttr.u32Profile = 100;
		ctx->venc[1].stChnAttr.stGopAttr.enGopMode =
		    VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
		SAMPLE_COMM_VENC_CreateChn(&ctx->venc[1]);
	}

	SAMPLE_COMM_KLOG("main 06");

	// Init RGN[0]
	ctx->rgn[0].rgnHandle = 1;
	ctx->rgn[0].stRgnAttr.enType = OVERLAY_RGN;
	ctx->rgn[0].bDrawBmpManual = RK_TRUE;
	ctx->rgn[0].stMppChn.enModId = RK_ID_VENC;
	ctx->rgn[0].stMppChn.s32ChnId = RGN_CHANNEL;
	ctx->rgn[0].stMppChn.s32DevId = 0;
	ctx->rgn[0].stRegion.s32X = 0;                 // must be 16 aligned
	ctx->rgn[0].stRegion.s32Y = 0;                 // must be 16 aligned
	ctx->rgn[0].stRegion.u32Width = video_width;   // must be 16 aligned
	ctx->rgn[0].stRegion.u32Height = video_height; // must be 16 aligned
#if defined(RV1126)
	ctx->rgn[0].u32BmpFormat = RK_FMT_8BPP;
#else
	ctx->rgn[0].u32BmpFormat = RK_FMT_2BPP;
#endif
	ctx->rgn[0].u32BgAlpha = 0;
	ctx->rgn[0].u32FgAlpha = 255;
	ctx->rgn[0].u32Layer = 1;
	ctx->rgn[0].stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_0] =
	    BLUE_COLOR;
	ctx->rgn[0].stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_1] =
	    RED_COLOR;
	SAMPLE_COMM_RGN_CreateChn(&ctx->rgn[0]);

	// Init RGN[1]
	ctx->rgn[1].rgnHandle = 2;
	ctx->rgn[1].stRgnAttr.enType = OVERLAY_RGN;
	ctx->rgn[1].bDrawBmpManual = RK_TRUE;
	ctx->rgn[1].stMppChn.enModId = RK_ID_VENC;
	ctx->rgn[1].stMppChn.s32ChnId = RGN_CHANNEL;
	ctx->rgn[1].stMppChn.s32DevId = 0;
	ctx->rgn[1].stRegion.s32X = 0;       // must be 16 aligned
	ctx->rgn[1].stRegion.s32Y = 0;       // must be 16 aligned
	ctx->rgn[1].stRegion.u32Width = 576; // must be 16 aligned
	ctx->rgn[1].stRegion.u32Height = 32; // must be 16 aligned
	ctx->rgn[1].u32BmpFormat = RK_FMT_ARGB8888;
	ctx->rgn[1].u32BgAlpha = 64;
	ctx->rgn[1].u32FgAlpha = 64;
	ctx->rgn[1].u32Layer = 2;
	ctx->rgn[1].st_osd_data.enable = 1; // enable time osd
	SAMPLE_COMM_RGN_CreateChn(&ctx->rgn[1]);

	// Bind VI[0] and VENC
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[0].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[0].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc[0].s32ChnId;
	if (!g_low_power_mode)
		SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

	if (s32EnableBypass) {
		stSrcChn.enModId = RK_ID_VI;
		stSrcChn.s32DevId = ctx->vi[2].s32DevId;
		stSrcChn.s32ChnId = ctx->vi[2].s32ChnId;
		stDestChn.enModId = RK_ID_VENC;
		stDestChn.s32DevId = 0;
		stDestChn.s32ChnId = ctx->venc[1].s32ChnId;
		SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
	}

	SAMPLE_COMM_KLOG("main 07");

	if (!u32QuickStart)
		RK_MPI_VI_StartPipe(ctx->vi[0].u32PipeId);

	// init rtsp
	g_rtsplive = create_rtsp_demo(554);
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
	if (enCodecType == RK_CODEC_TYPE_H264) {
		rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
	} else if (enCodecType == RK_CODEC_TYPE_H265) {
		rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H265, NULL, 0);
	} else {
		printf("not support other type\n");
		return -1;
	}
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

	SAMPLE_COMM_KLOG("main 08");

	SAMPLE_COMM_AOV_SetSuspendTime(s32SuspendTime);

	printf("%s initial finish\n", __func__);
#ifdef ROCKIVA
	// /* VI[1] IVA thread launch */
	if (g_low_power_mode)
		pthread_create(&vi_iva_thread_id, 0, vi_iva_low_power_thread, (void *)ctx);
	else
		pthread_create(&vi_iva_thread_id, 0, vi_iva_thread, (void *)ctx);
#endif

	while (!quit) {
		sleep(1);
	}

	printf("%s exit!\n", __func__);

#ifdef ROCKIVA
	/* Destroy IVA */
	pthread_join(vi_iva_thread_id, RK_NULL);
	SAMPLE_COMM_IVA_Destroy(&ctx->iva);
#endif
	/* ivs chn destroy*/
	s32Ret = RK_MPI_IVS_DestroyChn(ctx->ivs.s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_DestroyChn failure:%X", s32Ret);
	}

	if (ctx->venc[0].getStreamCbFunc) {
		pthread_join(ctx->venc[0].getStreamThread, NULL);
	}
	if (s32EnableBypass)
		pthread_join(ctx->venc[1].getStreamThread, NULL);

	SAMPLE_COMM_RGN_DestroyChn(&ctx->rgn[0]);
	SAMPLE_COMM_RGN_DestroyChn(&ctx->rgn[1]);

	if (g_rtsplive)
		rtsp_del_demo(g_rtsplive);

	// UnBind VI[0] and VENC[0]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi[0].s32DevId;
	stSrcChn.s32ChnId = ctx->vi[0].s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc[0].s32ChnId;
	if (!g_low_power_mode)
		SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

	if (s32EnableBypass) {
		stSrcChn.enModId = RK_ID_VI;
		stSrcChn.s32DevId = ctx->vi[2].s32DevId;
		stSrcChn.s32ChnId = ctx->vi[2].s32ChnId;
		stDestChn.enModId = RK_ID_VENC;
		stDestChn.s32DevId = 0;
		stDestChn.s32ChnId = ctx->venc[1].s32ChnId;
		SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);
	}

	// Destroy VENC[0]
	SAMPLE_COMM_VENC_DestroyChn(&ctx->venc[0]);
	if (s32EnableBypass)
		SAMPLE_COMM_VENC_DestroyChn(&ctx->venc[1]);

	RK_MPI_VI_DisableChn(ctx->vi[0].u32PipeId, ctx->vi[0].s32ChnId);
#ifdef ROCKIVA
	RK_MPI_VI_DisableChn(ctx->vi[1].u32PipeId, ctx->vi[1].s32ChnId);
#endif
	if (s32EnableBypass)
		RK_MPI_VI_DisableChn(ctx->vi[2].u32PipeId, ctx->vi[2].s32ChnId);

	RK_MPI_VI_DisableDev(0);
__FAILED:
	RK_MPI_SYS_Exit();
#ifdef RKAIQ
	SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
	SAMPLE_COMM_AOV_Deinit();
	if (!s32EnableMultiCore)
		SAMPLE_COMM_AOV_EnableNonBootCPUs();
	if (ctx) {
		free(ctx);
		ctx = RK_NULL;
	}

	return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
