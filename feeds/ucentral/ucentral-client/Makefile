include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-client
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/Telecominfraproject/wlan-ucentral-client.git
PKG_MIRROR_HASH:=49d19d1cddb538f233a7c1d588b5c765929a8fe28a1524d0455c4b516dcd0310
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-06-22
PKG_SOURCE_VERSION:=3bbcd36c6111e08df6c8f13e0079826ec417cc85

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ucentral-client
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral websocket client
  DEPENDS:=+ucode +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci +ucode-mod-math +ucode-mod-resolv +ucode-mod-uloop \
	   +libubox +libwebsockets-openssl +libblobmsg-json +libubus
endef

define Package/ucentral-client/install
	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ucentral $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ucentral-client))
