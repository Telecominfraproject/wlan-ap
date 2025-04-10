#
# Copyright 2023 Morse Micro
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wpa_event_listener
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/wpa_event_listener
  SECTION:=utils
  CATEGORY:=Base system
  TITLE:= WPA supplicant s1g event listener
  MAINTAINER:=Morse Micro
  DEPENDS:= wpa_supplicant_s1g +libwpa_client
endef

define Package/wpa_event_listener/description
 A Daemon to listen to wpa supplicant s1g events.
endef

define Package/wpa_event_listener/install
	$(INSTALL_DIR) $(1)/usr/sbin

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_event_listener 	$(1)/usr/sbin
endef

$(eval $(call BuildPackage,wpa_event_listener))