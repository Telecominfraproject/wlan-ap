# SPDX-License-Identifier: BSD-3-Clause

include $(TOPDIR)/rules.mk

PKG_NAME:=libwebsocket
PKG_RELEASE:=1.0.0

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/libwebsocket
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=Websocket client library
  DEPENDS:=+libopenssl
endef

define Package/libwebsocket/description
 websocket library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	[ ! -d ./src/ ] || $(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/websocket
	$(CP) $(PKG_BUILD_DIR)/include/websocket/* $(1)/usr/include/websocket/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libwebsocket.so $(1)/usr/lib/
endef

define Package/libwebsocket/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libwebsocket.so $(1)/usr/lib/
endef
$(eval $(call BuildPackage,libwebsocket))
