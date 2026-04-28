include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-schema
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/Telecominfraproject/wlan-ucentral-schema.git
PKG_MIRROR_HASH:=aa3b2c09dec67cbd12289d870b5d6d5ec21b36931d636369c98bcf02fa344521
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2026-04-28
PKG_SOURCE_VERSION:=4d4d45951a24161e44aff8596a9b1c3a1a068654
PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-schema
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral schema
  DEPENDS+= +ucode-mod-nl80211 +ucode-mod-rtnl +ucode-mod-log
endef

define Package/ucentral-recovery
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt uCentral recovery tooling
endef

define Build/Compile

endef

define Package/ucentral-schema/install
	$(INSTALL_DIR) $(1)/usr/share/ucentral $(1)/etc/ucentral/ $(1)/usr/bin $(1)/usr/share/ucode
	$(CP) $(PKG_BUILD_DIR)/schema.json $(1)/etc/ucentral/
	$(CP) $(PKG_BUILD_DIR)/schemareader.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/renderer/* $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/command/*.uc $(1)/usr/share/ucentral
	$(CP) $(PKG_BUILD_DIR)/system/* $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/wifi_detect.sh $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/capabilities.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/health.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/ip-collide.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/onlinecheck.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/package_list.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/reboot_cause.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/state.uc $(1)/usr/share/ucentral
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/system/telemetry.uc $(1)/usr/share/ucentral
	$(LN) /usr/share/ucentral/schemareader.uc $(1)/usr/share/ucode
	$(LN) /usr/share/ucentral/ucentral.uc $(1)/usr/bin/ucentral_apply
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
