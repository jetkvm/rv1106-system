
ifeq ($(SYSDRV_PARAM), )
    SYSDRV_PARAM:=../../../Makefile.param
    include $(SYSDRV_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)
PKG_TARBALL := nftables-1.1.5.tar.xz
PKG_NAME := nftables-1.1.5
PKG_BIN := out

DEPS_OUTPUT_DIR := $(CURRENT_DIR)/out

LIBMNL_TARBALL := libmnl-1.0.5.tar.bz2
LIBMNL_NAME := libmnl-1.0.5

LIBNFTNL_TARBALL := libnftnl-1.3.0.tar.xz
LIBNFTNL_NAME := libnftnl-1.3.0

NFT_CROSS_CFLAGS := $(SYSDRV_CROSS_CFLAGS)
NFT_CROSS_CFLAGS += -I$(DEPS_OUTPUT_DIR)/include
NFT_CROSS_CFLAGS += -I$(DEPS_OUTPUT_DIR)/include/libmnl
NFT_CROSS_CFLAGS += -I$(DEPS_OUTPUT_DIR)/include/libnftl

# this isn't a typo
NFT_CROSS_LDFLAGS := $(SYSDRV_CROSS_CFLAGS)
NFT_CROSS_LDFLAGS += -Wl,-rpath-link=$(DEPS_OUTPUT_DIR)/lib

LIBMNL_CFLAGS := -I$(DEPS_OUTPUT_DIR)/include
LIBMNL_LIBS := -L$(DEPS_OUTPUT_DIR)/lib -lmnl -Wl,-rpath-link=$(DEPS_OUTPUT_DIR)/lib

LIBNFTNL_CFLAGS := -I$(DEPS_OUTPUT_DIR)/include/libnftnl
LIBNFTNL_LIBS := -L$(DEPS_OUTPUT_DIR)/lib -lnftnl -Wl,-rpath-link=$(DEPS_OUTPUT_DIR)/lib

all: libmnl nftables
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(SYSDRV_DIR_OUT_ROOTFS), $(PKG_BIN))

nftables: libmnl libnftnl
	test -f $(PKG_BIN)/usr/sbin/nft || (\
	rm -rf $(CURRENT_DIR)/$(PKG_NAME); \
	tar -xf $(PKG_TARBALL); \
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME)/$(PKG_BIN); \
	mkdir -p $(CURRENT_DIR)/$(PKG_BIN)/usr/; \
	pushd $(CURRENT_DIR)/$(PKG_NAME)/; \
		LIBNFTNL_CFLAGS="$(LIBNFTNL_CFLAGS)" LIBNFTNL_LIBS="$(LIBNFTNL_LIBS)" \
		LIBMNL_CFLAGS="$(LIBMNL_CFLAGS)" LIBMNL_LIBS="$(LIBMNL_LIBS)" \
		./configure --host=$(SYSDRV_CROSS) --target=$(SYSDRV_CROSS) CFLAGS="$(NFT_CROSS_CFLAGS)" \
			LDFLAGS="$(NFT_CROSS_LDFLAGS)" --prefix=$(CURRENT_DIR)/$(PKG_BIN) \
			--with-mini-gmp --without-cli --disable-debug --disable-man-doc; \
		make -j$(SYSDRV_JOBS) || exit -1; \
		make install; \
		mv $(CURRENT_DIR)/$(PKG_BIN)/sbin $(CURRENT_DIR)/$(PKG_BIN)/usr ; \
		mkdir -p $(CURRENT_DIR)/$(PKG_BIN)/usr/lib; \
		cp $(DEPS_OUTPUT_DIR)/lib/*.so* $(CURRENT_DIR)/$(PKG_BIN)/usr/lib; \
	popd; )

libnftnl: libmnl
	@test -f $(DEPS_OUTPUT_DIR)/lib/libnftnl.so || (\
	rm -rf $(CURRENT_DIR)/$(LIBNFTNL_NAME); \
	tar -xf $(LIBNFTNL_TARBALL); \
	mkdir -p $(DEPS_OUTPUT_DIR); \
	pushd $(CURRENT_DIR)/$(LIBNFTNL_NAME); \
		echo "CFLAGS=$(SYSDRV_CROSS_CFLAGS) LIBMNL_CFLAGS=$(LIBMNL_CFLAGS) LIBMNL_LIBS=$(LIBMNL_LIBS)"; \
		./configure --host=$(SYSDRV_CROSS) --target=$(SYSDRV_CROSS) CFLAGS="$(SYSDRV_CROSS_CFLAGS)" \
			LDFLAGS="$(SYSDRV_CROSS_CFLAGS)" LIBMNL_CFLAGS="$(LIBMNL_CFLAGS)" LIBMNL_LIBS="$(LIBMNL_LIBS)" \
			--prefix=$(DEPS_OUTPUT_DIR) > /dev/null; \
		make -j$(SYSDRV_JOBS) || (exit -1 && echo "Error building libnftnl"); \
		make install; \
		echo "Finished building libnftnl"; \
	popd; )

libmnl:
	@test -f $(DEPS_OUTPUT_DIR)/lib/libmnl.so | (\
	rm -rf $(CURRENT_DIR)/$(LIBMNL_NAME); \
	tar -xf $(LIBMNL_TARBALL); \
	mkdir -p $(DEPS_OUTPUT_DIR); \
	pushd $(CURRENT_DIR)/$(LIBMNL_NAME); \
		echo "CFLAGS=$(SYSDRV_CROSS_CFLAGS)"; \
		./configure --host=$(SYSDRV_CROSS) --target=$(SYSDRV_CROSS) CFLAGS="$(SYSDRV_CROSS_CFLAGS)" \
			LDFLAGS="$(SYSDRV_CROSS_CFLAGS)" --prefix=$(DEPS_OUTPUT_DIR) > /dev/null; \
		make -j$(SYSDRV_JOBS) || (exit -1 && echo "Error building libmnl"); \
		make install; \
		echo "Finished building libmnl"; \
	popd; )

clean: distclean

distclean:
	-rm -rf $(PKG_NAME) $(PKG_BIN)

