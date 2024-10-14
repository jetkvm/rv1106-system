#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include "sample_comm.h"
#include "sample_comm_aov.h"
#include "rk_mpi_adec.h"
#include "rk_mpi_aenc.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_amix.h"

#define MOUNT_PATH "/mnt/sdcard"
#define TEST_FILE_PATH "/mnt/sdcard/aov_test.txt"
#define TEST_FILE_SIZE (1024 * 128)

static RK_CHAR optstr[] = "?:l:s:e:a:o:";

static char *output_path = NULL;

static const struct option long_options[] = {
    {"loop_count", required_argument, NULL, 'l'},
    {"help", optional_argument, NULL, '?'},
    {"enable_ethernet", optional_argument, NULL, 'e'},
    {"enable_sdcard", optional_argument, NULL, 's'},
    {"enable_audio", optional_argument, NULL, 'a'},
    {"output_path", optional_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
};

static void print_usage(const RK_CHAR *name) {
	printf("usage example:\n");
	printf("\t-l | --loop_count: total test loop, Default 1\n");
	printf("\t-e | --enable_ethernet: enable ethernet, Default 0\n");
	printf("\t-s | --enable_sdcard: enable sdcard, Default 0\n");
	printf("\t-a | --enable_audio: enable sdcard, Default 0\n");
	printf("\t-o | --output_path: set audio output file path, Default NULL\n");
}

#if defined(RV1126)
#define MOUNT_DEV_1 "/dev/mmcblk2p1"
#define MOUNT_DEV_2 "/dev/mmcblk2"
#else
#define MOUNT_DEV_1 "/dev/mmcblk1p1"
#define MOUNT_DEV_2 "/dev/mmcblk1"
#endif

static int mount_sdcard() {
	// mount sd
	int ret = RK_SUCCESS;
	if (access(MOUNT_DEV_1, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1p1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_1, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else if (access(MOUNT_DEV_2, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_2, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else {
		printf("[%s()] bad mount path!\n", __func__);
		// goto SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end;
		ret = RK_FAILURE;
	}
	if (ret == EBUSY)
		return RK_SUCCESS;
	else
		return ret;
}

static int unmount_sdcard() {
	int ret = 0;
	ret = umount2(MOUNT_PATH, MNT_DETACH);
	if (ret == 0)
		printf("[%s()] unmount success\n", __func__);
	else
		printf("[%s()] unmount failed because %s\n", __func__, strerror(errno));

	return ret;
}

static int create_test_file() {
	int fd;
	int random = rand();
	int write_arr[TEST_FILE_SIZE];
	int i = 0;
	fd = open(TEST_FILE_PATH, O_RDWR | O_TRUNC | O_CREAT, 777);
	if (fd < 0) {
		printf("[%s()] open failed: %s\n", __func__, strerror(errno));
		return RK_FAILURE;
	}
	for (i = 0; i != TEST_FILE_SIZE; ++i)
		write_arr[i] = (random * i);
	write(fd, write_arr, sizeof(write_arr));
	fsync(fd);
	close(fd);
	return RK_SUCCESS;
}

static int calculate_md5(char *dst_buf, int size) {
	char src_buf[512] = {'\0'};
	char *ptr;
	FILE *fp = NULL;
	int i = 0;
	int status;

	fp = popen("md5sum " TEST_FILE_PATH, "r");
	if (fp == NULL) {
		printf("[%s()] popen failed because %s\n", __func__, strerror(errno));
		return RK_FAILURE;
	}

	while (fgets(src_buf, sizeof(src_buf), fp) != NULL) {
		// printf("[%s()] %s\n", __func__, src_buf);
		continue;
	}

	src_buf[sizeof(src_buf) - 1] = '\0';
	i = 0;
	while (i < sizeof(src_buf) && src_buf[i] != '\0' && src_buf[i] != ' ') {
		i++;
	}

	memset(dst_buf, 0, size);
	strncpy(dst_buf, src_buf, i);

	status = pclose(fp);
	if (!WIFEXITED(status))
		printf("[%s()] child process exit error! %s\n", __func__, strerror(errno));
	return RK_SUCCESS;
}

static int ai_vqe_init(RK_S32 s32SampleRate) {
	AI_VQE_CONFIG_S stAiVqeConfig, stAiVqeConfig2;
	RK_S32 ret;
	RK_S32 s32VqeGapMs = 16;
	int s32DevId = 0;
	int s32ChnId = 0;
	const char *pVqeCfgPath = "/oem/usr/share/vqefiles/config_aivqe.json";

	// Need to config enCfgMode to VQE attr even the VQE is not enabled
	memset(&stAiVqeConfig, 0, sizeof(AI_VQE_CONFIG_S));
	if (pVqeCfgPath != RK_NULL) {
		stAiVqeConfig.enCfgMode = AIO_VQE_CONFIG_LOAD_FILE;
		memcpy(stAiVqeConfig.aCfgFile, pVqeCfgPath, strlen(pVqeCfgPath));
	}

	if (s32VqeGapMs != 16 && s32VqeGapMs != 10) {
		RK_LOGE("Invalid gap: %d, just supports 16ms or 10ms for AI VQE", s32VqeGapMs);
		return RK_FAILURE;
	}

	stAiVqeConfig.s32WorkSampleRate = s32SampleRate;
	stAiVqeConfig.s32FrameSample = s32SampleRate * s32VqeGapMs / 1000;
	ret = RK_MPI_AI_SetVqeAttr(s32DevId, s32ChnId, 0, 0, &stAiVqeConfig);
	if (ret != RK_SUCCESS) {
		RK_LOGE("%s: SetVqeAttr(%d,%d) failed with %#x", __FUNCTION__, s32DevId, s32ChnId,
		        ret);
		return ret;
	}

	ret = RK_MPI_AI_GetVqeAttr(s32DevId, s32ChnId, &stAiVqeConfig2);
	if (ret != RK_SUCCESS) {
		RK_LOGE("%s: SetVqeAttr(%d,%d) failed with %#x", __FUNCTION__, s32DevId, s32ChnId,
		        ret);
		return ret;
	}

	ret = memcmp(&stAiVqeConfig, &stAiVqeConfig2, sizeof(AI_VQE_CONFIG_S));
	if (ret != RK_SUCCESS) {
		RK_LOGE("%s: set/get vqe config is different: %d", __FUNCTION__, ret);
		return ret;
	}

	ret = RK_MPI_AI_EnableVqe(s32DevId, s32ChnId);
	if (ret != RK_SUCCESS) {
		RK_LOGE("%s: EnableVqe(%d,%d) failed with %#x", __FUNCTION__, s32DevId, s32ChnId,
		        ret);
		return ret;
	}

	return RK_SUCCESS;
}

static int ai_chn_init(RK_S32 InputSampleRate, RK_S32 OutputSampleRate,
                       RK_S32 u32FrameCnt, RK_S32 vqeEnable) {
	AIO_ATTR_S aiAttr;
	AI_CHN_PARAM_S pstParams;
	RK_S32 ret;
	int s32DevId = 0;
	int s32ChnId = 0;
	memset(&aiAttr, 0, sizeof(AIO_ATTR_S));

	RK_BOOL needResample = (InputSampleRate != OutputSampleRate) ? RK_TRUE : RK_FALSE;

	RK_MPI_SYS_Init();
#ifdef RV1126
	// 这是RV1126 声卡打开设置，RV1106设置无效，可以不设置
	ret = RK_MPI_AMIX_SetControl(s32DevId, "Capture MIC Path", (char *)"Main Mic");
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set Capture MIC Path fail, reason = %x", ret);
		goto __FAILED;
	}
#endif

	sprintf((char *)aiAttr.u8CardName, "%s", "hw:0,0");

	// s32DeviceSampleRate和s32SampleRate,s32SampleRate可以使用其他采样率，需要调用重采样函数。默认一样采样率。
	aiAttr.soundCard.channels = 2;
	aiAttr.soundCard.sampleRate = InputSampleRate;
	aiAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;
	aiAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
	aiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)OutputSampleRate;
	aiAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
	aiAttr.u32PtNumPerFrm = u32FrameCnt;
	// 以下参数无特殊需求，无需变动，保持默认值即可
	aiAttr.u32FrmNum = 4;
	aiAttr.u32EXFlag = 0;
	aiAttr.u32ChnCnt = 2;

	ret = RK_MPI_AI_SetPubAttr(s32DevId, &aiAttr);
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set attr fail, reason = %x", ret);
		goto __FAILED;
	}

	// 这是RV1106 回采设置，适用于左mic，右回采
	//  RV1126设置无效，可以不设置，RV1126需要配置asound.conf文件或者内核驱动配置软件回采
	ret =
	    RK_MPI_AMIX_SetControl(s32DevId, "I2STDM Digital Loopback Mode", (char *)"Mode2");
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set I2STDM Digital Loopback Mode fail, reason = %x", ret);
		goto __FAILED;
	}

	// 这是RV1106 ALC设置，而RV1126设置无效，可以不设置
	ret = RK_MPI_AMIX_SetControl(s32DevId, "ADC ALC Left Volume", (char *)"22");
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set alc left voulme fail, reason = %x", ret);
		goto __FAILED;
	}

	ret = RK_MPI_AMIX_SetControl(s32DevId, "ADC ALC Right Volume", (char *)"22");
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set alc right voulme fail, reason = %x", ret);
		goto __FAILED;
	}

	ret = RK_MPI_AI_Enable(s32DevId);
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai enable fail, reason = %x", ret);
		goto __FAILED;
	}

	memset(&pstParams, 0, sizeof(AI_CHN_PARAM_S));
	pstParams.s32UsrFrmDepth = 4; // 0, get fail, 1 - u32ChnCnt, can get, if bind to other
	                              // device, must be < u32ChnCnt
	ret = RK_MPI_AI_SetChnParam(s32DevId, s32ChnId, &pstParams);
	if (ret != RK_SUCCESS) {
		RK_LOGE("ai set channel params, s32ChnId = %d", s32ChnId);
		return RK_FAILURE;
	}

	// 使用声音增强功能，默认开启
	if (vqeEnable) {
		ai_vqe_init(InputSampleRate);
	}

	ret = RK_MPI_AI_EnableChn(s32DevId, s32ChnId);
	if (ret != 0) {
		RK_LOGE("ai enable channel fail, s32ChnId = %d, reason = %x", s32ChnId, ret);
		return RK_FAILURE;
	}

	// 重采样功能
	if (needResample == RK_TRUE) {
		RK_LOGI("need to resample %d -> %d", InputSampleRate, OutputSampleRate);
		ret = RK_MPI_AI_EnableReSmp(s32DevId, s32ChnId,
		                            (AUDIO_SAMPLE_RATE_E)OutputSampleRate);
		if (ret != 0) {
			RK_LOGE("ai enable channel fail, reason = %x, s32ChnId = %d", ret, s32ChnId);
			return RK_FAILURE;
		}
	}

	RK_MPI_AI_SetVolume(s32DevId, 100);

	// 双声道，左声道为MIC拾⾳数据，右声道为播放的右声道的回采数据
	RK_MPI_AI_SetTrackMode(s32DevId, AUDIO_TRACK_NORMAL);

	return RK_SUCCESS;
