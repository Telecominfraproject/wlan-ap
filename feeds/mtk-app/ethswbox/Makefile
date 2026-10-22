# SPDX-License-Identifier: GPL-2.0-or-later
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ethswbox
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/package-defaults.mk

define Package/ethswbox
  TITLE:=Mxl series Ethernet Switch Configuration Utility
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Applications
endef

define Package/ethswbox/description
  This package provides utility to control Mxl series switches
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/ethswbox/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ethswbox $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,ethswbox))
