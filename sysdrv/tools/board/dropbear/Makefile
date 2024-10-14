
ifeq ($(SYSDRV_PARAM), )
    SYSDRV_PARAM:=../../../Makefile.param
    include $(SYSDRV_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)
PKG_TARBALL := dropbear-2024.85.tar.bz2
PKG_NAME := dropbear-2024.85
PKG_BIN := out

PKG_DEP_ZLIB := $(CURRENT_DIR)/../toolkits/zlib

SYSDRV_CROSS_CFLAGS += $(SYSDRV_OPTS) -Os

all:
	@test -f $(PKG_BIN)/bin/dropbear || (\
	rm -rf $(CURRENT_DIR)/$(PKG_NAME); \
	tar -xf $(PKG_TARBALL); \
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME)/$(PKG_BIN); \
	mkdir -p $(CURRENT_DIR)/$(PKG_BIN); \
	pushd $(CURRENT_DIR)/$(PKG_NAME)/; \
		./configure --host=$(SYSDRV_CROSS) --target=$(SYSDRV_CROSS) \
			CFLAGS="$(SYSDRV_CROSS_CFLAGS) -I$(PKG_DEP_ZLIB)/out/include" \
			LDFLAGS="$(SYSDRV_CROSS_CFLAGS) -L$(PKG_DEP_ZLIB)/out/lib" \
			--prefix=$(CURRENT_DIR)/$(PKG_BIN) \
			$(PKG_CONF_OPTS) \
			runstatedir=/run \
			--disable-dependency-tracking \
			--enable-mpers=no \
			--with-libiberty || exit -1;\
		make -j$(SYSDRV_JOBS) > /dev/null || exit -1; \
		make install > /dev/null; \
		rm -rf $(CURRENT_DIR)/$(PKG_BIN)/share; \
	popd; )
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(SYSDRV_DIR_OUT_ROOTFS), $(PKG_BIN))

clean: distclean

distclean:
	-rm -rf $(PKG_NAME) $(PKG_BIN)

