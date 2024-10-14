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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>
#include <linux/input.h>

#include "rtsp_demo.h"
#include "sample_comm.h"
#include "sample_comm_aov.h"

#define VI_MAIN_CHANNEL 0
#define VENC_MAIN_CHNNAL 0

rtsp_demo_handle g_rtsplive = NULL;
static rtsp_session_handle g_rtsp_session;
static RK_U32 g_u32BootFrame = 60;
static RK_S32 g_s32AovLoopCount = -1;
static RK_S32 g_enable_save_sd = 1;

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi;
	SAMPLE_VENC_CTX_S venc;
	// SAMPLE_RGN_CTX_S rgn[2];
} SAMPLE_MPI_CTX_S;

static bool quit = false;
static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	quit = true;
}

static RK_CHAR optstr[] = "?::a::b:w:h:l:o:e:d:D:I:i:L:M:r:";
static const struct option long_options[] = {
    {"aiq", optional_argument, NULL, 'a'},
    {"bitrate", required_argument, NULL, 'b'},
    {"device_name", required_argument, NULL, 'd'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"input_bmp_name", required_argument, NULL, 'i'},
    {"loop_count", required_argument, NULL, 'l'},
    {"output_path", required_argument, NULL, 'o'},
    {"encode", required_argument, NULL, 'e'},
    {"disp_devid", required_argument, NULL, 'D'},
    {"camid", required_argument, NULL, 'I'},
    {"multictx", required_argument, NULL, 'M'},
    {"fps", required_argument, NULL, 'f'},
    {"wrap", required_argument, RK_NULL, 'r'},
    {"hdr_mode", required_argument, NULL, 'h' + 'm'},
    {"ae_mode", required_argument, NULL, 'a' + 'm'},
    {"aov_loop_count", required_argument, NULL, 'a' + 'm' + 'c'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"boot_frame", required_argument, NULL, 'b' + 'f'},
    {"wrap_lines", required_argument, RK_NULL, 'w' + 'l'},
    {"enable_save_sdcard", required_argument, RK_NULL, 'e' + 's'},
    {"quick_start", required_argument, NULL, 'q' + 's'},
    {"help", optional_argument, NULL, '?'},
    {NULL, 0, NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("usage example:\n");
	printf("\t%s -w 1920 -h 1080 -a /etc/iqfiles/ -I 0 -e h264cbr -b 4096 "
	       "-o /data/\n",
	       name);
	printf("\trtsp://xx.xx.xx.xx/live/0, Default OPEN\n");
#ifdef RKAIQ
	printf("\t-a | --aiq: enable aiq with dirpath provided, eg:-a "
	       "/etc/iqfiles/, "
	       "set dirpath empty to using path by default, without this option aiq "
	       "should run in other application\n");
	printf("\t-M | --multictx: switch of multictx in isp, set 0 to disable, set "
	       "1 to enable. Default: 0\n");
#endif
	printf("\t-d | --device_name: set pcDeviceName, eg: /dev/video0 Default "
	       "NULL\n");
	printf("\t-I | --camid: camera ctx id, Default 0\n");
	printf("\t-w | --width: camera with, Default 1920\n");
	printf("\t-h | --height: camera height, Default 1080\n");
	printf("\t-r | --wrap : wrap for mainStream, 0: close 1: open, Default: 0\n");
	printf("\t-e | --encode: encode type, Default:h265vbr, Value:h264cbr, "
	       "h264vbr, h264avbr "
	       "h265cbr, h265vbr, h265avbr, mjpegcbr, mjpegvbr\n");
	printf("\t-b | --bitrate: encode bitrate, Default 4096\n");
	printf("\t-i | --input_bmp_name: input file path of logo.bmp, Default NULL\n");
	printf("\t-l | --loop_count: loop count, Default -1\n");
	printf("\t-o | --output_path: encode save file path, Default /data/\n");
	printf("\t--enable_save_sdcard : enable save venc stream to sdcard, default: 1\n");
	printf("\t--wrap_lines : 0: height/2, 1: height/4, 2: height/8. default: 1\n");
	printf("\t--ae_mode: set aov ae wakeup mode, 0: MD wakupe: 1: always wakeup, 2: no "
	       "wakeup, Default: 0\n");
	printf("\t--aov_loop_count: set aov wakeup loop count, Default: -1(unlimit)\n");
	printf("\t--mcu_max_run_count: set aov mcu wakeup time, Default: -1\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--boot_frame: How long will it take to enter AOV mode after boot, Default: "
	       "60 frames\n");
	printf("\t--quick_start: quick start stream, Default: 0\n");
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
	RK_S32 aov_got_idr = 0;
	RK_S32 force_flush_to_storage = 0;
	RK_S32 venc_drop_frame_count = 0;
	bool is_gpioirq_happened = false;

	char *venc_data =
	    (char *)malloc(AOV_STREAM_SIZE_WRITE_TO_SDCARD); // 10M form 200 frame
	// befor enter AOV
	for (int i = 0; i < g_u32BootFrame; i++) {
		s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

			RK_LOGD("boot frame, chn:%d, loopCount:%d, len:%u, pts:%llu, seq:%u",
			        ctx->s32ChnId, i, ctx->stFrame.pstPack->u32Len,
			        ctx->stFrame.pstPack->u64PTS, ctx->stFrame.u32Seq);
			SAMPLE_COMM_AOV_DumpPtsToTMP(ctx->stFrame.u32Seq,
			                             ctx->stFrame.pstPack->u64PTS, g_u32BootFrame);
			if (venc_data_size <= AOV_STREAM_SIZE_WRITE_TO_SDCARD) {
				memcpy(venc_data + venc_data_size, pData, ctx->stFrame.pstPack->u32Len);
				venc_data_size += ctx->stFrame.pstPack->u32Len;
			} else {
				SAMPLE_COMM_AOV_CopyRawStreamToSdcard(ctx->s32ChnId, venc_data,
				                                      venc_data_size, pData,
				                                      ctx->stFrame.pstPack->u32Len);
				venc_data_size = 0;
				RK_MPI_VENC_RequestIDR(ctx->s32ChnId, RK_FALSE);
			}
			SAMPLE_COMM_VENC_ReleaseStream(ctx);
		}
	}

	SAMPLE_COMM_ISP_SingleFrame(0);
	// drop frame
	VENC_STREAM_S stFrame_tmp;
	stFrame_tmp.pstPack = (VENC_PACK_S *)(malloc(sizeof(VENC_PACK_S)));
	while (RK_MPI_VENC_GetStream(ctx->s32ChnId, &stFrame_tmp, 1000) == RK_SUCCESS) {
		s32Ret = RK_MPI_VENC_ReleaseStream(ctx->s32ChnId, &stFrame_tmp);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
			abort();
		}
	}
	free(stFrame_tmp.pstPack);

	RK_MPI_VENC_RequestIDR(ctx->s32ChnId, RK_FALSE);
	SAMPLE_COMM_AOV_EnterSleep();

	while (!quit) {
		// 1. Check input event to detect weather gpio irq is happened.
		is_gpioirq_happened = SAMPLE_COMM_AOV_GetGpioIrqStat();
		RK_LOGD("is_gpioirq_happened %d", is_gpioirq_happened);
		printf("is_gpioirq_happened %d", is_gpioirq_happened);

		s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

			if (g_enable_save_sd) {
				if ((venc_data_size + ctx->stFrame.pstPack->u32Len) <=
				        AOV_STREAM_SIZE_WRITE_TO_SDCARD &&
				    !is_gpioirq_happened) {
					if (aov_got_idr == 0 &&
					    ctx->stFrame.pstPack->DataType.enH265EType == H265E_NALU_PSLICE) {
						// force idr frame
						RK_LOGI("work round force idr, skip...\n");
					} else {
						if (!aov_got_idr)
							aov_got_idr = 1;
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
					aov_got_idr = 0;
				}
			}

#if 0
			PrintStreamDetails(ctx->s32ChnId, ctx->stFrame.pstPack->u32Len);
			rtsp_tx_video(g_rtsp_session, pData, ctx->stFrame.pstPack->u32Len,
			              ctx->stFrame.pstPack->u64PTS);
			rtsp_do_event(g_rtsplive);
#endif

			RK_LOGD("chn:%d, loopCount:%d, len:%u, pts:%llu, seq:%u", ctx->s32ChnId,
			        loopCount, ctx->stFrame.pstPack->u32Len, ctx->stFrame.pstPack->u64PTS,
			        ctx->stFrame.u32Seq);

			SAMPLE_COMM_VENC_ReleaseStream(ctx);

			if (g_s32AovLoopCount != 0) {
				if (g_s32AovLoopCount > 0)
					--g_s32AovLoopCount;
				SAMPLE_COMM_AOV_GetGpioIrqStat(); // ignore previous input events
				SAMPLE_COMM_AOV_EnterSleep();
			} else {
				quit = true;
				RK_LOGI("Exit AOV!");
				break;
			}

			loopCount++;
			venc_drop_frame_count = 0;
		} else {
			RK_LOGE("venc drop frame, force to Sleep, venc_drop_frame_count = %d\n",
			        venc_drop_frame_count);
			venc_drop_frame_count++;
			if (venc_drop_frame_count < 10)
				SAMPLE_COMM_AOV_EnterSleep();
		}
	}

	SAMPLE_COMM_ISP_MultiFrame(0);
	if (g_enable_save_sd && venc_data && venc_data_size > 0) {
		SAMPLE_COMM_AOV_CopyRawStreamToSdcard(ctx->s32ChnId, venc_data, venc_data_size,
		                                      NULL, 0);
		venc_data_size = 0;
	}
	if (venc_data)
		free(venc_data);

	return RK_NULL;
}

