include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-copp
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-copp
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=uCentral CoPP Service
endef

define Build/Compile

endef

define Package/ucentral-copp/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/hotplug.d/net $(1)/etc/config
	$(INSTALL_BIN) ./files/copp.init $(1)/etc/init.d/copp
	$(INSTALL_BIN) ./files/copp.hotplug $(1)/etc/hotplug.d/net/30-copp
	$(INSTALL_BIN) ./files/copp_template.nft $(1)/etc/
	$(INSTALL_BIN) ./files/copp.config $(1)/etc/config/copp
endef

$(eval $(call BuildPackage,ucentral-copp))