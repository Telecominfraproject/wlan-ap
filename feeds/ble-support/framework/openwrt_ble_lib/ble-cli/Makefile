#
# OpenWrt package Makefile for ble-cli
#
# Standalone BLE command-line tool — depends on libble.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ble-cli
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/ble-cli
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=BLE Command-Line Tool
  DEPENDS:=+libble
endef

define Package/ble-cli/description
  Command-line tool for BLE operations (scan, beacon, GATT).
  Links against libble.so — no daemon required.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/ble-cli \
		$(PKG_BUILD_DIR)/ble_cli.c \
		-lble
endef

define Package/ble-cli/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble-cli $(1)/usr/bin/
endef

$(eval $(call BuildPackage,ble-cli))
