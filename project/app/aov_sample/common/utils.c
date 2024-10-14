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

#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static RK_U8 rgn_color_lut_0_left_value[4] = {0x03, 0xf, 0x3f, 0xff};
static RK_U8 rgn_color_lut_0_right_value[4] = {0xc0, 0xf0, 0xfc, 0xff};
static RK_U8 rgn_color_lut_1_left_value[4] = {0x02, 0xa, 0x2a, 0xaa};
static RK_U8 rgn_color_lut_1_right_value[4] = {0x80, 0xa0, 0xa8, 0xaa};

RK_S32 draw_rect_2bpp(RK_U8 *buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y,
                      int rgn_w, int rgn_h, int line_pixel, COLOR_INDEX_E color_index) {
	int i;
	RK_U8 *ptr = buffer;
	RK_U8 value = 0;
	if (color_index == RGN_COLOR_LUT_INDEX_0)
		value = 0xff;
	if (color_index == RGN_COLOR_LUT_INDEX_1)
		value = 0xaa;

	if (line_pixel > 4) {
		printf("line_pixel > 4, not support\n");
		return -1;
	}

	// printf("YUV %dx%d, rgn (%d,%d,%d,%d), line pixel %d\n", width, height, rgn_x,
	// rgn_y, rgn_w, rgn_h, line_pixel); draw top line
	ptr += (width * rgn_y + rgn_x) >> 2;
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, value, (rgn_w + 3) >> 2);
		ptr += width >> 2;
	}
	// draw letft/right line
	for (i = 0; i < (rgn_h - line_pixel * 2); i++) {
		if (color_index == RGN_COLOR_LUT_INDEX_1) {
			*ptr = rgn_color_lut_1_left_value[line_pixel - 1];
			*(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_1_right_value[line_pixel - 1];
		} else {
			*ptr = rgn_color_lut_0_left_value[line_pixel - 1];
			*(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_0_right_value[line_pixel - 1];
		}
		ptr += width >> 2;
	}
	// draw bottom line
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, value, (rgn_w + 3) >> 2);
		ptr += width >> 2;
	}
	return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
RK_S32 draw_rect_8bpp(RK_U64 buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y,
                      int rgn_w, int rgn_h, int line_pixel, COLOR_INDEX_E color_index) {
	// actual draw color index, need set color table, and rga dst not support
	// RK_FORMAT_BPP8
	int i, j;
	int pixel_format_byte = 1;
	RK_U8 *ptr = (RK_U8 *)buffer;
	RK_U8 color = 0;

	if (color_index == RGN_COLOR_LUT_INDEX_0)
		color = 13; // blue
	if (color_index == RGN_COLOR_LUT_INDEX_1)
		color = 10; // red
	// LOG_DEBUG("YUV %dx%d, rgn (%d,%d,%d,%d), line pixel %d\n", width, height, rgn_x,
	// rgn_y, rgn_w,
	//          rgn_h, line_pixel);

	//  draw top line
	ptr += (width * rgn_y + rgn_x);
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, color,
		       (rgn_w + line_pixel) * pixel_format_byte); // memset is byte
		ptr += width;
	}
	// draw letft/right line
	for (i = 0; i < (rgn_h - line_pixel * 2); i++) {
		for (j = 0; j < line_pixel; j++) {
			*(ptr + j) = color;
			*(ptr + rgn_w + j) = color;
		}
		ptr += width;
	}
	// draw bottom line
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, color,
		       (rgn_w + line_pixel) * pixel_format_byte); // memset is byte
		ptr += width;
	}

	return 0;
}
#pragma GCC diagnostic pop

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
