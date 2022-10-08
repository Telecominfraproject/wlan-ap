include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-wifi
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-wifi.git
PKG_MIRROR_HASH:=3af1982530bf95be32826a8ef5158df573ead05818fb1c81f3644c61d10ce017
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-01-10
PKG_SOURCE_VERSION:=d55f2d5b4527adcdeaf521e25bbef2f3765bb920

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ucentral-wifi
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt wifi status daemon
  DEPENDS:=+libubox +libubus +libnl-tiny +kmod-mac80211
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/libnl-tiny

define Build/Prepare
	$(Build/Prepare/Default)
	$(CP) $(STAGING_DIR)/usr/include/mac80211/uapi/linux/nl80211.h $(PKG_BUILD_DIR)/nl80211_copy.h
endef

define Package/ucentral-wifi/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ucentral-wifi $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-wifi $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,ucentral-wifi))
