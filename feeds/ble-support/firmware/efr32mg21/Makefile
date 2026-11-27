# SPDX-License-Identifier: BSD-3-Clause
include $(TOPDIR)/rules.mk

PKG_NAME:=efr32mg21
PKG_VERSION:=1.0
PKG_LICENSE:=BSD-3-Clause AND Zlib AND LicenseRef-Silabs-MSLA
PKG_LICENSE_FILES:=main.c sl_bt_api.h
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/efr32mg21
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=efr32mg21 -- Silicon Labs bluetooth firmware upgrade tool
	DEPENDS:=+kmod-hci-uart +socat
endef

define Package/efr32mg21/description
	Silicon Labs EFR32MG21 BLE host firmware (NCP / RCP / RCP+802.15.4)
	and the UART DFU tool used to flash it. The RCP firmware is attached
	to the Linux Bluetooth stack as hci0 via btattach.
endef

# The DFU tool is a plain host executable built from ./src with the OpenWrt
# target toolchain. ./src/Makefile already drives the compile; just hand it
# the cross CC/CFLAGS.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall -Wextra -std=gnu99"
endef

BOARD_NAME:=$(call qstrip,$(subst DEVICE_,,$(PROFILE)))

define Package/efr32mg21/install
	$(INSTALL_DIR) $(1)/etc/ble_fw/silabs
	$(INSTALL_BIN) ./files/firmware/* $(1)/etc/ble_fw/silabs
	$(INSTALL_DIR) $(1)/etc/config
	if [ -f "./files/$(BOARD_NAME).config" ]; then \
		$(INSTALL_DATA) ./files/$(BOARD_NAME).config $(1)/etc/config/ble_chip; \
	else \
		$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip; \
	fi
	$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble_chip.init $(1)/etc/init.d/ble_chip
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bt_host_uart_dfu $(1)/usr/sbin/sl_bt_host_uart_dfu
	$(INSTALL_BIN) ./files/bt_rcp_ver.sh $(1)/usr/sbin/bt_rcp_ver.sh
endef

$(eval $(call BuildPackage,efr32mg21))
