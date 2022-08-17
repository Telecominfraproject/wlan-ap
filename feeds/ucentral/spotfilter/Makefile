#
# Copyright (C) 2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=spotfilter
PKG_VERSION:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_BUILD_DEPENDS:=bpf-headers
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk
include $(INCLUDE_DIR)/bpf.mk
include $(INCLUDE_DIR)/nls.mk

define Package/spotfilter
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Network filter for hotspot services
  DEPENDS:=+libbpf +libubox +libubus +libnl-tiny +kmod-sched-cake +kmod-sched-bpf $(BPF_DEPENDS)
endef

TARGET_CFLAGS += \
	-Wno-error=deprecated-declarations \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-I$(STAGING_DIR)/usr/include -g3

CMAKE_OPTIONS += \
	-DLIBNL_LIBS=-lnl-tiny

define Build/Compile
	$(call CompileBPF,$(PKG_BUILD_DIR)/spotfilter-bpf.c)
	$(Build/Compile/Default)
endef

define Package/spotfilter/install
	$(INSTALL_DIR) \
		$(1)/etc/hotplug.d/net \
		$(1)/etc/init.d \
		$(1)/lib/bpf \
		$(1)/usr/sbin
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/spotfilter-bpf.o $(1)/lib/bpf
	$(INSTALL_BIN) ./files/spotfilter.init $(1)/etc/init.d/spotfilter
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/spotfilter $(1)/usr/sbin/
	$(INSTALL_DATA) ./files/spotfilter.hotplug $(1)/etc/hotplug.d/net/10-spotfilter
endef

$(eval $(call BuildPackage,spotfilter))