__FAILED:
	return RK_FAILURE;
}

static int ao_chn_init(RK_S32 s32ReSmpSampleRate, RK_S32 s32SampleRate,
                       RK_S32 u32FrameCnt) {
	RK_S32 ret = 0;
	AUDIO_DEV s32DevId = 0;
	AO_CHN s32ChnId = 0;
	AIO_ATTR_S aoAttr;
	AO_CHN_PARAM_S pstParams;
	RK_S32 s32SetVolume = 100;

	memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));
	memset(&aoAttr, 0, sizeof(AIO_ATTR_S));
#ifdef RV1126
	/*==============================================================================*/
	// 这是RV1126 声卡打开设置，RV1106设置无效，可以不设置
	ret = RK_MPI_AMIX_SetControl(s32DevId, "Playback Path", (char *)"SPK");
	if (ret != RK_SUCCESS) {
		RK_LOGE("ao set Playback Path fail, reason = %x", ret);
		return RK_FAILURE;
	}
#endif
	sprintf((char *)aoAttr.u8CardName, "%s", "hw:0,0");

	aoAttr.soundCard.channels = 2;               // 2
	aoAttr.soundCard.sampleRate = s32SampleRate; // s32SampleRate;       16000
	aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

	aoAttr.enBitwidth = AUDIO_BIT_WIDTH_16;                   // AUDIO_BIT_WIDTH_16
	aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)s32SampleRate; // 16000

	aoAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
	aoAttr.u32PtNumPerFrm = u32FrameCnt; // 1024
	// 以下参数没有特殊需要，无需修改
	aoAttr.u32FrmNum = 4;
	aoAttr.u32EXFlag = 0;
	aoAttr.u32ChnCnt = 2;

	RK_MPI_AO_SetPubAttr(s32DevId, &aoAttr);
	RK_MPI_AO_Enable(s32DevId);
	/*==============================================================================*/
	pstParams.enLoopbackMode = AUDIO_LOOPBACK_NONE;
	ret = RK_MPI_AO_SetChnParams(s32DevId, s32ChnId, &pstParams);
	if (ret != RK_SUCCESS) {
		RK_LOGE("ao set channel params, s32ChnId = %d", s32ChnId);
		return RK_FAILURE;
	}
	/*==============================================================================*/
	ret = RK_MPI_AO_EnableChn(s32DevId, s32ChnId);
	if (ret != 0) {
		RK_LOGE("ao enable channel fail, s32ChnId = %d, reason = %x", s32ChnId, ret);
		return RK_FAILURE;
	}
	/*==============================================================================*/
	// set sample rate of input data
	printf("********RK_MPI_AO_EnableReSmp*********\n");
	ret = RK_MPI_AO_EnableReSmp(s32DevId, s32ChnId,
	                            (AUDIO_SAMPLE_RATE_E)s32ReSmpSampleRate);
	if (ret != 0) {
		RK_LOGE("ao enable channel fail, reason = %x, s32ChnId = %d", ret, s32ChnId);
		return RK_FAILURE;
	}

	RK_MPI_AO_SetVolume(s32DevId, s32SetVolume);

	RK_MPI_AO_SetTrackMode(s32DevId, AUDIO_TRACK_OUT_STEREO);

	return RK_SUCCESS;
}

