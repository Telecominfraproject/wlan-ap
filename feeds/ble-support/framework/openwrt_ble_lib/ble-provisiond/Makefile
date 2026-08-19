#
# OpenWrt package Makefile for ble-provisiond
#
# BLE provision daemon — depends on libble for all BLE operations.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ble-provisiond
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/ble-provisiond
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=BLE Provision Daemon
  DEPENDS:=+libble +libubus +libubox +libblobmsg-json +libuci +libdbus
endef

define Package/ble-provisiond/description
  BLE provision daemon for OpenWrt. Uses libble for all BLE operations,
  exposes ubus API, reads UCI configuration, and runs application plugins
  (iBeacon, Scanner, GATT client).
endef

define Package/ble-provisiond/conffiles
/etc/config/ble
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/dbus-1.0 \
		-I$(STAGING_DIR)/usr/lib/dbus-1.0/include \
		-o $(PKG_BUILD_DIR)/ble-provisiond \
		$(PKG_BUILD_DIR)/main.c \
		$(PKG_BUILD_DIR)/ubus_api.c \
		$(PKG_BUILD_DIR)/uci_config.c \
		$(PKG_BUILD_DIR)/app/ibeacon_app.c \
		$(PKG_BUILD_DIR)/app/scan_app.c \
		$(PKG_BUILD_DIR)/app/gatt_app.c \
		$(PKG_BUILD_DIR)/app/gatt_server_app.c \
		$(PKG_BUILD_DIR)/app/ble_advertise.c \
		$(PKG_BUILD_DIR)/app/scan_filter.c \
		$(PKG_BUILD_DIR)/app/scan_file.c \
		$(PKG_BUILD_DIR)/app/config_provision.c \
		$(PKG_BUILD_DIR)/app/mtu_segment.c \
		$(PKG_BUILD_DIR)/app/npi_gatt_handler.c \
		-lble -lubus -lubox -lblobmsg_json -luci -ldbus-1 -ljson-c
endef

define Package/ble-provisiond/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble-provisiond $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble-provision.init $(1)/etc/init.d/ble-provision

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/ble.uci $(1)/etc/config/ble
endef

$(eval $(call BuildPackage,ble-provisiond))
