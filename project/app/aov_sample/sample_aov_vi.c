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

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi;
} SAMPLE_MPI_CTX_S;

enum ISP_MODE {
	SINGLE_FRAME_MODE,
	MULTI_FRAME_MODE,
};

static bool quit = false;
static RK_U32 g_u32BootFrame = 60;
static RK_S32 g_s32AovLoopCount = -1;
static RK_BOOL g_bEnableDummyFrame = RK_TRUE;
static RK_U32 g_u32DummyFrameCnt = 3;
static RK_S32 g_exit_result = RK_SUCCESS;

static void program_handle_error(const char *func, RK_U32 line) {
	RK_LOGE("func: <%s> line: <%d> error exit!", func, line);
	g_exit_result = RK_FAILURE;
	quit = RK_TRUE;
}

static void program_normal_exit(const char *func, RK_U32 line) {
	RK_LOGE("func: <%s> line: <%d> normal exit!", func, line);
	g_exit_result = RK_SUCCESS;
	quit = RK_TRUE;
}

static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	quit = true;
}

static RK_CHAR optstr[] = "?::a::d:c:f:w:h:o:I:l:m:";
static const struct option long_options[] = {
    {"aiq", optional_argument, NULL, 'a'},
    {"device_name", required_argument, NULL, 'd'},
    {"chn_id", required_argument, NULL, 'c'},
    {"pixel_format", optional_argument, NULL, 'f'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"output_path", required_argument, NULL, 'o'},
    {"camid", required_argument, NULL, 'I'},
    {"hdr_mode", required_argument, NULL, 'h' + 'm'},
    {"vi_frame_mode", required_argument, NULL, 'v' + 'f' + 'm'},
    {"ae_mode", required_argument, NULL, 'a' + 'm'},
    {"aov_loop_count", required_argument, NULL, 'a' + 'm' + 'c'},
    {"suspend_time", required_argument, NULL, 's' + 't'},
    {"boot_frame", required_argument, NULL, 'b' + 'f'},
    {"enable_dummy_frame", required_argument, NULL, 'e' + 'd' + 'f'},
    {"dummy_frame_cnt", required_argument, NULL, 'd' + 'f' + 'c'},
    {"help", optional_argument, NULL, '?'},
    {NULL, 0, NULL, 0},
};

/******************************************************************************
 * function : show usage
 ******************************************************************************/
static void print_usage(const RK_CHAR *name) {
	printf("usage example:\n");
	printf("\t%s -w 1920 -h 1080 -a /etc/iqfiles/ --aov_loop_count 10 "
	       "-o /data/\n",
	       name);
#ifdef RKAIQ
	printf("\t-a | --aiq: enable aiq with dirpath provided, eg:-a /etc/iqfiles/, "
	       "set dirpath empty to using path by default, without this option aiq "
	       "should run in other application\n");
#endif
	printf("\t-d | --device_name: set pcDeviceName, eg: /dev/video0 Default "
	       "NULL\n");
	printf("\t-c | --chn_id: channel id, default: 1\n");
	printf("\t-f | --pixel_format: camera Format, Default nv12, "
	       "Value:nv12,nv16,uyvy,yuyv.\n");
	printf("\t-w | --width: camera with, Default 1920\n");
	printf("\t-h | --height: camera height, Default 1080\n");
	printf("\t-o | --output_path: vi output file path, Default NULL\n");
	printf("\t-I | --camid: camera ctx id, Default 0\n");
	printf("\t--hdr_mode: set hdr mode, 0: normal 1: HDR2, 2: HDR3, Default: 0\n");
	printf("\t--vi_frame_mode: set vi frame mode, 0: single mode 1: multi -> single -> "
	       "multi, Default: 0\n");
	printf("\t--ae_mode: set aov ae wakeup mode, 0: MD wakupe: 1: always wakeup, 2: no "
	       "wakeup, Default: 0\n");
	printf("\t--aov_loop_count: set aov wakeup loop count, Default: -1(unlimit)\n");
	printf("\t--suspend_time: set aov suspend time, Default: 1000ms\n");
	printf("\t--boot_frame: How long will it take to enter AOV mode after boot, Default: "
	       "60 frames\n");
	printf("\t--enable_dummy_frame: Enable fetch dummy frame in single frame mode"
	       ", Default: 1\n");
	printf("\t--dummy_frame_cnt: Dummy frame count in single frame mode"
	       ", Default: 3\n");
}