/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
int main(int argc, char *argv[]) {
	SAMPLE_MPI_CTX_S *ctx;
	int video_width = 1920;
	int video_height = 1080;
	RK_CHAR *pDeviceName = NULL;
	RK_CHAR *pInPathBmp = NULL;
	RK_CHAR *pOutPathVenc = NULL;
	CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H265;
	VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H265VBR;
	RK_CHAR *pCodecName = "H264";
	RK_S32 s32CamId = 0;
	RK_S32 s32DisId = -1;
	RK_S32 s32DisLayerId = 0;
	RK_S32 s32loopCnt = -1;
	RK_S32 s32BitRate = 4 * 1024;
	MPP_CHN_S stSrcChn, stDestChn;
	RK_S32 s32AeMode = 0;
	RK_S32 s32SuspendTime = 1000;
	RK_BOOL bWrapIfEnable = RK_FALSE;
	RK_U32 u32WrapLine = 4;
	RK_S32 u32QuickStart = 0;
	RK_S32 s32Ret;

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
		case 'i':
			pInPathBmp = optarg;
			break;
		case 'l':
			s32loopCnt = atoi(optarg);
			break;
		case 'L':
			s32DisLayerId = atoi(optarg);
			break;
		case 'o':
			pOutPathVenc = optarg;
			break;
		case 'r':
			bWrapIfEnable = atoi(optarg);
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
		case 'b' + 'f':
			g_u32BootFrame = atoi(optarg);
			break;
		case 'e' + 's':
			g_enable_save_sd = atoi(optarg);
			break;
		case 'w' + 'l':
			if (0 == atoi(optarg)) {
				u32WrapLine = 2;
			} else if (1 == atoi(optarg)) {
				u32WrapLine = 4;
			} else if (2 == atoi(optarg)) {
				u32WrapLine = 8;
			} else {
				RK_LOGE("ERROR: Invalid WrapLine Value.");
			}
			break;
#ifdef RKAIQ
		case 'M':
			if (atoi(optarg)) {
				bMultictx = RK_TRUE;
			}
			break;
#endif
		case 'q' + 's':
			u32QuickStart = atoi(optarg);
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
	printf("#Output Path: %s\n", pOutPathVenc);

	SAMPLE_COMM_AOV_Init();

	SAMPLE_COMM_KLOG("main 01");

#ifdef RKAIQ
	printf("#bMultictx: %d\n\n", bMultictx);
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;

	SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, iq_file_dir);
	SAMPLE_COMM_ISP_Run(s32CamId);
	SAMPLE_COMM_KLOG("main 02");
