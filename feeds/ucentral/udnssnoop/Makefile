include $(TOPDIR)/rules.mk

PKG_NAME:=udnssnoop
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

PKG_SOURCE_URL=https://github.com/blogic/udnssnoop.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-12
PKG_SOURCE_VERSION:=852a5246af43d321905979fc850e654718a56061

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
