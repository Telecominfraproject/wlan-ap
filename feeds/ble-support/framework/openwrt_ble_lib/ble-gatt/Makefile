#
# OpenWrt package Makefile for ble-gatt
#
# Standalone BLE GATT client application.
# Communicates with ble-provisiond via ubus. Supports interactive subscribe mode.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ble-gatt
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/ble-gatt
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=BLE GATT Client (daemon client)
  DEPENDS:=+libble-client
endef

define Package/ble-gatt/description
  Standalone BLE GATT client. Communicates with ble-provisiond via ubus
  through libble-client. Supports connect, read, write, subscribe
  (notification listener), and disconnect operations.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/ble-gatt \
		$(PKG_BUILD_DIR)/main.c \
		-lble-client -lubus -lubox -lblobmsg_json
endef

define Package/ble-gatt/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble-gatt $(1)/usr/bin/
endef

$(eval $(call BuildPackage,ble-gatt))