#endif

	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		goto __FAILED;
	}

	// Init VI[0]
	ctx->vi.bIfQuickStart = u32QuickStart;
#if defined(RV1126)
	// RV1126 not support RK_MPI_VI_EnableChnExt
	ctx->vi.bIfQuickStart = true;
#endif
	ctx->vi.u32Width = video_width;
	ctx->vi.u32Height = video_height;
	ctx->vi.s32DevId = s32CamId;
	ctx->vi.u32PipeId = ctx->vi.s32DevId;
	ctx->vi.s32ChnId = VI_MAIN_CHANNEL;
	if (bWrapIfEnable) {
		ctx->vi.bWrapIfEnable = RK_TRUE;
		ctx->vi.u32BufferLine = ctx->vi.u32Height / u32WrapLine;
	}
	ctx->vi.stChnAttr.stIspOpt.u32BufCount = 2;
	ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi.stChnAttr.stIspOpt.stMaxSize.u32Width = video_width;
	ctx->vi.stChnAttr.stIspOpt.stMaxSize.u32Height = video_height;
	ctx->vi.stChnAttr.u32Depth = 0;
	ctx->vi.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
	if (pDeviceName) {
		strcpy(ctx->vi.stChnAttr.stIspOpt.aEntityName, pDeviceName);
	}
	SAMPLE_COMM_KLOG("main 03");
	SAMPLE_COMM_VI_CreateChn(&ctx->vi);

	// Init VENC[0]
	ctx->venc.s32ChnId = VENC_MAIN_CHNNAL;
	ctx->venc.u32Width = video_width;
	ctx->venc.u32Height = video_height;
	ctx->venc.u32Fps = 10;
	ctx->venc.u32Gop = 20;
	ctx->venc.u32BitRate = s32BitRate;
	ctx->venc.enCodecType = enCodecType;
	ctx->venc.enRcMode = enRcMode;
	ctx->venc.getStreamCbFunc = venc_get_stream;
	ctx->venc.s32loopCount = s32loopCnt;
	ctx->venc.dstFilePath = pOutPathVenc;
	// H264  66：Baseline  77：Main Profile 100：High Profile
	// H265  0：Main Profile  1：Main 10 Profile
	// MJPEG 0：Baseline
	ctx->venc.stChnAttr.stVencAttr.u32Profile = 100;
	ctx->venc.stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
	if (bWrapIfEnable) {
		ctx->venc.bWrapIfEnable = RK_TRUE;
		ctx->venc.u32BufferLine = ctx->venc.u32Height / u32WrapLine;
	}
	SAMPLE_COMM_KLOG("main 04");
	SAMPLE_COMM_VENC_CreateChn(&ctx->venc);

	// Bind VI[0] and VENC
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi.s32DevId;
	stSrcChn.s32ChnId = ctx->vi.s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc.s32ChnId;
	SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

	SAMPLE_COMM_KLOG("main 05");

	if (!u32QuickStart)
		RK_MPI_VI_StartPipe(ctx->vi.u32PipeId);

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

	SAMPLE_COMM_KLOG("main 06");

	SAMPLE_COMM_AOV_SetSuspendTime(s32SuspendTime);

	printf("%s initial finish\n", __func__);

	while (!quit) {
		sleep(1);
	}

	printf("%s exit!\n", __func__);

	if (ctx->venc.getStreamCbFunc) {
		pthread_join(ctx->venc.getStreamThread, NULL);
	}

	if (g_rtsplive)
		rtsp_del_demo(g_rtsplive);

	// UnBind VI[0] and VENC[0]
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi.s32DevId;
	stSrcChn.s32ChnId = ctx->vi.s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc.s32ChnId;
	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

	// Destroy VENC[0]
	SAMPLE_COMM_VENC_DestroyChn(&ctx->venc);
	// Destroy VI[0]
	SAMPLE_COMM_VI_DestroyChn(&ctx->vi);
__FAILED:
	RK_MPI_SYS_Exit();
#ifdef RKAIQ
	SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
	SAMPLE_COMM_AOV_Deinit();
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