/******************************************************************************
 * function : vi thread
 ******************************************************************************/
static void *vi_get_multi_stream(void *pArgs) {
	SAMPLE_VI_CTX_S *ctx = (SAMPLE_VI_CTX_S *)(pArgs);
	RK_S32 s32Ret = RK_FAILURE;
	char name[256] = {0};
	FILE *fp = RK_NULL;
	void *pData = RK_NULL;
	RK_S32 loopCount = 0;
	RK_S32 waitTime = -1;
	RK_S32 wakeup_multi_fps = 20;
	enum ISP_MODE wakeup_current_mode = MULTI_FRAME_MODE;
	RK_U32 size = 0;

	if (ctx->dstFilePath) {
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", ctx->dstFilePath, ctx->s32DevId);
		fp = fopen(name, "wb");
		if (fp == RK_NULL) {
			printf("chn %d can't open %s file !\n", ctx->s32DevId, ctx->dstFilePath);
			quit = true;
			return RK_NULL;
		}
	}

	// befor enter AOV
	for (int i = 0; i < g_u32BootFrame; i++) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

			size = ctx->stViFrame.stVFrame.u32VirWidth *
			       ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
			if (fp) {
				fwrite(pData, 1, size, fp);
				fflush(fp);
			}
			RK_LOGI(
			    "SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u loop:%d seq:%d "
			    "pts:%lld ms\n",
			    ctx->s32DevId, pData, size, loopCount, ctx->stViFrame.stVFrame.u32TimeRef,
			    ctx->stViFrame.stVFrame.u64PTS / 1000);
			SAMPLE_COMM_VI_ReleaseChnFrame(ctx);
			loopCount++;
		}
	}

	while (!quit) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {
			size = ctx->stViFrame.stVFrame.u32VirWidth *
			       ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
			if (fp) {
				fwrite(pData, 1, size, fp);
				fflush(fp);
			}

			RK_LOGI(
			    "SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u loop:%d seq:%d "
			    "pts:%lld ms\n",
			    ctx->s32DevId, pData, size, loopCount, ctx->stViFrame.stVFrame.u32TimeRef,
			    ctx->stViFrame.stVFrame.u64PTS / 1000);
			SAMPLE_COMM_VI_ReleaseChnFrame(ctx);

#if defined(RV1106)
			if (g_bEnableDummyFrame && wakeup_current_mode == SINGLE_FRAME_MODE) {
				for (int i = 0; i != g_u32DummyFrameCnt; ++i) {
					RK_MPI_VI_DevEnableSinglelFrame(ctx->s32DevId, 1);
					s32Ret = RK_MPI_VI_GetChnFrame(ctx->u32PipeId, ctx->s32ChnId,
					                               &ctx->stViFrame, 1000);
					if (s32Ret == RK_SUCCESS) {
						RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
						        ctx->s32DevId, ctx->stViFrame.stVFrame.u32TimeRef,
						        ctx->stViFrame.stVFrame.u64PTS / 1000);
						RK_MPI_VI_ReleaseChnFrame(ctx->u32PipeId, ctx->s32ChnId,
						                          &ctx->stViFrame);
					} else {
						RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
						program_handle_error(__FUNCTION__, __LINE__);
						break;
					}
				}
			}
