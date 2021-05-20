# SPDX-License-Identifier: BSD-3-Clause

include $(TOPDIR)/rules.mk

PKG_NAME:=apc
PKG_RELEASE:=1.0.0

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/apc
  SECTION:=base
  DEPENDS:=+libev +libinterapcomm +libubus +libubox
  CATEGORY:=Base system
  TITLE:=Access Point Coordinator
endef

define Package/apc/description
 Access Point Coordinator.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	[ ! -d ./src/ ] || $(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/apc $(1)/usr/bin
endef

define Package/apc/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/apc $(1)/usr/bin
	chmod 0700 $(1)/usr/bin/apc
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/apc.init $(1)/etc/init.d/apc
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/apc.config $(1)/etc/config/apc

endef
$(eval $(call BuildPackage,apc))
