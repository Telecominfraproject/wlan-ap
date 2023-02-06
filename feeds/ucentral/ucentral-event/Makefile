include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-event
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-event
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=uCentral event gathering daemon
endef

define Build/Compile

endef

define Package/ucentral-event/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) ./files/ucentral-event $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-wifiscan $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-event.init $(1)/etc/init.d/ucentral-event
	$(INSTALL_BIN) ./files/ucentral-wifiscan.init $(1)/etc/init.d/ucentral-wifiscan
	$(INSTALL_DATA) ./files/event $(1)/etc/config/
	$(INSTALL_DATA) ./files/events.json $(1)/etc/
endef

$(eval $(call BuildPackage,ucentral-event))
