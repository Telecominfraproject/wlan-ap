#
# OpenWrt package Makefile for ble-scan
#
# Standalone BLE scanner — communicates with ble-provisiond via ubus.
# Links against libble-client (NOT libble).
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ble-scan
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/ble-scan
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=BLE Scanner (daemon client)
  DEPENDS:=+libble-client
endef

define Package/ble-scan/description
  Standalone BLE scanner application. Communicates with ble-provisiond
  via ubus through libble-client. Multiple instances can run simultaneously.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/ble-scan \
		$(PKG_BUILD_DIR)/main.c \
		-lble-client -lubus -lubox -lblobmsg_json
endef

define Package/ble-scan/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble-scan $(1)/usr/bin/
endef

$(eval $(call BuildPackage,ble-scan))
