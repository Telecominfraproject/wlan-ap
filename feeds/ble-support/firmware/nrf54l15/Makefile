include $(TOPDIR)/rules.mk

PKG_NAME:=nrf54l15
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/nrf54l15
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=nrf54l15 -- Nordic bluetooth firmware upgrade tool
endef

define Package/nrf54l15/description
	nrf54l15 rcp for hci uart
endef

define Build/Compile
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Package/nrf54l15/install
	$(INSTALL_DIR) $(1)/etc/ble_fw/nordic
	$(INSTALL_BIN) ./files/firmware/* $(1)/etc/ble_fw/nordic
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip
	$(INSTALL_DATA) ./files/ble.config $(1)/etc/config/ble
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble_chip.init $(1)/etc/init.d/ble_chip
	$(INSTALL_BIN) ./files/ble.init $(1)/etc/init.d/ble
#	$(INSTALL_BIN) ./files/wifi_provision.init $(1)/etc/init.d/wifi_provision
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/mcumgr $(1)/usr/sbin
	$(INSTALL_BIN) ./files/burning_hci_uart_image $(1)/usr/sbin
	$(INSTALL_BIN) ./files/ibeacon_start.sh $(1)/usr/sbin
#	$(INSTALL_BIN) ./files/wifi_provision.py $(1)/usr/sbin
#	$(INSTALL_BIN) ./files/enable_standalone.sh $(1)/usr/sbin
#	$(INSTALL_BIN) ./files/disable_standalone.sh $(1)/usr/sbin
endef

$(eval $(call BuildPackage,nrf54l15))
