#
# Copyright (C) 2022 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=unetd
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/nbd168/unetd
PKG_SOURCE_DATE:=2022-06-30
PKG_SOURCE_VERSION:=425e75e3c38a7f1b6b732ff6ad0475073a6125b6
PKG_MIRROR_HASH:=1a3aa840046ded3524fd5eeed984ba500755ad641bf8c8df62b1d137bb5c76cd

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_BUILD_PARALLEL:=1
PKG_BUILD_DEPENDS:=bpf-headers

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk
include $(INCLUDE_DIR)/bpf.mk
include $(INCLUDE_DIR)/nls.mk

define Package/libunet
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=unetd support library
  DEPENDS:=+libubox
endef

define Package/unetd
  SECTION:=utils
  CATEGORY:=Base system
  TITLE:=OpenWrt wireguard overlay network configuration service
  DEPENDS:=+libubox +libubus +libblobmsg-json +libnl-tiny +libunet +libbpf $(BPF_DEPENDS) +TARGET_ipq807x:kmod-wireguard-backport +!TARGET_ipq807x:kmod-wireguard +wireguard-tools
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-I$(STAGING_DIR)/usr/include

CMAKE_OPTIONS += \
	-DLIBNL_LIBS=-lnl-tiny

define Build/Compile
	$(call CompileBPF,$(PKG_BUILD_DIR)/mss-bpf.c)
	$(call Build/Compile/Default,)
endef

define Package/libunet/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/libunet.so* $(1)/usr/lib/
endef

define Package/unetd/install
	$(INSTALL_DIR) \
		$(1)/lib/bpf \
		$(1)/etc/init.d \
		$(1)/lib/netifd/proto \
		$(1)/usr/sbin
	$(INSTALL_BIN) \
		$(PKG_INSTALL_DIR)/usr/sbin/unetd \
		$(1)/usr/sbin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/mss-bpf.o $(1)/lib/bpf/mss.o
	$(INSTALL_BIN) ./files/unetd.init $(1)/etc/init.d/unetd
	$(INSTALL_BIN) ./files/unetd.sh $(1)/lib/netifd/proto
endef

$(eval $(call BuildPackage,libunet))
$(eval $(call BuildPackage,unetd))
