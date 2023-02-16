#
# Copyright (C) 2023 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mcu
PKG_VERSION:=2023-03-05
PKG_RELEASE=1

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=Piotr Dymacz <pepe2k@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/mcu
  TITLE:=Generic OpenWrt/OpenWiFi MCU config and management handler
  CATEGORY:=Utilities
  DEPENDS:=+umcumgr +libubox
  SECTION:=utils
endef

define Package/mcu/conffiles
  /etc/config/mcu
endef

define Build/Compile
endef

define Package/mcu/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/hotplug.d/usb $(1)/lib/functions
	$(INSTALL_BIN) ./files/mcu.init $(1)/etc/init.d/mcu
	$(INSTALL_DATA) ./files/mcu.hotplug $(1)/etc/hotplug.d/usb/30-mcu
	$(INSTALL_DATA) ./files/mcu.sh $(1)/lib/functions
endef

$(eval $(call BuildPackage,mcu))
