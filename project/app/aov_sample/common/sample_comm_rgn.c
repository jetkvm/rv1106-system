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
#include <sys/poll.h>
#include <unistd.h>
#include <time.h>
#include "utils.h"
#include "font_factory.h"
#include "sample_comm.h"

#define CLUT_TABLE_8BPP_NUM 256
// ARGB color, each color channel takes 16 bits
const uint32_t clut_table_8bpp[CLUT_TABLE_8BPP_NUM] = {
    0x00000000, 0xff000000, 0xff800000, 0xff008000, 0xff808000, 0xff000080, 0xff800080,
    0xff008080, 0xffc0c0c0, 0xff808080, 0xffff0000, 0xff00ff00, 0xffffff00, 0xff0000ff,
    0xffff00ff, 0xff00ffff, 0xffffffff, 0xff000000, 0xff00005f, 0xff000087, 0xff0000af,
    0xff0000d7, 0xff0000ff, 0xff005f00, 0xff005f5f, 0xff005f87, 0xff005faf, 0xff005fd7,
    0xff005fff, 0xff008700, 0xff00875f, 0xff0087af, 0xff0087d7, 0xff0087ff, 0xff00af00,
    0xff00af5f, 0xff00af87, 0xff00afaf, 0xff00afd7, 0xff00afff, 0xff00d700, 0xff00d75f,
    0xff00d787, 0xff00d7af, 0xff00d7d7, 0xff00d7ff, 0xff00ff00, 0xff00ff5f, 0xff00ff87,
    0xff00ffaf, 0xff00ffd7, 0xff00ffff, 0xff5f0000, 0xff5f005f, 0xff5f0087, 0xff5f00af,
    0xff5f00d7, 0xff5f00ff, 0xff5f5f00, 0xff5f5f5f, 0xff5f5f87, 0xff5f5faf, 0xff5f5fd7,
    0xff5f5fff, 0xff5f8700, 0xff5f875f, 0xff5f8787, 0xff5f87af, 0xff5f87d7, 0xff5f87ff,
    0xff5faf00, 0xff5faf5f, 0xff5faf87, 0xff5fafaf, 0xff5fafd7, 0xff5fafff, 0xff5fd700,
    0xff5fd75f, 0xff5fd787, 0xff5fd7af, 0xff5fd7d7, 0xff5fd7ff, 0xff5fff00, 0xff5fff5f,
    0xff5fff87, 0xff5fffaf, 0xff5fffd7, 0xff5fffff, 0xff870000, 0xff87005f, 0xff870087,
    0xff8700af, 0xff8700d7, 0xff8700ff, 0xff875f00, 0xff875f5f, 0xff875f87, 0xff875faf,
    0xff875fd7, 0xff875fff, 0xff878700, 0xff87875f, 0xff878787, 0xff8787af, 0xff8787d7,
    0xff8787ff, 0xff87af00, 0xff87af5f, 0xff87af87, 0xff87afaf, 0xff87afd7, 0xff87afff,
    0xff87d700, 0xff87d75f, 0xff87d787, 0xff87d7af, 0xff87d7d7, 0xff87d7ff, 0xff87ff00,
    0xff87ff5f, 0xff87ff87, 0xff87ffaf, 0xff87ffd7, 0xff87ffff, 0xffaf0000, 0xffaf005f,
    0xffaf0087, 0xffaf00af, 0xffaf00d7, 0xffaf00ff, 0xffaf5f00, 0xffaf5f5f, 0xffaf5f87,
    0xffaf5faf, 0xffaf5fd7, 0xffaf5fff, 0xffaf8700, 0xffaf875f, 0xffaf8787, 0xffaf87af,
    0xffaf87d7, 0xffaf87ff, 0xffafaf00, 0xffafaf5f, 0xffafaf87, 0xffafafaf, 0xffafafd7,
    0xffafafff, 0xffafd700, 0xffafd75f, 0xffafd787, 0xffafd7af, 0xffafd7d7, 0xffafd7ff,
    0xffafff00, 0xffafff5f, 0xffafff87, 0xffafffaf, 0xffafffd7, 0xffafffff, 0xffd70000,
    0xffd7005f, 0xffd70087, 0xffd700af, 0xffd700d7, 0xffd700ff, 0xffd75f00, 0xffd75f5f,
    0xffd75f87, 0xffd75faf, 0xffd75fd7, 0xffd75fff, 0xffd78700, 0xffd7875f, 0xffd78787,
    0xffd787af, 0xffd787d7, 0xffd787ff, 0xffd7af00, 0xffd7af5f, 0xffd7af87, 0xffd7afaf,
    0xffd7afd7, 0xffd7afff, 0xffd7d700, 0xffd7d75f, 0xffd7d787, 0xffd7d7af, 0xffd7d7d7,
    0xffd7d7ff, 0xffd7ff00, 0xffd7ff5f, 0xffd7ff87, 0xffd7ffaf, 0xffd7ffd7, 0xffd7ffff,
    0xffff0000, 0xffff005f, 0xffff0087, 0xffff00af, 0xffff00d7, 0xffff00ff, 0xffff5f00,
    0xffff5f5f, 0xffff5f87, 0xffff5faf, 0xffff5fd7, 0xffff5fff, 0xffff8700, 0xffff875f,
    0xffff8787, 0xffff87af, 0xffff87d7, 0xffff87ff, 0xffffaf00, 0xffffaf5f, 0xffffaf87,
    0xffffafaf, 0xffffafd7, 0xffffafff, 0xffffd700, 0xffffd75f, 0xffffd787, 0xffffd7af,
    0xffffd7d7, 0xffffd7ff, 0xffffff00, 0xffffff5f, 0xffffff87, 0xffffffaf, 0xffffffd7,
    0xffffffff, 0xff080808, 0xff121212, 0xff1c1c1c, 0xff262626, 0xff303030, 0xff3a3a3a,
    0xff444444, 0xff4e4e4e, 0xff585858, 0xff626262, 0xff6c6c6c, 0xff767676, 0xff808080,
    0xff8a8a8a, 0xff949494, 0xff9e9e9e, 0xffa8a8a8, 0xffb2b2b2, 0xffbcbcbc, 0xffc6c6c6,
    0xffd0d0d0, 0xffdadada, 0xffe4e4e4, 0xffeeeeee,
};

