#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=netifd-morse
PKG_RELEASE=1

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>

include $(INCLUDE_DIR)/package.mk

define Package/netifd-morse
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Morse Micro HaLow support for netifd
  DEPENDS:= netifd \
	+@BUSYBOX_CONFIG_USLEEP +@BUSYBOX_CONFIG_BASE64 +@BUSYBOX_CONFIG_XXD \
	+kmod-morse +kmod-cfg80211 \
	+morse-regdb +wpa_event_listener +wpa_supplicant_s1g +hostapd_s1g +morsecli
endef

define Build/Compile
endef

define Package/netifd-morse/install
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) ./lib/* $(1)/lib/
	$(INSTALL_BIN) ./etc/init.d/halow-gpio-reset $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,netifd-morse))
