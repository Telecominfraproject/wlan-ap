include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-tools
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ucentral-tools
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=ucentral tools collection
  DEPENDS:=+libubox +libcurl +libradcli +libopenssl
endef

define Package/ucentral-tools/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/{dhcpdiscover,dnsprobe,radiusprobe,ip-collide} $(1)/usr/sbin/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ucentral-tools))
