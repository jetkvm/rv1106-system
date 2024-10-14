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
#include "mp3_enc_types.h"
#include "mp3_enc_table1.h"
#include "mp3_enc_table2.h"
#include "aenc_mp3_register.h"

static RK_S32 extCodecHandle = -1;
static RK_U32 mp3InitCnt = 0;

typedef struct _RK_AENC_MP3_CTX_S {
	mp3_enc *pMp3Enc;
	RK_S32 frameLength;
} RK_AENC_MP3_CTX_S;

RK_S32 RKAduioMp3EncoderOpen(RK_VOID *pEncoderAttr, RK_VOID **ppEncoder) {
	int bitrate = 0;
	if (pEncoderAttr == NULL) {
		RK_LOGE("pEncoderAttr is NULL");
		return RK_FAILURE;
	}

	AENC_ATTR_CODEC_S *attr = (AENC_ATTR_CODEC_S *)pEncoderAttr;
	if (attr->enType != RK_AUDIO_ID_MP3) {
		RK_LOGE("Invalid enType[%d]", attr->enType);
		return RK_FAILURE;
	}

	RK_AENC_MP3_CTX_S *ctx = (RK_AENC_MP3_CTX_S *)malloc(sizeof(RK_AENC_MP3_CTX_S));
	if (!ctx) {
		RK_LOGE("malloc aenc mp3 ctx failed");
		return RK_FAILURE;
	}

	memset(ctx, 0, sizeof(RK_AENC_MP3_CTX_S));
	if (attr->u32Resv[0] > 1152) {
		RK_LOGE("error: MP3 FrameLength is too large, FrameLength = %d",
		        attr->u32Resv[0]);
		goto __FAILED;
	}

	ctx->frameLength = attr->u32Resv[0];
	bitrate = attr->u32Resv[1] / 1000;
	RK_LOGD("MP3Encode: sample_rate = %d, channel = %d, bitrate = %d.",
	        attr->u32SampleRate, attr->u32Channels, bitrate);
	ctx->pMp3Enc = Mp3EncodeVariableInit(attr->u32SampleRate, attr->u32Channels, bitrate);
	if (ctx->pMp3Enc->frame_size <= 0) {
		RK_LOGE("MP3Encode init failed! r:%d c:%d\n", attr->u32SampleRate,
		        attr->u32Channels);
		goto __FAILED;
	}

	RK_LOGD("MP3Encode FrameSize = %d", ctx->pMp3Enc->frame_size);
	*ppEncoder = (RK_VOID *)ctx;

	return RK_SUCCESS;

__FAILED:
	RKAduioMp3EncoderClose((RK_VOID *)ctx);
	*ppEncoder = RK_NULL;
	return RK_FAILURE;
}

RK_S32 RKAduioMp3EncoderClose(RK_VOID *pEncoder) {
	RK_AENC_MP3_CTX_S *ctx = (RK_AENC_MP3_CTX_S *)pEncoder;
	if (ctx == NULL)
		return RK_SUCCESS;

	Mp3EncodeDeinit(ctx->pMp3Enc);
	free(ctx);
	ctx = NULL;
	return RK_SUCCESS;
}

RK_S32 RKAduioMp3EncoderEncode(RK_VOID *pEncoder, RK_VOID *pEncParam) {
	RK_AENC_MP3_CTX_S *ctx = (RK_AENC_MP3_CTX_S *)pEncoder;
	AUDIO_ADENC_PARAM_S *pParam = (AUDIO_ADENC_PARAM_S *)pEncParam;

	if (ctx == NULL || pParam == NULL) {
		RK_LOGE("Invalid ctx or pParam");
		return AENC_ENCODER_ERROR;
	}

	RK_U32 u32EncSize = 0;
	RK_U8 *inData = pParam->pu8InBuf;
	RK_U64 inPts = pParam->u64InTimeStamp;
	RK_U32 inbufSize = 0;
	RK_U32 copySize = 0;

	// if input buffer is NULL, this means eos(end of stream)
	if (inData == NULL)
		pParam->u64OutTimeStamp = inPts;

	if (pParam->u32InLen == 0)
		return AENC_ENCODER_EOS;

	inbufSize = 2 * ctx->pMp3Enc->frame_size;
	copySize = (pParam->u32InLen > inbufSize) ? inbufSize : pParam->u32InLen;
	memcpy(ctx->pMp3Enc->config.in_buf, inData, copySize);
	pParam->u32InLen = pParam->u32InLen - copySize;
	u32EncSize =
	    L3_compress(ctx->pMp3Enc, 0, (unsigned char **)(&ctx->pMp3Enc->config.out_buf));
	u32EncSize = (u32EncSize > pParam->u32OutLen) ? pParam->u32OutLen : u32EncSize;
	memcpy(pParam->pu8OutBuf, ctx->pMp3Enc->config.out_buf, u32EncSize);
	pParam->u64OutTimeStamp = inPts;
	pParam->u32OutLen = u32EncSize;
	return AENC_ENCODER_OK;
}