static RK_S32 test_rgn_load_bmp(SAMPLE_RGN_CTX_S *ctx) {
	OSD_SURFACE_S Surface;
	OSD_BITMAPFILEHEADER bmpFileHeader;
	OSD_BITMAPINFO bmpInfo;
	RK_BOOL bCPU = RK_FALSE;

	if (get_bmp_info(ctx->srcFileBmpName, &bmpFileHeader, &bmpInfo) < 0) {
		RK_LOGE("GetBmpInfo err, generate from cpu!\n");
		bCPU = RK_TRUE;
		// return RK_FAILURE;
	}

	switch (ctx->u32BmpFormat) {
	case RK_FMT_ARGB8888:
		Surface.enColorFmt = OSD_COLOR_FMT_ARGB8888;
		break;
	case RK_FMT_BGRA8888:
		Surface.enColorFmt = OSD_COLOR_FMT_BGRA8888;
		break;
	case RK_FMT_ARGB1555:
		Surface.enColorFmt = OSD_COLOR_FMT_ARGB1555;
		break;
	case RK_FMT_BGRA5551:
		Surface.enColorFmt = OSD_COLOR_FMT_BGRA5551;
		break;
	default:
		return RK_FAILURE;
	}

	if (bCPU) {
		ctx->stBitmap.pData =
		    malloc(4 * (ctx->stRegion.u32Width) * (ctx->stRegion.u32Height));

		if (RK_NULL == ctx->stBitmap.pData) {
			RK_LOGE("malloc osd memroy err!");
			return RK_FAILURE;
		}
		SAMPLE_COMM_FillImage(ctx->stBitmap.pData, ctx->stRegion.u32Width,
		                      ctx->stRegion.u32Height, ctx->stRegion.u32Width,
		                      ctx->stRegion.u32Height, ctx->u32BmpFormat, 0);
		ctx->stBitmap.u32Width = ctx->stRegion.u32Width;
		ctx->stBitmap.u32Height = ctx->stRegion.u32Height;
		ctx->stBitmap.enPixelFormat = ctx->u32BmpFormat;
		return RK_SUCCESS;
	}

	ctx->stBitmap.pData =
	    malloc(4 * (bmpInfo.bmiHeader.biWidth) * (bmpInfo.bmiHeader.biHeight));

	if (RK_NULL == ctx->stBitmap.pData) {
		RK_LOGE("malloc osd memroy err!");
		return RK_FAILURE;
	}

	create_surface_by_bitmap(ctx->srcFileBmpName, &Surface,
	                         (RK_U8 *)(ctx->stBitmap.pData));

	ctx->stBitmap.u32Width = Surface.u16Width;
	ctx->stBitmap.u32Height = Surface.u16Height;
	ctx->stBitmap.enPixelFormat = (PIXEL_FORMAT_E)ctx->u32BmpFormat;

	return RK_SUCCESS;
}

