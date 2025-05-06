
ifeq ($(SYSDRV_PARAM), )
    SYSDRV_PARAM:=../../../Makefile.param
    include $(SYSDRV_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)
PKG_TARBALL := curl-8.13.0.tar.bz2
PKG_NAME := curl-8.13.0
PKG_BIN := out

PKG_DEP_ZLIB := $(CURRENT_DIR)/../toolkits/zlib
PKG_DEP_OPENSSL := $(CURRENT_DIR)/../toolkits/openssl
PKG_DEP_LIBUV := $(CURRENT_DIR)/../toolkits/libuv
SYSDRV_CROSS_CFLAGS += $(SYSDRV_OPTS) -Os

all: dep-libuv
	@test -f $(PKG_BIN)/bin/curl || (\
	rm -rf $(CURRENT_DIR)/$(PKG_NAME); \
	tar -xf $(PKG_TARBALL); \
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME)/$(PKG_BIN); \
	mkdir -p $(CURRENT_DIR)/$(PKG_BIN); \
	pushd $(CURRENT_DIR)/$(PKG_NAME)/; \
		./configure --host=$(SYSDRV_CROSS) --target=$(SYSDRV_CROSS) \
			CFLAGS="$(SYSDRV_CROSS_CFLAGS) -I$(PKG_DEP_ZLIB)/out/include -I$(PKG_DEP_OPENSSL)/out/include" \
			LDFLAGS="$(SYSDRV_CROSS_CFLAGS) -L$(PKG_DEP_ZLIB)/out/lib -L$(PKG_DEP_OPENSSL)/out/lib" \
			--prefix=$(CURRENT_DIR)/$(PKG_BIN) \
			$(PKG_CONF_OPTS) \
			--with-openssl \
			--disable-pop3 \
			--disable-imap \
			--disable-smtp \
			--disable-gopher \
			--disable-rtsp \
			--disable-aws \
			--disable-kerberos-auth \
			--disable-negotiate-auth \
			--disable-ntlm \
			--disable-ares \
			--disable-dnsshuffle \
			--disable-hsts \
			--disable-manual \
			--disable-netrc \
			--disable-socketpair \
			--disable-tls-srp \
			--disable-verbose \
			--disable-versioned-symbols \
			--disable-docs \
			--enable-symbol-hiding \
			--without-brotli \
			--without-libpsl \
			--without-nghttp2 \
			--without-ngtcp2 \
			--without-librtmp \
			--without-libidn2 \
			--without-zstd || exit -1;\
		make -j$(SYSDRV_JOBS) > /dev/null || exit -1; \
		make install > /dev/null; \
		rm -rf $(CURRENT_DIR)/$(PKG_BIN)/share; \
	popd; )
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(SYSDRV_DIR_OUT_ROOTFS), $(PKG_BIN))

dep-libuv:
	$(MAKE) -C $(PKG_DEP_LIBUV)

clean: distclean

distclean:
	-rm -rf $(PKG_NAME) $(PKG_BIN)

