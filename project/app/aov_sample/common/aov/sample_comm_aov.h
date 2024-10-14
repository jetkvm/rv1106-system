// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __SAMPLE_COMM_AOV_H__
#define __SAMPLE_COMM_AOV_H__

#include "sample_comm.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef AOV_STREAM_SIZE_WRITE_TO_SDCARD
#define AOV_STREAM_SIZE_WRITE_TO_SDCARD (10 * 1024 * 1024)
#endif

int SAMPLE_COMM_AOV_Init();
int SAMPLE_COMM_AOV_Deinit();
void SAMPLE_COMM_AOV_EnterSleep();
int SAMPLE_COMM_AOV_SetSuspendTime(int wakeup_suspend_time);
int SAMPLE_COMM_AOV_CopyRawStreamToSdcard(int venc_chn_id, char *data, int data_size,
                                          char *data2, int data2_size);
int SAMPLE_COMM_AOV_InitMp4(SAMPLE_VENC_CTX_S *ctx, SAMPLE_MPI_MUXER_S **ppstMuxer);
int SAMPLE_COMM_AOV_DeinitMp4(SAMPLE_MPI_MUXER_S *pstMuxer);
int SAMPLE_COMM_AOV_StartRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer);
int SAMPLE_COMM_AOV_StopRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer);
bool SAMPLE_COMM_AOV_IsRecordingMp4(SAMPLE_MPI_MUXER_S *pstMuxer);
int SAMPLE_COMM_AOV_CopyMp4StreamToSdcard(SAMPLE_MPI_MUXER_S *pstMuxer,
                                          VENC_STREAM_S *frame, void *pData);

int SAMPLE_COMM_AOV_PreInitIsp(const char *sensor_name, const char *iq_file_dir,
                               int cam_index);

int SAMPLE_COMM_AOV_BindSdcard();
int SAMPLE_COMM_AOV_UnbindSdcard();
int SAMPLE_COMM_AOV_BindEthernet();
int SAMPLE_COMM_AOV_UnbindEthernet();
int SAMPLE_COMM_AOV_BindSoundcard();
int SAMPLE_COMM_AOV_UnbindSoundcard();
int SAMPLE_COMM_AOV_BindEmmc();
int SAMPLE_COMM_AOV_UnbindEmmc();

int SAMPLE_COMM_AOV_DisableNonBootCPUs();
int SAMPLE_COMM_AOV_EnableNonBootCPUs();

#ifdef RK_ENABLE_RTT
int SAMPLE_COMM_AOV_WakeupBinMmap(const char *rtthread_wakeup_bin_path);
int SAMPLE_COMM_AOV_WakeupParamCheck();
#endif

void SAMPLE_COMM_AOV_DumpPtsToTMP(uint32_t seq, uint64_t pts, int max_dump_pts_count);

bool SAMPLE_COMM_AOV_GetGpioIrqStat();

#endif // #ifndef __SAMPLE_COMM_AOV_H__