#define UPALIGNTO(value, align) ((value + align - 1) & (~(align - 1)))
#define UPALIGNTO2(value) UPALIGNTO(value, 2)
#define UPALIGNTO4(value) UPALIGNTO(value, 4)
#define UPALIGNTO16(value) UPALIGNTO(value, 16)
#define DOWNALIGNTO16(value) (UPALIGNTO(value, 16) - 16)
#define MULTI_UPALIGNTO16(grad, value) UPALIGNTO16((int)(grad * value))

RK_S32 SAMPLE_COMM_RGN_CreateChn(SAMPLE_RGN_CTX_S *ctx) {
	RK_S64 s64TimeStart;
	RK_S64 s64TimeEnd;
	RK_S32 s32Ret = RK_SUCCESS;

	switch (ctx->stRgnAttr.enType) {
	case OVERLAY_RGN: {
		ctx->stRgnAttr.enType = OVERLAY_RGN;
		ctx->stRgnAttr.unAttr.stOverlay.enPixelFmt = (PIXEL_FORMAT_E)ctx->u32BmpFormat;
		ctx->stRgnAttr.unAttr.stOverlay.stSize.u32Width = ctx->stRegion.u32Width;
		ctx->stRgnAttr.unAttr.stOverlay.stSize.u32Height = ctx->stRegion.u32Height;

		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = OVERLAY_RGN;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = ctx->u32BgAlpha;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = ctx->u32FgAlpha;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = ctx->u32Layer;

		if (!ctx->bDrawBmpManual) {
			s32Ret = test_rgn_load_bmp(ctx);
			if (s32Ret != RK_SUCCESS) {
				RK_LOGE("test_rgn_load_bmp failure:%#X", s32Ret);
				return s32Ret;
			}
		}
	} break;
	case COVER_RGN: {
		ctx->stRgnAttr.enType = COVER_RGN;
		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = COVER_RGN;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Width = ctx->stRegion.u32Width;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Height = ctx->stRegion.u32Height;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.u32Color = ctx->u32Color;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.u32Layer = ctx->u32Layer;
	} break;
	case MOSAIC_RGN: {
		ctx->stRgnAttr.enType = MOSAIC_RGN;
		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = MOSAIC_RGN;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = ctx->stRegion.u32Width;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.u32Height =
		    ctx->stRegion.u32Height;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_64;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.u32Layer = ctx->u32Layer;
	} break;
	default:
		RK_LOGE("unsupport type %d.", ctx->stRgnAttr.enType);
		return RK_FAILURE;
	}

	s32Ret = RK_MPI_RGN_Create(ctx->rgnHandle, &ctx->stRgnAttr);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_Create (%d) failed with %#x!", ctx->rgnHandle, s32Ret);
		return RK_FAILURE;
	}
#if defined(RV1126)
	if (ctx->u32BmpFormat == RK_FMT_8BPP) {
		ctx->stRgnAttr.unAttr.stOverlay.u32ClutNum = CLUT_TABLE_8BPP_NUM;
		memcpy(ctx->stRgnAttr.unAttr.stOverlay.u32Clut, clut_table_8bpp,
		       sizeof(ctx->stRgnAttr.unAttr.stOverlay.u32Clut[0]) * CLUT_TABLE_8BPP_NUM);
		s32Ret = RK_MPI_RGN_SetAttr(ctx->rgnHandle, &ctx->stRgnAttr);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_RGN_SetAttr (%d) failed with %#x!", ctx->rgnHandle, s32Ret);
			return RK_FAILURE;
		}
	}
