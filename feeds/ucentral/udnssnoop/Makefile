include $(TOPDIR)/rules.mk

PKG_NAME:=udnssnoop
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

PKG_SOURCE_URL=https://github.com/blogic/udnssnoop.git
PKG_MIRROR_HASH:=afd17cc6aed4a151bc0f437b84491d751932a39f93f429418200e9e8be53dfad
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-12
PKG_SOURCE_VERSION:=67e1e5f0bfc12222aa59c54e7066b1c00a680e56

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/udnssnoop
  SECTION:=net
  CATEGORY:=Network
  TITLE:=DNS Snooping Daemon
  DEPENDS:=+libubox +libubus
endef

define Package/udnssnoop/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/udnssnoop $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,udnssnoop))
