include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-client
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/Telecominfraproject/wlan-ucentral-client.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-06-22
PKG_SOURCE_VERSION:=8f5ab3f8fe87cad5c3a6c56463294c946459c725

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
