include $(TOPDIR)/rules.mk

PKG_NAME:=dynamic-vlan
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/dynamic-vlan.git
PKG_MIRROR_HASH:=2129d5e4b397afad76825a042dab6fb57c63e57c686d354f3d0d77a5754ab760
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-06-04
PKG_SOURCE_VERSION:=7202189d1b710c52f8ddc3c7040821708c3f438b

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/dynamic-vlan
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=dynamic VLAN netifd helper
  DEPENDS:=+libubox +libubus
endef

define Package/dynamic-vlan/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d 
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dynamic-vlan $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/dynamic-vlan $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,dynamic-vlan))
