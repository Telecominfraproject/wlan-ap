include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-sshguard
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-sshguard
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=uCentral SSH brute-force guard
  DEPENDS:=+ucode +ucode-mod-fs +ucode-mod-log +ucode-mod-ubus +ucode-mod-uci +ucode-mod-uloop +firewall4
endef

define Package/ucentral-sshguard/description
 Blocks a source address on the SSH port for a while after repeated
 authentication failures. Failures are reported by dropbear as
 "dropbear.auth" ubus events (dropbear patch 920); no log parsing.
endef

define Package/ucentral-sshguard/conffiles
/etc/config/sshguard
endef

define Build/Compile

endef

define Package/ucentral-sshguard/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/ucentral-sshguard $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-sshguard.init $(1)/etc/init.d/ucentral-sshguard
	$(INSTALL_DATA) ./files/ucentral-sshguard.config $(1)/etc/config/sshguard
	$(INSTALL_BIN) ./files/ucentral-sshguard.defaults $(1)/etc/uci-defaults/90-ucentral-sshguard
endef

$(eval $(call BuildPackage,ucentral-sshguard))