#endif

			if (wakeup_current_mode == MULTI_FRAME_MODE) {
				RK_LOGI("#Pause isp, Enter single frame\n");
				SAMPLE_COMM_ISP_SingleFrame(ctx->s32DevId);
				// drop frame
				VIDEO_FRAME_INFO_S stViFrame_tmp;
				while (RK_MPI_VI_GetChnFrame(ctx->s32DevId, ctx->s32ChnId, &stViFrame_tmp,
				                             1000) == RK_SUCCESS) {
					RK_MPI_VI_ReleaseChnFrame(ctx->s32DevId, ctx->s32ChnId,
					                          &stViFrame_tmp);
				}
				wakeup_current_mode = SINGLE_FRAME_MODE;
			}

			if (loopCount % 5 == 0) {
				RK_LOGI("#Resume isp, Enter multi frame\n");
				SAMPLE_COMM_ISP_MultiFrame(ctx->s32DevId);

				// drop frame
				RK_LOGI("#Resume isp, Enter multi frame\n");
				VIDEO_FRAME_INFO_S stViFrame_tmp;
				for (int i = 0; i < 60; i++) {
					int ret;
					ret = RK_MPI_VI_GetChnFrame(ctx->s32DevId, ctx->s32ChnId,
					                            &stViFrame_tmp, -1);
					if (ret != RK_SUCCESS) {
						RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", ret);
						abort();
					}
					RK_LOGI("Multi RK_MPI_VI_GetFrame %d\n",
					        stViFrame_tmp.stVFrame.u32TimeRef);
					ret = RK_MPI_VI_ReleaseChnFrame(ctx->s32DevId, ctx->s32ChnId,
					                                &stViFrame_tmp);
					if (ret != RK_SUCCESS) {
						RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", ret);
						abort();
					}
				}

				wakeup_current_mode = MULTI_FRAME_MODE;
			}

			if (g_s32AovLoopCount != 0 && wakeup_current_mode == SINGLE_FRAME_MODE) {
				if (g_s32AovLoopCount > 0)
					--g_s32AovLoopCount;
				SAMPLE_COMM_AOV_EnterSleep();
			} else if (g_s32AovLoopCount == 0) {
				quit = true;
				RK_LOGI("Exit AOV!");
				break;
			}
			loopCount++;
		}
	}

	if (wakeup_current_mode == SINGLE_FRAME_MODE)
		SAMPLE_COMM_ISP_MultiFrame(ctx->s32DevId);

	if (fp)
		fclose(fp);

	return RK_NULL;
}

/******************************************************************************
 * function : vi thread
 ******************************************************************************/
