include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-state
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-state
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=uCentral state monitoring daemon
endef

define Build/Compile

endef

define Package/ucentral-state/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) ./files/ucentral-state $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-pstore $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-state.init $(1)/etc/init.d/ucentral-state
	$(INSTALL_BIN) ./files/ucentral-state.config $(1)/etc/config/state
endef

$(eval $(call BuildPackage,ucentral-state))
