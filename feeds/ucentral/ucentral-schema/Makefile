include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-schema
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-schema.git
PKG_MIRROR_HASH:=14988b3755677d637b7aa22b62d2070c4882d3fff17cb5b3d24c428e27856aaa
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-01-17
PKG_SOURCE_VERSION:=fb537b90f75b92c7b965e2eca1b611907694a48f

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-schema
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral schema
  DEPENDS+= +ucode-mod-nl80211 +ucode-mod-rtnl
endef

define Package/ucentral-recovery
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral recovery tooling
endef

define Build/Compile

endef

define Package/ucentral-schema/install
	$(INSTALL_DIR) $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/schemareader.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/renderer/* $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/command/*.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/system/*.uc $(1)/usr/share/ucentral
	$(CP) ./files/* $(1)
endef

define Package/ucentral-recovery/install
	$(INSTALL_DIR) $(1)/usr/share/ucentral $(1)/etc/uci-defaults/
	$(CP) $(PKG_BUILD_DIR)/command/*.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/system/{capabilities,crashlog}.uc $(1)/usr/share/ucentral
	$(CP) ./files/etc/uci-defaults/99-ucentral-hostname $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,ucentral-recovery))
$(eval $(call BuildPackage,ucentral-schema))
