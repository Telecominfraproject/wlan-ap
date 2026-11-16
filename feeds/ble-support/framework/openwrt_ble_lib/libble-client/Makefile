#
# OpenWrt package Makefile for libble-client
#
# Thin ubus client library for communicating with ble-provisiond.
# Used by standalone BLE application binaries (ble-scan, ble-beacon, ble-gatt).
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libble-client
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/libble-client
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=BLE Client Library (ubus wrapper)
  DEPENDS:=+libubus +libubox +libblobmsg-json
  ABI_VERSION:=1
endef

define Package/libble-client/description
  High-level client library for BLE operations via ubus IPC.
  Communicates with ble-provisiond daemon — does NOT access hardware directly.
  Multiple processes can use this library simultaneously.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src $(PKG_BUILD_DIR)/
	$(CP) ./include $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-fPIC \
		-I$(PKG_BUILD_DIR)/include \
		-I$(STAGING_DIR)/usr/include \
		-shared -Wl,-soname,libble-client.so.1 \
		-o $(PKG_BUILD_DIR)/libble-client.so.$(PKG_VERSION) \
		$(PKG_BUILD_DIR)/src/ble_client.c \
		-lubus -lubox -lblobmsg_json
	cd $(PKG_BUILD_DIR) && \
		ln -sf libble-client.so.$(PKG_VERSION) libble-client.so.1 && \
		ln -sf libble-client.so.1 libble-client.so
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/include/ble_client.h $(1)/usr/include/

	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libble-client.so* $(1)/usr/lib/
endef

define Package/libble-client/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libble-client.so.$(PKG_VERSION) $(1)/usr/lib/
	cd $(1)/usr/lib && \
		ln -sf libble-client.so.$(PKG_VERSION) libble-client.so.1 && \
		ln -sf libble-client.so.1 libble-client.so
endef

$(eval $(call BuildPackage,libble-client))
