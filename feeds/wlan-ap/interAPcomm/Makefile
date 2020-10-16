# SPDX-License-Identifier: BSD-3-Clause

include $(TOPDIR)/rules.mk

PKG_NAME:=libinterapcomm
PKG_RELEASE:=1.0.0

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/libinterapcomm
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Inter AP communication library
endef

define Package/libinterapcomm/description
 Inter AP communication library.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/interap
	$(CP) $(PKG_BUILD_DIR)/include/* $(1)/usr/include/interap/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libinterapcomm.so $(1)/usr/lib/
endef

define Package/libinterapcomm/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libinterapcomm.so $(1)/usr/lib/
endef
$(eval $(call BuildPackage,libinterapcomm))
