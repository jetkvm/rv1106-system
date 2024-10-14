
ifeq ($(APP_PARAM), )
    APP_PARAM:=../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

PKG_NAME := sample_aov
PKG_BIN ?= out

COMM_DIR := $(CURRENT_DIR)/common
COMM_SRC := $(wildcard $(COMM_DIR)/*.c)
$(info "---------------- RK_CHIP $(RK_CHIP) -----------------")
ifeq ($(RK_CHIP),rv1126)
	COMM_SRC += $(wildcard $(COMM_DIR)/isp2.x/*.c)
	COMM_SRC += $(wildcard $(COMM_DIR)/aov/sample_comm_aov_rv1126.c)
else
	COMM_SRC += $(wildcard $(COMM_DIR)/isp3.x/*.c)
	COMM_SRC += $(wildcard $(COMM_DIR)/aov/sample_comm_aov_rv1106.c)
endif
COMM_OBJ := $(COMM_SRC:%.c=%.o)

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH) \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/libdrm \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/algos \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/common \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/xcore \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser_v2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/smartIr \
				-I $(RK_APP_OUTPUT)/include \
				-I $(RK_APP_OUTPUT)/include/freetype2 \
				-I $(COMM_DIR) \
				-I $(COMM_DIR)/aov
ifeq ($(RK_CHIP),rv1126)
	RK_APP_CFLAGS += -I $(COMM_DIR)/isp2.x
	SAMPLE_AOV_LD_FLAGS += -DRV1126
else
	RK_APP_CFLAGS += -I $(COMM_DIR)/isp3.x
	SAMPLE_AOV_LD_FLAGS += -DRV1106
endif

RK_APP_LDFLAGS = -L $(RK_APP_MEDIA_LIBS_PATH) -L$(RK_APP_OUTPUT)/lib

RK_APP_OPTS += -Wl,-rpath-link,$(RK_APP_MEDIA_LIBS_PATH):$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib:$(RK_APP_OUTPUT)/lib

ifeq ($(RK_AOV_APP_BUILD_STATIC),y)
SAMPLE_AOV_LD_FLAGS += $(RK_APP_OPTS) \
					   $(RK_APP_LDFLAGS) $(RK_APP_CFLAGS) \
					   -L$(RK_APP_OUTPUT)/lib -pthread \
					   -Wl,-Bstatic \
					   -laec_bf_process \
					   -lrkaudio_detect \
					   -lrockit \
					   -lrkaiq \
					   -lrockiva \
					   -lrknnmrt \
					   -lrockchip_mpp \
					   -lrkaudio\
					   -lrga \
					   -lrkaudio_common \
					   -Wl,-Bdynamic \
					   -lrtsp \
					   -O2 -mthumb -mthumb-interwork \
					   -lrkmuxer \
					   -Wl,--gc-sections -Wl,--as-needed \
					   -lstdc++ -lm \
					   -lfreetype

else
SAMPLE_AOV_LD_FLAGS += $(RK_APP_OPTS) \
	$(RK_APP_LDFLAGS) $(RK_APP_CFLAGS) \
	-L$(RK_APP_OUTPUT)/lib -pthread -lrockit -lrtsp \
	-O2 -mthumb -mthumb-interwork \
	-lrockchip_mpp -lrockiva -lrkmuxer -lrkaudio\
	-lrkaudio_detect -laec_bf_process -lrkaudio_common\
	-lrga -Wl,-Bdynamic \
	-Wl,--gc-sections -Wl,--as-needed \
	-lrkaiq -lstdc++ -lm \
	-lfreetype
endif

SAMPLE_AOV_LD_FLAGS += -DROCKIVA

SAMPLE_AOV_LD_FLAGS += -D_SAMPLE_AOV_ENABLE_KLOG_

ifeq ($(RK_ENABLE_FASTBOOT),y)
SAMPLE_AOV_LD_FLAGS += -DAOV_FASTBOOT_ENABLE
endif

ifeq ($(RK_ENABLE_AOV),y)
ifneq (, $(filter RK_SAMPLE_AOV, $(RK_APP_TYPE)))
PKG_TARGET := sample_aov-build
endif
RK_APP_OPTS += -DRK_ENABLE_AOV=1
endif

sample_aov_build_objs := sample_aov_vi sample_aov_vi_venc sample_aov_vi_iva_venc sample_aov_aiisp_iva_venc sample_aov_bind_test sample_aov_npu_test sample_aov_multi_vi sample_aov_multi_vi_venc sample_aov_multi_vi_iva_venc

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done"

sample_aov-build: $(sample_aov_build_objs)
	cp $(CURRENT_DIR)/RkLunch.sh $(PKG_BIN)/bin/RkLunch.sh
	cp $(CURRENT_DIR)/resume_suspend_test_2.sh $(PKG_BIN)/bin/resume_suspend_test.sh
	cp -fr $(CURRENT_DIR)/resource/* $(PKG_BIN)/usr/share/
	-cp hooks/pre-commit .git/hooks/pre-commit
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))

sample_aov_vi: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_vi.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_vi $(SAMPLE_AOV_LD_FLAGS)

sample_aov_vi_venc: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_vi_venc.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_vi_venc $(SAMPLE_AOV_LD_FLAGS)

sample_aov_vi_iva_venc: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_vi_iva_venc.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_vi_iva_venc $(SAMPLE_AOV_LD_FLAGS)

sample_aov_aiisp_iva_venc: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_aiisp_iva_venc.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_aiisp_iva_venc $(SAMPLE_AOV_LD_FLAGS)

sample_aov_bind_test: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_bind_test.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_bind_test $(SAMPLE_AOV_LD_FLAGS)

sample_aov_npu_test: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_npu_test.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_npu_test $(SAMPLE_AOV_LD_FLAGS)

sample_aov_multi_vi: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_multi_vi.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_multi_vi $(SAMPLE_AOV_LD_FLAGS)

sample_aov_multi_vi_venc: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_multi_vi_venc.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_multi_vi_venc $(SAMPLE_AOV_LD_FLAGS)

sample_aov_multi_vi_iva_venc: sample_aov-build-prepare
	$(RK_APP_CROSS)-gcc sample_aov_multi_vi_iva_venc.c $(COMM_SRC) -o $(PKG_BIN)/bin/sample_aov_multi_vi_iva_venc $(SAMPLE_AOV_LD_FLAGS)

sample_aov-build-prepare: thirdlibs-build
	rm -rf $(PKG_BIN);
	mkdir -p $(PKG_BIN)/bin;
	mkdir -p $(PKG_BIN)/usr/share;

thirdlibs-build:
	@test ! -d $(RK_APP_COMPONENT)/third_libs || make -C $(RK_APP_COMPONENT)/third_libs

clean:
	@rm -rf $(PKG_BIN)

distclean: clean

info:
ifneq ($(RK_SAMPLE_AOV),)
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "RK_APP_TYPE=$(RK_APP_TYPE)"
	@echo -e "option support as follow:"
	@echo -e "	RK_SAMPLE_AOV"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
endif
