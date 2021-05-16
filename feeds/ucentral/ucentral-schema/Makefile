include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-schema
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-schema.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-02-15
PKG_SOURCE_VERSION:=c64a0eee031d24a458b2b6b112a3711b2634feac

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-schema
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral schema
endef

define Build/Compile

endef

define Package/ucentral-schema/install
	$(INSTALL_DIR) $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/schemareader.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/renderer/* $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/command/*.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/system/*.uc $(1)/usr/share/ucentral
endef

$(eval $(call BuildPackage,ucentral-schema))
