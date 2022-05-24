include $(TOPDIR)/rules.mk

PKG_NAME:=radius-gw-proxy
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/radius-gw-proxy
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=uCentral Gateway radius-gw-proxy
  DEPENDS:=+libubox +libubus
endef

define Package/radius-gw-proxy/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/radius-gw-proxy $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,radius-gw-proxy))