static int ao_chn_deinit() {
	RK_S32 ret = 0;
	AUDIO_DEV s32DevId = 0;
	AO_CHN s32ChnId = 0;

	RK_MPI_AO_DisableReSmp(s32DevId, s32ChnId);
	RK_MPI_AO_DisableChn(s32DevId, s32ChnId);
	RK_MPI_AO_Disable(s32DevId);

	return RK_SUCCESS;
}

static int ai_chn_deinit(RK_S32 vqeEnable) {
	RK_S32 ret;
	int s32DevId = 0;
	int s32ChnId = 0;

	if (vqeEnable) {
		RK_MPI_AI_DisableVqe(s32DevId, s32ChnId);

		// 这是RV1106 回采设置关闭，而RV1126设置无效，可以不配置
		ret =
		    RK_MPI_AMIX_SetControl(0, "I2STDM Digital Loopback Mode", (char *)"Disabled");
		if (ret != RK_SUCCESS)
			RK_LOGE("ai set I2STDM Digital Loopback Mode fail, reason = %x", ret);
	}

	RK_MPI_AI_DisableChn(s32DevId, s32ChnId);
	RK_MPI_AI_Disable(s32DevId);
	RK_MPI_SYS_Exit();
}

static int test_ai_ao_pipe() {
	int i = 0;
	RK_S32 s32MilliSec = 1000;
	AUDIO_FRAME_S frame;
	RK_S32 ret = RK_SUCCESS;
	FILE *save_file = NULL;
	char save_path[256] = {'\0'};
	if (output_path) {
		snprintf(save_path, 256, "%s/ai.pcm", output_path);
		save_file = fopen(save_path, "w");
		if (!save_file) {
			RK_LOGE("open ret file failed: %s", strerror(errno));
			return RK_FAILURE;
		}
	}
	for (int i = 0; i < 100; ++i) {
		ret = RK_MPI_AI_GetFrame(0, 0, &frame, RK_NULL, s32MilliSec);
		if (ret == RK_SUCCESS) {
			RK_LOGD("RK_MPI_AI_GetFrame %d success", i);
			if (save_file) {
				void *data = RK_MPI_MB_Handle2VirAddr(frame.pMbBlk);
				RK_U32 len = frame.u32Len;
				RK_LOGD("data = %p, len = %d", data, len);
				fwrite(data, len, 1, save_file);
				fflush(save_file);
			}
			ret = RK_MPI_AO_SendFrame(0, 0, &frame, s32MilliSec);
			if (ret != RK_SUCCESS) {
				RK_LOGE("RK_MPI_AO_SendFrame failed %#X", ret);
				RK_MPI_AI_ReleaseFrame(0, 0, &frame, RK_NULL);
				break;
			} else {
				RK_LOGD("RK_MPI_AO_SendFrame %d success", i);
			}

			RK_MPI_AI_ReleaseFrame(0, 0, &frame, RK_NULL);
		} else {
			RK_LOGE("RK_MPI_AI_GetFrame failed %#X", ret);
			break;
		}
	}
	if (save_file) {
		fflush(save_file);
		fclose(save_file);
	}
	return ret;
}