RK_S32 RegisterAencMp3(void) {
	if (!mp3InitCnt) {
		RK_S32 ret;
		AENC_ENCODER_S aencCtx;
		memset(&aencCtx, 0, sizeof(AENC_ENCODER_S));

		extCodecHandle = -1;
		aencCtx.enType = RK_AUDIO_ID_MP3;
		snprintf((RK_CHAR *)(aencCtx.aszName), sizeof(aencCtx.aszName), "rkaudio");
		aencCtx.u32MaxFrmLen = 2048;
		aencCtx.pfnOpenEncoder = RKAduioMp3EncoderOpen;
		aencCtx.pfnEncodeFrm = RKAduioMp3EncoderEncode;
		aencCtx.pfnCloseEncoder = RKAduioMp3EncoderClose;

		RK_LOGD("register external aenc(%s)", aencCtx.aszName);
		ret = RK_MPI_AENC_RegisterEncoder(&extCodecHandle, &aencCtx);
		if (ret != RK_SUCCESS) {
			RK_LOGE("aenc %s register decoder fail %x", aencCtx.aszName, ret);
			return RK_FAILURE;
		}
	}

	mp3InitCnt++;
	return RK_SUCCESS;
}

RK_S32 UnRegisterAencMp3(void) {
	if (extCodecHandle == -1) {
		return RK_SUCCESS;
	}

	if (0 == mp3InitCnt) {
		return 0;
	} else if (1 == mp3InitCnt) {
		RK_LOGD("unregister external aenc");
		RK_S32 ret = RK_MPI_AENC_UnRegisterEncoder(extCodecHandle);
		if (ret != RK_SUCCESS) {
			RK_LOGE("aenc unregister decoder fail %x", ret);
			return RK_FAILURE;
		}

		extCodecHandle = -1;
	}
	mp3InitCnt--;
	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AENC_SetAttr(SAMPLE_AENC_CTX_S *ctx) {
	AENC_CHN_ATTR_S *pstChnAttr = &ctx->stChnAttr;
	memset(pstChnAttr, 0, sizeof(AENC_CHN_ATTR_S));

	pstChnAttr->stCodecAttr.enType = RK_AUDIO_ID_MP3;
	pstChnAttr->stCodecAttr.u32Channels = 1;
	pstChnAttr->stCodecAttr.u32SampleRate = 16000;
	pstChnAttr->stCodecAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
	pstChnAttr->stCodecAttr.pstResv = RK_NULL;
	if (pstChnAttr->stCodecAttr.enType == RK_AUDIO_ID_MP3) {
		pstChnAttr->stCodecAttr.u32Resv[0] = 1152;
		pstChnAttr->stCodecAttr.u32Resv[1] = 160000;
	}

	pstChnAttr->enType = RK_AUDIO_ID_MP3;
	pstChnAttr->u32BufCount = 4;

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AENC_CreateChn(SAMPLE_AENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;
	s32Ret = RegisterAencMp3();
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("mp3 register failed %x", s32Ret);
		return s32Ret;
	}

	s32Ret = RK_MPI_AENC_CreateChn(ctx->s32ChnId, &ctx->stChnAttr);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AENC_CreateChn failed %x", s32Ret);
		return s32Ret;
	}

	if (ctx->getStreamCbFunc) {
		pthread_create(&ctx->getStreamThread, 0, ctx->getStreamCbFunc, (void *)(ctx));
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_AENC_SendLastFrame(SAMPLE_AENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;
	AUDIO_FRAME_S stAudioFrm;
	stAudioFrm.u32Len = 0;
	stAudioFrm.u64TimeStamp = 0;
	stAudioFrm.u32Seq = 0;
	stAudioFrm.bBypassMbBlk = RK_FALSE;

	MB_EXT_CONFIG_S extConfig = {0};
	extConfig.pFreeCB = NULL;
	extConfig.pOpaque = NULL;
	extConfig.pu8VirAddr = NULL;
	extConfig.u64Size = 0;
	RK_MPI_SYS_CreateMB(&(stAudioFrm.pMbBlk), &extConfig);

	s32Ret = RK_MPI_AENC_SendFrame(ctx->s32ChnId, &stAudioFrm, RK_NULL, -1);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("fail to send aenc stream.");
	}

	RK_MPI_MB_ReleaseMB(stAudioFrm.pMbBlk);
	return s32Ret;
}

RK_S32 SAMPLE_COMM_AENC_GetStream(SAMPLE_AENC_CTX_S *ctx, void **pdata) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_AENC_GetStream(ctx->s32ChnId, &ctx->stFrame, -1);
	if (s32Ret == RK_SUCCESS) {
		*pdata = RK_MPI_MB_Handle2VirAddr(ctx->stFrame.pMbBlk);
	} else {
		RK_LOGE("RK_MPI_AENC_GetStream fail %x", s32Ret);
	}

	return s32Ret;
}

RK_S32 SAMPLE_COMM_AENC_ReleaseStream(SAMPLE_AENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_AENC_ReleaseStream(ctx->s32ChnId, &ctx->stFrame);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_AENC_ReleaseStream fail %x", s32Ret);
	}

	return s32Ret;
}

RK_S32 SAMPLE_COMM_AENC_DestroyChn(SAMPLE_AENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_AENC_DestroyChn(ctx->s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_ADEC_DestroyChn fail %x", s32Ret);
	}
	UnRegisterAencMp3();

	return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
