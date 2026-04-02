#
# Copyright (C) 2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=rap630w-312g_boot_img
PKG_VERSION:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=boot
  CATEGORY:=Boot Loaders
  TITLE:=IPQxxx U-Boot image
  DEPENDS:=
endef

define Package/$(PKG_NAME)/description
U-Boot image install /usr/share/boot_img
endef

define Build/Prepare
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/share/boot_img
	$(CP) ./files/openwrt-ipq5018-u-boot.tar.gz $(1)/usr/share/boot_img/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