static void *vi_get_stream(void *pArgs) {
	SAMPLE_VI_CTX_S *ctx = (SAMPLE_VI_CTX_S *)(pArgs);
	RK_S32 s32Ret = RK_FAILURE;
	char name[256] = {0};
	FILE *fp = RK_NULL;
	void *pData = RK_NULL;
	RK_S32 loopCount = 0;
	RK_S32 waitTime = -1;
	RK_U32 size = 0;

	if (ctx->dstFilePath) {
		snprintf(name, sizeof(name), "/%s/vi_%d.bin", ctx->dstFilePath, ctx->s32DevId);
		fp = fopen(name, "wb");
		if (fp == RK_NULL) {
			printf("chn %d can't open %s file !\n", ctx->s32DevId, ctx->dstFilePath);
			quit = true;
			return RK_NULL;
		}
	}

	// befor enter AOV
	for (int i = 0; i < g_u32BootFrame; i++) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

			size = ctx->stViFrame.stVFrame.u32VirWidth *
			       ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
			if (fp) {
				fwrite(pData, 1, size, fp);
				fflush(fp);
			}
			RK_LOGI(
			    "SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u loop:%d seq:%d "
			    "pts:%lld ms\n",
			    ctx->s32DevId, pData, size, loopCount, ctx->stViFrame.stVFrame.u32TimeRef,
			    ctx->stViFrame.stVFrame.u64PTS / 1000);
			SAMPLE_COMM_VI_ReleaseChnFrame(ctx);
			loopCount++;
			SAMPLE_COMM_AOV_DumpPtsToTMP(ctx->stViFrame.stVFrame.u32TimeRef,
			                             ctx->stViFrame.stVFrame.u64PTS, g_u32BootFrame);
		}
	}

	// When booting, the default is multi-frame mode, so you need to pause the
	// stream and pick up the remaining frames
	SAMPLE_COMM_ISP_SingleFrame(ctx->s32DevId);
	SAMPLE_COMM_AOV_DisableNonBootCPUs();
	// drop frame
	VIDEO_FRAME_INFO_S stViFrame_tmp;
	while (RK_MPI_VI_GetChnFrame(ctx->s32DevId, ctx->s32ChnId, &stViFrame_tmp, 1000) ==
	       RK_SUCCESS) {
		RK_MPI_VI_ReleaseChnFrame(ctx->s32DevId, ctx->s32ChnId, &stViFrame_tmp);
	}
	SAMPLE_COMM_AOV_EnterSleep();

	while (!quit) {
		s32Ret = SAMPLE_COMM_VI_GetChnFrame(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {
			size = ctx->stViFrame.stVFrame.u32VirWidth *
			       ctx->stViFrame.stVFrame.u32VirHeight * 3 / 2;
			if (fp) {
				fwrite(pData, 1, size, fp);
				fflush(fp);
			}

			RK_LOGI(
			    "SAMPLE_COMM_VI_GetChnFrame DevId %d ok:data %p size:%u loop:%d seq:%d "
			    "pts:%lld ms\n",
			    ctx->s32DevId, pData, size, loopCount, ctx->stViFrame.stVFrame.u32TimeRef,
			    ctx->stViFrame.stVFrame.u64PTS / 1000);
			SAMPLE_COMM_VI_ReleaseChnFrame(ctx);

#if defined(RV1106)
			if (g_bEnableDummyFrame) {
				for (int i = 0; i != g_u32DummyFrameCnt; ++i) {
					RK_MPI_VI_DevEnableSinglelFrame(ctx->s32DevId, 1);
					s32Ret = RK_MPI_VI_GetChnFrame(ctx->u32PipeId, ctx->s32ChnId,
					                               &ctx->stViFrame, 1000);
					if (s32Ret == RK_SUCCESS) {
						RK_LOGD("get dummy frame DevId %d seq:%d pts:%lld ms\n",
						        ctx->s32DevId, ctx->stViFrame.stVFrame.u32TimeRef,
						        ctx->stViFrame.stVFrame.u64PTS / 1000);
						RK_MPI_VI_ReleaseChnFrame(ctx->u32PipeId, ctx->s32ChnId,
						                          &ctx->stViFrame);
					} else {
						RK_LOGE("RK_MPI_VI_GetChnFrame failed %#X", s32Ret);
						program_handle_error(__FUNCTION__, __LINE__);
						break;
					}
				}
			}
#endif

			if (g_s32AovLoopCount != 0) {
				if (g_s32AovLoopCount > 0)
					--g_s32AovLoopCount;
				SAMPLE_COMM_AOV_EnterSleep();
			} else {
				quit = true;
				RK_LOGI("Exit AOV!");
				break;
			}
			loopCount++;
		} else {
			RK_LOGI("get vi frame failed");
		}
	}
	SAMPLE_COMM_ISP_MultiFrame(ctx->s32DevId);
	SAMPLE_COMM_AOV_EnableNonBootCPUs();

	if (fp)
		fclose(fp);

	return RK_NULL;
}

