include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-schema
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-schema.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-02-15
PKG_SOURCE_VERSION:=efefa5cd8af1329f8c78f7310e1183ac31ba0d2a

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
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ucentral.schema.json $(1)/usr/share/ucentral
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/sys/*.uc $(1)/usr/share/ucentral
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/cmd/*.uc $(1)/usr/share/ucentral
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/cfg/*.uc $(1)/usr/share/ucentral
endef

$(eval $(call BuildPackage,ucentral-schema))
