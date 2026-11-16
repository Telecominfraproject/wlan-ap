#
# OpenWrt package Makefile for ble-beacon
#
# Standalone BLE beacon (iBeacon) controller.
# Communicates with ble-provisiond via ubus. Fire-and-forget style.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ble-beacon
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/ble-beacon
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=BLE Beacon Controller (daemon client)
  DEPENDS:=+libble-client
endef

define Package/ble-beacon/description
  Standalone iBeacon controller. Communicates with ble-provisiond
  via ubus through libble-client. Fire-and-forget: daemon maintains
  beacon state after this process exits.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/ble-beacon \
		$(PKG_BUILD_DIR)/main.c \
		-lble-client -lubus -lubox -lblobmsg_json
endef

define Package/ble-beacon/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble-beacon $(1)/usr/bin/
endef

$(eval $(call BuildPackage,ble-beacon))
