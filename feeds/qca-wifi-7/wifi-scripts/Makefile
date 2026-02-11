# Copyright (C) 2024 OpenWrt.org
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=wifi-scripts
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/wifi-scripts
  SECTION:=utils
  CATEGORY:=Base system
  DEPENDS:=+ucode +ucode-mod-nl80211 +ucode-mod-rtnl +ucode-mod-ubus +ucode-mod-uci
  TITLE:=Wi-Fi configuration scripts
  PKGARCH:=all
endef

define Package/wifi-scripts/description
 A set of scripts that handle setup and configuration of Wi-Fi devices.
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/wifi-scripts/install
	$(INSTALL_DIR) $(1)
ifeq ($(CONFIG_USE_PRPLMESH_WHM),y)
	sed -i 's/OpenWrt/prplOS/g' ./files/lib/wifi/mac80211.uc
endif
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,wifi-scripts))