#endif

	s32Ret = RK_MPI_RGN_AttachToChn(ctx->rgnHandle, &ctx->stMppChn, &ctx->stRgnChnAttr);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_AttachToChn (%d) failed with %#x!", ctx->rgnHandle, s32Ret);
		return RK_FAILURE;
	}

	if (ctx->stRgnAttr.enType == OVERLAY_RGN && !ctx->bDrawBmpManual) {
		// s64TimeStart = mpi_test_utils_get_now_us();

		s32Ret = RK_MPI_RGN_SetBitMap(ctx->rgnHandle, &ctx->stBitmap);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_RGN_SetBitMap failed with %#x!", s32Ret);
			if (RK_NULL != ctx->stBitmap.pData) {
				free(ctx->stBitmap.pData);
				ctx->stBitmap.pData = NULL;
			}
			return RK_FAILURE;
		}

		if (RK_NULL != ctx->stBitmap.pData) {
			free(ctx->stBitmap.pData);
			ctx->stBitmap.pData = NULL;
		}

		// s64TimeEnd = mpi_test_utils_get_now_us();
		// RK_LOGI("Handle:%d, space time %lld us, load bmp success!",
		// ctx->rgnHandle, s64TimeEnd -
		// s64TimeStart);
	}

	s32Ret =
	    RK_MPI_RGN_GetDisplayAttr(ctx->rgnHandle, &ctx->stMppChn, &ctx->stRgnChnAttr);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_GetDisplayAttr (%d)) failed with %#x!", ctx->rgnHandle,
		        s32Ret);
		return RK_FAILURE;
	}

	switch (ctx->stRgnAttr.enType) {
	case OVERLAY_RGN: {
		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = OVERLAY_RGN;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = ctx->u32BgAlpha;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = ctx->u32FgAlpha;
		ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = ctx->u32Layer;
		RK_LOGE("resize the overlay region %d to <%d, %d> BgAlpha %d "
		        "FgAlpha%d, color<0x%x>",
		        ctx->rgnHandle, ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,
		        ctx->stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y,
		        ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha,
		        ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha,
		        ctx->stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer);
	} break;
	case COVER_RGN: {
		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = COVER_RGN;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Width = ctx->stRegion.u32Width;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Height = ctx->stRegion.u32Height;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.u32Color = ctx->u32Color;
		ctx->stRgnChnAttr.unChnAttr.stCoverChn.u32Layer = ctx->u32Layer;
		RK_LOGE("resize the cover region %d to <%d, %d, %d, %d>, color<0x%x>",
		        ctx->rgnHandle, ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32X,
		        ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32Y,
		        ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Width,
		        ctx->stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Height,
		        ctx->stRgnChnAttr.unChnAttr.stCoverChn.u32Color);
	} break;
	case MOSAIC_RGN: {
		ctx->stRgnChnAttr.bShow = RK_TRUE;
		ctx->stRgnChnAttr.enType = MOSAIC_RGN;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.s32X = ctx->stRegion.s32X;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = ctx->stRegion.s32Y;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = ctx->stRegion.u32Width;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.stRect.u32Height =
		    ctx->stRegion.u32Height;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_64;
		ctx->stRgnChnAttr.unChnAttr.stMosaicChn.u32Layer = ctx->u32Layer;
	} break;
	default:
		RK_LOGE("unsupport type %d.", ctx->stRgnAttr.enType);
		return RK_FAILURE;
	}

	s32Ret =
	    RK_MPI_RGN_SetDisplayAttr(ctx->rgnHandle, &ctx->stMppChn, &ctx->stRgnChnAttr);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_SetDisplayAttr (%d)) failed with %#x!", ctx->rgnHandle,
		        s32Ret);
		return RK_FAILURE;
	}

	if (ctx->st_osd_data.enable) {
		int normalized_screen_width = WEB_VIEW_RECT_W;
		int normalized_screen_height = WEB_VIEW_RECT_H;
		int video_width = ctx->stRegion.u32Width;
		int video_height = ctx->stRegion.u32Height;
		double x_rate = (double)video_width / (double)normalized_screen_width;
		double y_rate = (double)video_height / (double)normalized_screen_height;
		// init
		ctx->st_osd_data.enable = 1;
		ctx->st_osd_data.origin_x = UPALIGNTO16((int)(16 * x_rate));
		ctx->st_osd_data.origin_y = UPALIGNTO16((int)(16 * y_rate));
		ctx->st_osd_data.text.font_size = 32;
		ctx->st_osd_data.text.font_color = 0xfff799;
		ctx->st_osd_data.text.color_inverse = 1;
		ctx->st_osd_data.text.font_path = "/oem/usr/share/simsun_en.ttf";
		s32Ret =
		    create_font(ctx->st_osd_data.text.font_path, ctx->st_osd_data.text.font_size);
		if (s32Ret != 0) {
			RK_LOGE("Failed create font!\n");
			return RK_FAILURE;
		}
	}
	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_RGN_DrawRectFromIVA(SAMPLE_RGN_CTX_S *ctx, RK_BOOL bDraw) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wint-conversion"

	RK_S32 s32Ret = RK_SUCCESS;
	int line_pixel = 2;
	int video_width = ctx->stRegion.u32Width;
	int video_height = ctx->stRegion.u32Height;
	RGN_HANDLE RgnHandle = ctx->rgnHandle;
	RGN_CANVAS_INFO_S stCanvasInfo;
	RockIvaDetectResult *iva_result = NULL;
	RockIvaObjectInfo *objectInfo = NULL;
	struct timespec now;
	iva_result = SAMPLE_COMM_IVA_Pop_Result();
	if (iva_result == NULL) {
		RK_LOGI("empty iva result list...\n");
	}
	s32Ret = RK_MPI_RGN_GetCanvasInfo(RgnHandle, &stCanvasInfo);
	if (s32Ret != RK_SUCCESS) {
		SAMPLE_COMM_IVA_Release_Result(iva_result);
		RK_LOGE("RK_MPI_RGN_GetCanvasInfo failed with %#x!", s32Ret);
		return s32Ret;
	}

	if (ctx->u32BmpFormat == RK_FMT_2BPP)
		memset((void *)stCanvasInfo.u64VirAddr, 0,
		       stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight >> 2);
	else
		memset((void *)stCanvasInfo.u64VirAddr, 0,
		       stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight);
	if (iva_result != NULL) {
		for (int i = 0; i < iva_result->objNum; i++) {
			// only draw rect on multi frame
			if (!bDraw)
				break;
			RK_LOGD("topLeft:[%d,%d], bottomRight:[%d,%d],"
			        "objId is %d, frameId is %d, score is %d, type is %d\n",
			        iva_result->objInfo[i].rect.topLeft.x,
			        iva_result->objInfo[i].rect.topLeft.y,
			        iva_result->objInfo[i].rect.bottomRight.x,
			        iva_result->objInfo[i].rect.bottomRight.y,
			        iva_result->objInfo[i].objId, iva_result->objInfo[i].frameId,
			        iva_result->objInfo[i].score, iva_result->objInfo[i].type);
			int x, y, w, h;
			objectInfo = &iva_result->objInfo[i];
			x = video_width * objectInfo->rect.topLeft.x / 10000;
			y = video_height * objectInfo->rect.topLeft.y / 10000;
			w = video_width *
			    (objectInfo->rect.bottomRight.x - objectInfo->rect.topLeft.x) / 10000;
			h = video_height *
			    (objectInfo->rect.bottomRight.y - objectInfo->rect.topLeft.y) / 10000;
			x = x / 16 * 16;
			y = y / 16 * 16;
			w = (w + 3) / 16 * 16;
			h = (h + 3) / 16 * 16;
			while (x + w + line_pixel >= video_width)
				w -= 8;
			while (y + h + line_pixel >= video_height)
				h -= 8;
			RK_LOGD("draw rect x:%d, y:%d, w:%d, h:%d\n", x, y, w, h);
			if (x >= 0 && y >= 0 && w > 0 && h > 0) {
				if (ctx->u32BmpFormat == RK_FMT_2BPP) {
					if (objectInfo->type == ROCKIVA_OBJECT_TYPE_PERSON) {
						draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_0);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_FACE) {
						draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_0);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_VEHICLE) {
						draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_1);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_NON_VEHICLE) {
						draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_1);
					}
				} else {
					if (objectInfo->type == ROCKIVA_OBJECT_TYPE_PERSON) {
						draw_rect_8bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_0);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_FACE) {
						draw_rect_8bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_0);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_VEHICLE) {
						draw_rect_8bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_1);
					} else if (objectInfo->type == ROCKIVA_OBJECT_TYPE_NON_VEHICLE) {
						draw_rect_8bpp((RK_U8 *)stCanvasInfo.u64VirAddr,
						               stCanvasInfo.u32VirWidth,
						               stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
						               RGN_COLOR_LUT_INDEX_1);
					}
				}
			}
		}
		SAMPLE_COMM_IVA_Release_Result(iva_result);
	}
	s32Ret = RK_MPI_RGN_UpdateCanvas(RgnHandle);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("RK_MPI_RGN_UpdateCanvas failed with %#x!", s32Ret);
	return s32Ret;
