#
# Copyright (C) 2024 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wifi-scripts
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

include $(INCLUDE_DIR)/package.mk

define Package/wifi-scripts
  SECTION:=utils
  CATEGORY:=Base system
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

WIFI_SCRIPTS_UPSTREAM_DIR:=$(TOPDIR)/package/network/config/wifi-scripts/files

define Package/wifi-scripts/install
	$(INSTALL_DIR) $(1)
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/lib/netifd
	$(CP) $(WIFI_SCRIPTS_UPSTREAM_DIR)/lib/netifd/wireless.uc $(1)/lib/netifd/
	$(CP) $(WIFI_SCRIPTS_UPSTREAM_DIR)/lib/netifd/wireless-device.uc $(1)/lib/netifd/
	$(INSTALL_DIR) $(1)/usr/share/ucode/wifi
	$(CP) $(WIFI_SCRIPTS_UPSTREAM_DIR)/usr/share/ucode/wifi/utils.uc $(1)/usr/share/ucode/wifi/
endef

$(eval $(call BuildPackage,wifi-scripts))
