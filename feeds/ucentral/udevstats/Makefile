#
# Copyright (C) 2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=udevstats
PKG_VERSION:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_BUILD_DEPENDS:=bpf-headers

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/bpf.mk

define Package/udevstats
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Device statistics module
  DEPENDS:=+ucode +ucode-mod-ubus +ucode-mod-bpf +kmod-sched-bpf +ucode-mod-struct
endef

define Build/Compile
	$(call CompileBPF,$(PKG_BUILD_DIR)/udevstats-bpf.c)
endef

define Package/udevstats/conffiles
/etc/config/udevstats
endef

define Package/udevstats/install
	$(INSTALL_DIR) \
		$(1)/etc/hotplug.d/net \
		$(1)/etc/init.d \
		$(1)/etc/config \
		$(1)/lib/bpf \
		$(1)/usr/sbin
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/udevstats-bpf.o $(1)/lib/bpf/udevstats.o
	$(INSTALL_BIN) ./files/udevstats.init $(1)/etc/init.d/udevstats
	$(INSTALL_BIN) ./files/udevstats.uc $(1)/usr/sbin/udevstats
	$(INSTALL_DATA) ./files/udevstats.conf $(1)/etc/config/udevstats
	$(INSTALL_DATA) ./files/udevstats.hotplug $(1)/etc/hotplug.d/net/10-udevstats
endef

$(eval $(call BuildPackage,udevstats))
