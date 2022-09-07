include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-client
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/Telecominfraproject/wlan-ucentral-client.git
PKG_MIRROR_HASH:=98b982e730df1a8683cb8c791a071d2147ee014b66313eecf260bf53443ea8db
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-06-22
PKG_SOURCE_VERSION:=f5e5a615dcc75395f1a9643a71717b1c7c437f20

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

define Package/ucentral-client/conffiles
/etc/ucentral/
/etc/config-shadow/
endef

define Package/ucentral-client/install
	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ucentral $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ucentral-client))