int main(int argc, char *argv[]) {
	int ret = RK_SUCCESS;
	int fd = -1;
	int loop_count = 1;
	int c;
	int i = 0;
	bool enable_sdcard = false;
	bool enable_ethernet = false;
	bool enable_audio = false;
	char old_buf[256] = {'\0'}, new_buf[256] = {'\0'};
	if (argc < 2) {
		print_usage(argv[0]);
		return RK_FAILURE;
	}
	while ((c = getopt_long(argc, argv, optstr, long_options, NULL)) != -1) {
		switch (c) {
		case 'l':
			loop_count = atoi(optarg);
			break;
		case 'e':
			enable_ethernet = atoi(optarg);
			break;
		case 's':
			enable_sdcard = atoi(optarg);
			break;
		case 'a':
			enable_audio = atoi(optarg);
			break;
		case 'o':
			output_path = optarg;
			break;
		case '?':
			print_usage(argv[0]);
		default:
			return RK_FAILURE;
		}
	}
	if (enable_sdcard) {
		SAMPLE_COMM_AOV_BindSdcard();
		mount_sdcard();
	}
	if (enable_ethernet)
		SAMPLE_COMM_AOV_BindEthernet();
	if (enable_audio) {
#if defined(RV1106)
		system("insmod /oem/usr/ko/snd-soc-rockchip-i2s-tdm.ko");
		system("insmod /oem/usr/ko/snd-soc-rv1106.ko");
		system("insmod /oem/usr/ko/snd-soc-simple-card-utils.ko");
		system("insmod /oem/usr/ko/snd-soc-simple-card.ko");
#elif defined(RV1126)
		system("insmod /oem/usr/ko/snd-soc-simple-card-utils.ko");
		system("insmod /oem/usr/ko/snd-soc-simple-card.ko");
		system("insmod /oem/usr/ko/snd-soc-rockchip-i2s-tdm.ko");
		system("insmod /oem/usr/ko/snd-soc-rockchip-pdm.ko");
#endif
	}

	SAMPLE_COMM_AOV_SetSuspendTime(100);

	printf("[%s()] -------------- Test Start! --------------\n", __func__);
	for (int i = 0; i < loop_count; ++i) {
		if (enable_sdcard) {
			ret = create_test_file();
			if (ret != RK_SUCCESS) {
				printf("[%s()] create test file failed!", __func__);
				break;
			}
			calculate_md5(old_buf, sizeof(old_buf));

			unmount_sdcard();
			ret = SAMPLE_COMM_AOV_UnbindSdcard();
			if (ret != RK_SUCCESS) {
				printf("[%s()] unbind sdcard failed! please check sdcard is exist\n",
				       __func__);
				break;
			}
		}
		if (enable_ethernet) {
			ret = SAMPLE_COMM_AOV_UnbindEthernet();
			if (ret != RK_SUCCESS) {
				printf("[%s()] unbind eternet failed! please check sdcard is exist\n",
				       __func__);
				break;
			}
		}
		if (enable_audio) {
			ret = SAMPLE_COMM_AOV_UnbindSoundcard();
			if (ret != RK_SUCCESS) {
				printf("[%s()] unbind sound card failed!\n", __func__);
				break;
			}
		}

		SAMPLE_COMM_AOV_EnterSleep();

		if (enable_audio) {
			ret = SAMPLE_COMM_AOV_BindSoundcard();
			if (ret != RK_SUCCESS) {
				printf("[%s()] bind sound card failed!\n", __func__);
				break;
			}
			ret = ai_chn_init(16000, 16000, 1024, 1);
			if (ret != RK_SUCCESS) {
				printf("[%s()] init ai failed %#X\n", __func__, ret);
				break;
			}
			ret = ao_chn_init(16000, 16000, 1024);
			if (ret != RK_SUCCESS) {
				printf("[%s()] init ao failed %#X\n", __func__, ret);
				break;
			}
			ret = test_ai_ao_pipe();
			if (ret != RK_SUCCESS) {
				printf("[%s()] test ai ao failed!\n", __func__);
				break;
			}
			ao_chn_deinit();
			ai_chn_deinit(1);
		}
		if (enable_ethernet) {
			ret = SAMPLE_COMM_AOV_BindEthernet();
			if (ret != RK_SUCCESS) {
				printf("[%s()] bind ethernet failed! please check sdcard is exist\n",
				       __func__);
				break;
			}
		}
		if (enable_sdcard) {
			ret = SAMPLE_COMM_AOV_BindSdcard();
			if (ret != RK_SUCCESS) {
				printf("[%s()] bind sdcard failed! please check sdcard is exist\n",
				       __func__);
				break;
			}
			ret = mount_sdcard();
			if (ret != RK_SUCCESS) {
				printf("[%s()] mount sdcard failed!\n", __func__);
				break;
			}
			calculate_md5(new_buf, sizeof(new_buf));
			printf("[%s()] pre md5 %s\n", __func__, old_buf);
			printf("[%s()] new md5 %s\n", __func__, new_buf);
			if (strncmp(new_buf, old_buf, sizeof(new_buf)) != 0) {
				printf("[%s()] test failed, test file is broken\n", __func__);
				ret = RK_FAILURE;
				break;
			}
		}
		printf("[%s()] -------------- Success count %d --------------\n", __func__, i);
	}
	if (ret == RK_SUCCESS)
		printf("[%s()] -------------- Test Success! --------------\n", __func__);
	else
		printf("[%s()] -------------- Test Failed! --------------\n", __func__);

	printf("[%s()] -------------- Test End! --------------\n", __func__);

	return ret;
}
