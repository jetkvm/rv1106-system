/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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
#include <sys/poll.h>
#include <unistd.h>

#include "sample_comm.h"

RK_S32 SAMPLE_COMM_AI_SetAttr(SAMPLE_AI_CTX_S *ctx) {
	AIO_ATTR_S *pstAiAttr = &ctx->stAiAttr;
	RK_S32 deviceSampleRate = 16000, outSampleRate = 16000;
	RK_S32 deviceChannels = 2, outChannels = 1;

	memset(pstAiAttr, 0, sizeof(AIO_ATTR_S));
	sprintf((char *)pstAiAttr->u8CardName, "%s", "hw:0,0");
	RK_U32 u32FrameCnt = 0;

	if (outSampleRate == 32000 || outSampleRate == 44100 || outSampleRate == 48000)
		u32FrameCnt = 1152 * 2 * outChannels;
	else
		u32FrameCnt = 576 * 2 * outChannels;

	pstAiAttr->soundCard.channels = deviceChannels;
	pstAiAttr->soundCard.sampleRate = deviceSampleRate;
	pstAiAttr->soundCard.bitWidth = AUDIO_BIT_WIDTH_16;
	pstAiAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
	pstAiAttr->enSamplerate = outSampleRate;
	if (outChannels == 1)
		pstAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
	else if (outChannels == 2)
		pstAiAttr->enSoundmode = AUDIO_SOUND_MODE_STEREO;
	else {
		RK_LOGE("unsupport = %d", outChannels);
		return RK_FAILURE;
	}

	pstAiAttr->u32PtNumPerFrm = u32FrameCnt;
	pstAiAttr->u32FrmNum = 4;
	pstAiAttr->u32EXFlag = 0;
	pstAiAttr->u32ChnCnt = 2;

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AI_CreateChn(SAMPLE_AI_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;
	AI_CHN_PARAM_S pstParams;

	s32Ret = RK_MPI_AI_SetPubAttr(ctx->s32DevId, &ctx->stAiAttr);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_SetPubAttr failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	s32Ret = RK_MPI_AI_Enable(ctx->s32DevId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_Enable failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	memset(&pstParams, 0, sizeof(AI_CHN_PARAM_S));
	pstParams.enLoopbackMode = AUDIO_LOOPBACK_NONE;
	pstParams.s32UsrFrmDepth = 1;
	s32Ret = RK_MPI_AI_SetChnParam(ctx->s32DevId, ctx->s32ChnId, &pstParams);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_SetChnParam failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	if (ctx->stAiAttr.enSoundmode == AUDIO_SOUND_MODE_MONO)
		RK_MPI_AI_SetTrackMode(ctx->s32DevId, AUDIO_TRACK_FRONT_LEFT);
	else
		RK_MPI_AI_SetTrackMode(ctx->s32DevId, AUDIO_TRACK_NORMAL);

	RK_MPI_AI_SetVolume(ctx->s32DevId, 100);
	s32Ret = RK_MPI_AI_EnableChn(ctx->s32DevId, ctx->s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_EnableChn failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	s32Ret = RK_MPI_AI_EnableReSmp(ctx->s32DevId, ctx->s32ChnId,
	                               (AUDIO_SAMPLE_RATE_E)ctx->stAiAttr.enSamplerate);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_EnableReSmp failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AI_DestroyChn(SAMPLE_AI_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_AI_DisableReSmp(ctx->s32DevId, ctx->s32ChnId);
	s32Ret |= RK_MPI_AI_DisableChn(ctx->s32DevId, ctx->s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_DisableChn failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	s32Ret = RK_MPI_AI_Disable(ctx->s32DevId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_Disable failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AI_GetFrame(SAMPLE_AI_CTX_S *ctx, void **pdata) {
	RK_S32 s32Ret = RK_FAILURE;
	RK_S32 s32MilliSec = -1;

	s32Ret = RK_MPI_AI_GetFrame(ctx->s32DevId, ctx->s32ChnId, &ctx->stFrame, RK_NULL,
	                            s32MilliSec);
	if (s32Ret == RK_SUCCESS) {
		*pdata = RK_MPI_MB_Handle2VirAddr(ctx->stFrame.pMbBlk);
	} else {
		RK_LOGE("RK_MPI_AI_GetFrame failed with %#x!\n", s32Ret);
	}

	return 0;
}

RK_S32 SAMPLE_COMM_AI_ReleaseFrame(SAMPLE_AI_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_AI_ReleaseFrame(ctx->s32DevId, ctx->s32ChnId, &ctx->stFrame, RK_NULL);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AI_ReleaseFrame failed with %#x!\n", s32Ret);
		return RK_FAILURE;
	}

	return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
