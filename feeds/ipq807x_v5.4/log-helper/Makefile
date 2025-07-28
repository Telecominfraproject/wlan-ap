include $(TOPDIR)/rules.mk

PKG_NAME:=log-helper
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/log-helper
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Log backup helper
  DEPENDS:=+libuci +libubus +libubox
endef

define Package/log-helper/description
  This service that backs up files on boot.
endef

define Package/log-helper/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/log-helper.uci $(1)/etc/config/log-helper

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/log-helper.init $(1)/etc/init.d/log-helper

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/log-helper $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,log-helper))
