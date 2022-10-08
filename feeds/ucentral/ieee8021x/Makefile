include $(TOPDIR)/rules.mk

PKG_NAME:=ieee8021x
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

PKG_SOURCE_URL=https://github.com/blogic/ieee8021x.git
PKG_MIRROR_HASH:=5a47c355366fb7cfaa0d622c4ea1fa6a867b0fb30e624343727d5f8722c79f7b
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-12
PKG_SOURCE_VERSION:=c526967cbc14ba5528c817d5f6115156c2da94b9

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ieee8021x
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Wired 802.1x
  DEPENDS:=+libubox +libubus +libuci
endef

define Package/ieee8021x/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ieee8021x $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ieee8021x))