#pragma GCC diagnostic pop
}

static int fill_text(osd_data_s *data) {
	if (data->text.font_path == NULL) {
		RK_LOGE("font_path is NULL\n");
		return -1;
	}
	set_font_color(data->text.font_color);
	draw_argb8888_text(data->buffer, data->width, data->height, data->text.wch);
	return 0;
}
static int generate_date_time(wchar_t *result, const int r_size, char *append) {
	char time_str[64];
	time_t curtime;
	curtime = time(0);
	// strftime(time_str, sizeof(time_str), "%Y-%m-%d %A %H:%M:%S", localtime(&curtime));
	strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H:%M:%S", localtime(&curtime));
	if (append)
		sprintf(time_str + strlen(time_str), "%s", append);
	return swprintf(result, r_size, L"%s", time_str);
}
RK_S32 SAMPLE_COMM_RGN_DrawOsd(SAMPLE_RGN_CTX_S *ctx, RK_BOOL bAOV, RK_FLOAT Fps) {
	RK_S32 s32Ret = RK_SUCCESS;

	BITMAP_S stBitmap;
	time_t rawtime;
	struct tm *cur_time_info;
	static int last_time_sec;
	int wchar_cnt;
	char osd_append_char[15];

	if (!ctx->st_osd_data.enable) {
		RK_LOGE("st_osd_data.enable must be enable\n");
		return RK_FAILURE;
	}
	time(&rawtime);
	cur_time_info = localtime(&rawtime);
	if (cur_time_info->tm_sec == last_time_sec)
		return s32Ret;
	else
		last_time_sec = cur_time_info->tm_sec;
	// generate time string.
	RK_LOGD("bAov %d, Fps %.1f\n", bAOV, Fps);
	if (bAOV) {
		sprintf(osd_append_char, "_AOV_%.1fFPS", Fps);
	} else {
		sprintf(osd_append_char, "_Normal_%.1fFPS", Fps);
	}

	wchar_cnt =
	    generate_date_time(ctx->st_osd_data.text.wch, MAX_WCH_BYTE, osd_append_char);
	RK_LOGD("wchar_cnt = %d\n", wchar_cnt);
	if (wchar_cnt <= 0) {
		RK_LOGE("generate_date_time error\n");
		return -1;
	}
	// calculate really buffer size and allocate buffer for time string.
	ctx->st_osd_data.width =
	    UPALIGNTO16(wstr_get_actual_advance_x(ctx->st_osd_data.text.wch) / 64);
	ctx->st_osd_data.height = UPALIGNTO16(ctx->st_osd_data.text.font_size);
	ctx->st_osd_data.size =
	    ctx->st_osd_data.width * ctx->st_osd_data.height * 4; // BGRA8888 4byte
	ctx->st_osd_data.buffer = malloc(ctx->st_osd_data.size);
	RK_LOGD("ctx->st_osd_data.width %d, ctx->st_osd_data.height = %d, "
	        "ctx->st_osd_data.size = %d\n",
	        ctx->st_osd_data.width, ctx->st_osd_data.height, ctx->st_osd_data.size);
	memset(ctx->st_osd_data.buffer, 0, ctx->st_osd_data.size);
	// draw font in buffer
	fill_text(&ctx->st_osd_data);
	// set bitmap
	stBitmap.enPixelFormat = RK_FMT_ARGB8888;
	stBitmap.u32Width = ctx->st_osd_data.width;
	stBitmap.u32Height = ctx->st_osd_data.height;
	stBitmap.pData = (RK_VOID *)ctx->st_osd_data.buffer;
	s32Ret = RK_MPI_RGN_SetBitMap(ctx->rgnHandle, &stBitmap);
	if (s32Ret != RK_SUCCESS)
		RK_LOGE("RK_MPI_RGN_SetBitMap failed with %#x\n", s32Ret);
	free(ctx->st_osd_data.buffer);
	return s32Ret;
}

RK_S32 SAMPLE_COMM_RGN_DestroyChn(SAMPLE_RGN_CTX_S *ctx) {
	RK_S32 s32Ret = RK_SUCCESS;

	if (ctx->st_osd_data.enable)
		destroy_font();

	s32Ret = RK_MPI_RGN_DetachFromChn(ctx->rgnHandle, &ctx->stMppChn);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_DetachFrmChn (%d) failed with %#x!", ctx->rgnHandle, s32Ret);
		return RK_FAILURE;
	}

	s32Ret = RK_MPI_RGN_Destroy(ctx->rgnHandle);
	if (RK_SUCCESS != s32Ret) {
		RK_LOGE("RK_MPI_RGN_Destroy [%d] failed with %#x", ctx->rgnHandle, s32Ret);
		return RK_FAILURE;
	}

	return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
