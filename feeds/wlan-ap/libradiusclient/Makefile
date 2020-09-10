# SPDX-License-Identifier: BSD-3-Clause

include $(TOPDIR)/rules.mk

PKG_NAME:=libradiusclient
PKG_RELEASE:=1.0.0

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/libradiusclient
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=Radius client library
endef

define Package/libradiusclient/description
 Radius client library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	[ ! -d ./src/ ] || $(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/radius
	$(CP) $(PKG_BUILD_DIR)/include/radius/* $(1)/usr/include/radius/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libradiusclient.so $(1)/usr/lib/
endef

define Package/libradiusclient/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libradiusclient.so $(1)/usr/lib/
endef
$(eval $(call BuildPackage,libradiusclient))
