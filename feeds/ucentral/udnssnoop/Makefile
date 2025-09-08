include $(TOPDIR)/rules.mk

PKG_NAME:=udnssnoop
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

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
