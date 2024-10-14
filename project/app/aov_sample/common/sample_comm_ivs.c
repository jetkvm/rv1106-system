/*
 * Copyright 2022 Rockchip Electronics Co. LTD
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

RK_S32 SAMPLE_COMM_IVS_Create(SAMPLE_IVS_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_IVS_CreateChn(ctx->s32ChnId, &ctx->stIvsAttr);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_CreateChn failure:%X", s32Ret);
		return s32Ret;
	}
#if defined(RV1106)
	IVS_MD_ATTR_S stMdAttr;
	memset(&stMdAttr, 0, sizeof(stMdAttr));
	s32Ret = RK_MPI_IVS_GetMdAttr(0, &stMdAttr);
	if (s32Ret) {
		RK_LOGE("ivs get mdattr failed:%x", s32Ret);
		return -1;
	}
	stMdAttr.s32ThreshSad = 64;
	stMdAttr.s32ThreshMove = 2;
	stMdAttr.s32SwitchSad = 2;
	stMdAttr.bFlycatkinFlt = RK_TRUE;
	stMdAttr.s32ThresDustMove = 3;
	stMdAttr.s32ThresDustBlk = 3;
	stMdAttr.s32ThresDustChng = 50;

	s32Ret = RK_MPI_IVS_SetMdAttr(0, &stMdAttr);
	if (s32Ret) {
		RK_LOGE("ivs set mdattr failed:%x", s32Ret);
		return -1;
	}
#endif
	return 0;
}

RK_S32 SAMPLE_COMM_IVS_bMove(SAMPLE_IVS_CTX_S *ctx, VIDEO_FRAME_INFO_S *stVFrame) {
	RK_S32 s32Ret = RK_FAILURE;
	RK_BOOL bMove = RK_FALSE;
	s32Ret = RK_MPI_IVS_SendFrame(ctx->s32ChnId, stVFrame, -1);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_SendFrame chnd	%d failed %#X", ctx->s32ChnId, s32Ret);
		return s32Ret;
	}

	IVS_RESULT_INFO_S stIVSResults;
	s32Ret = RK_MPI_IVS_GetResultsRaw(ctx->s32ChnId, &stIVSResults, -1);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_GetResults chnd  %d failed %#X", ctx->s32ChnId, s32Ret);
		return s32Ret;
	}

	if (stIVSResults.s32ResultNum == 1) {
		if (1000 * stIVSResults.pstResults->stMdInfo.u32Square /
		        ctx->stIvsAttr.u32PicWidth / ctx->stIvsAttr.u32PicHeight >
		    10) {
			bMove = RK_TRUE;
		}
	}

	s32Ret = RK_MPI_IVS_ReleaseResults(ctx->s32ChnId, &stIVSResults);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_ReleaseResults chnd  %d failed %#X", ctx->s32ChnId, s32Ret);
		return s32Ret;
	}
	return bMove;
}

RK_S32 SAMPLE_COMM_IVS_Destroy(RK_S32 s32IvsChnid) {
	RK_S32 s32Ret = RK_FAILURE;
	s32Ret = RK_MPI_IVS_DestroyChn(s32IvsChnid);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_IVS_DestroyChn failure:%X", s32Ret);
		return s32Ret;
	}

	return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
