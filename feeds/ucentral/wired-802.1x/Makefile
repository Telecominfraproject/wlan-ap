include $(TOPDIR)/rules.mk

PKG_NAME:=wired-802.1x
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

PKG_SOURCE_URL=https://github.com/blogic/wired-802.1x.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-12
PKG_SOURCE_VERSION:=2f394a8416d8bed2298efe9737b86bcebe69744f

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/wired-802.1x
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Wired 802.1x
  DEPENDS:=+libubox +libubus +libuci
endef

define Package/wired-802.1x/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wired-802.1x $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,wired-802.1x))
