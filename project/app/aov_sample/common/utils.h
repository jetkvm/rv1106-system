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
#ifndef __UTILS_H__
#define __UTILS_H__

#include "rk_common.h"

typedef enum rkCOLOR_INDEX_E {
	RGN_COLOR_LUT_INDEX_0 = 0,
	RGN_COLOR_LUT_INDEX_1 = 1,
} COLOR_INDEX_E;

RK_S32 draw_rect_2bpp(RK_U8 *buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y,
                      int rgn_w, int rgn_h, int line_pixel, COLOR_INDEX_E color_index);

RK_S32 draw_rect_8bpp(RK_U64 buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y,
                      int rgn_w, int rgn_h, int line_pixel, COLOR_INDEX_E color_index);

#endif
