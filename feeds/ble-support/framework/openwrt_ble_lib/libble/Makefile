#
# OpenWrt package Makefile for libble
#
# Builds libble as a shared library (.so) for BLE abstraction.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libble
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=MIT
PKG_MAINTAINER:=OpenWrt BLE Team

include $(INCLUDE_DIR)/package.mk

define Package/libble
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=BLE Abstraction Layer Shared Library
  DEPENDS:=+libubox +libdbus
  ABI_VERSION:=1
endef

define Package/libble/description
  Shared library providing unified BLE operations regardless of
  underlying transport (BlueZ D-Bus or UART HCI controllers).
  Supports multiple chip profiles: HCI H4, TI NPI, JSON-over-UART.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src $(PKG_BUILD_DIR)/
	$(CP) ./include $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=c99 -Wall -Wextra \
		-fPIC -fvisibility=hidden -DLIBBLE_BUILDING \
		-I$(PKG_BUILD_DIR)/include \
		-I$(PKG_BUILD_DIR)/src \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/dbus-1.0 \
		-I$(STAGING_DIR)/usr/lib/dbus-1.0/include \
		-shared -Wl,-soname,libble.so.1 \
		-o $(PKG_BUILD_DIR)/libble.so.$(PKG_VERSION) \
		$(PKG_BUILD_DIR)/src/ble_core.c \
		$(PKG_BUILD_DIR)/src/ble_status_file.c \
		$(PKG_BUILD_DIR)/src/radio_power.c \
		$(PKG_BUILD_DIR)/src/transport/bluez_transport.c \
		$(PKG_BUILD_DIR)/src/transport/uart_transport.c \
		$(PKG_BUILD_DIR)/src/chip_profile/chip_profile_hci_h4.c \
		$(PKG_BUILD_DIR)/src/chip_profile/chip_profile_ti_npi.c \
		$(PKG_BUILD_DIR)/src/chip_profile/chip_profile_json.c \
		-lubox -ldbus-1
	cd $(PKG_BUILD_DIR) && \
		ln -sf libble.so.$(PKG_VERSION) libble.so.1 && \
		ln -sf libble.so.1 libble.so
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/include/ble.h $(1)/usr/include/

	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libble.so* $(1)/usr/lib/
endef

define Package/libble/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libble.so.$(PKG_VERSION) $(1)/usr/lib/
	cd $(1)/usr/lib && \
		ln -sf libble.so.$(PKG_VERSION) libble.so.1 && \
		ln -sf libble.so.1 libble.so
endef

$(eval $(call BuildPackage,libble))