/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
int main(int argc, char *argv[]) {
	SAMPLE_MPI_CTX_S *ctx;
	RK_U32 u32ViWidth = 1920;
	RK_U32 u32ViHeight = 1080;
	RK_CHAR *pOutPath = NULL;
	RK_CHAR *pDeviceName = NULL;
	RK_S32 s32CamId = 0;
	RK_S32 s32ChnId = VI_MAIN_CHANNEL;
	RK_S32 s32ViFrameMode = 0;
	RK_S32 s32AeMode = 0;
	RK_S32 s32SuspendTime = 1000;
	PIXEL_FORMAT_E PixelFormat = RK_FMT_YUV420SP;
	COMPRESS_MODE_E CompressMode = COMPRESS_MODE_NONE;
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	RK_S32 s32Ret;
	pthread_t vi_thread_id;
	g_bEnableDummyFrame = RK_TRUE;
	g_u32DummyFrameCnt = 3;

	if (argc < 2) {
		print_usage(argv[0]);
		return 0;
	}

	ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
	memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

	signal(SIGINT, sigterm_handler);

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
		case 'd':
			pDeviceName = optarg;
			break;
		case 'c':
			s32ChnId = atoi(optarg);
			break;
		case 'f':
			if (!strcmp(optarg, "nv12")) {
				PixelFormat = RK_FMT_YUV420SP;
			} else if (!strcmp(optarg, "nv16")) {
				PixelFormat = RK_FMT_YUV422SP;
			} else if (!strcmp(optarg, "uyvy")) {
				PixelFormat = RK_FMT_YUV422_UYVY;
			} else if (!strcmp(optarg, "yuyv")) {
				PixelFormat = RK_FMT_YUV422_YUYV;
			}
#if defined(RV1106)
			else if (!strcmp(optarg, "rgb565")) {
				PixelFormat = RK_FMT_RGB565;
				s32ChnId = 1;
			} else if (!strcmp(optarg, "xbgr8888")) {
				PixelFormat = RK_FMT_XBGR8888;
				s32ChnId = 1;
			}
#endif
			else {
				RK_LOGE("this pixel_format is not supported in the sample");
				print_usage(argv[0]);
				goto __FAILED2;
			}
			break;
		case 'w':
			u32ViWidth = atoi(optarg);
			break;
		case 'h':
			u32ViHeight = atoi(optarg);
			break;
		case 'I':
			s32CamId = atoi(optarg);
			break;
		case 'o':
			pOutPath = optarg;
			break;
		case 'h' + 'm':
			if (atoi(optarg) == 0) {
				hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
			} else if (atoi(optarg) == 1) {
				hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
			} else if (atoi(optarg) == 2) {
				hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR3;
			} else {
				RK_LOGE("input hdr_mode is not support(error)");
				print_usage(argv[0]);
				goto __FAILED2;
			}
			break;
		case 'v' + 'f' + 'm':
			s32ViFrameMode = atoi(optarg);
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
		case 'e' + 'd' + 'f':
			g_bEnableDummyFrame = atoi(optarg) ? RK_TRUE : RK_FALSE;
			break;
		case 'd' + 'f' + 'c':
			g_u32DummyFrameCnt = atoi(optarg);
			break;
		case '?':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	printf("#CameraIdx: %d\n", s32CamId);
	printf("#pDeviceName: %s\n", pDeviceName);
	printf("#Output Path: %s\n", pOutPath);

	SAMPLE_COMM_AOV_Init();
#ifdef RKAIQ

	printf("#Rkaiq XML DirPath: %s\n", iq_file_dir);
	printf("#bMultictx: %d\n\n", bMultictx);
	SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, iq_file_dir);
	SAMPLE_COMM_ISP_Run(s32CamId);
#endif

	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		goto __FAILED;
	}

	// Init VI
	ctx->vi.u32Width = u32ViWidth;
	ctx->vi.u32Height = u32ViHeight;
	ctx->vi.s32DevId = s32CamId;
	ctx->vi.u32PipeId = ctx->vi.s32DevId;
	ctx->vi.s32ChnId = s32ChnId;
	ctx->vi.stChnAttr.stIspOpt.u32BufCount = 2;
	ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi.stChnAttr.u32Depth = 1;
	ctx->vi.stChnAttr.enPixelFormat = PixelFormat;
	ctx->vi.stChnAttr.enCompressMode = CompressMode;
	ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
	ctx->vi.dstFilePath = pOutPath;
	ctx->vi.bIfQuickStart = RK_TRUE;
	if (pDeviceName) {
		strcpy(ctx->vi.stChnAttr.stIspOpt.aEntityName, pDeviceName);
	}
	SAMPLE_COMM_VI_CreateChn(&ctx->vi);

	if (!ctx->vi.bIfQuickStart) {
		RK_MPI_VI_StartPipe(ctx->vi.u32PipeId);
	}
	if (s32ViFrameMode == 0)
		pthread_create(&vi_thread_id, 0, vi_get_stream, (void *)(&ctx->vi));
	else
		pthread_create(&vi_thread_id, 0, vi_get_multi_stream, (void *)(&ctx->vi));

	SAMPLE_COMM_AOV_SetSuspendTime(s32SuspendTime);

	printf("%s initial finish\n", __func__);

	while (!quit) {
		sleep(1);
	}

	printf("%s exit!\n", __func__);

	pthread_join(vi_thread_id, NULL);

	// Destroy VI
	SAMPLE_COMM_VI_DestroyChn(&ctx->vi);

__FAILED:
	RK_MPI_SYS_Exit();
#ifdef RKAIQ
	SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
	SAMPLE_COMM_AOV_Deinit();
__FAILED2:
	if (ctx) {
		free(ctx);
		ctx = RK_NULL;
	}

	return g_exit_result;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
