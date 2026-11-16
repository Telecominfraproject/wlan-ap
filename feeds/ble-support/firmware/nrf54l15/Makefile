include $(TOPDIR)/rules.mk

PKG_NAME:=nrf54l15
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/target.mk

define Package/nrf54l15
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=nrf54l15 -- Nordic bluetooth firmware upgrade tool
	DEPENDS:=+socat
endef

define Package/nrf54l15/description
	nrf54l15 rcp for hci uart
endef

define Build/Compile
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

BOARD_NAME:=$(call qstrip,$(subst DEVICE_,,$(PROFILE)))

define Package/nrf54l15/install
	$(INSTALL_DIR) $(1)/etc/ble_fw/nordic
	$(INSTALL_BIN) ./files/firmware/* $(1)/etc/ble_fw/nordic
	$(INSTALL_DIR) $(1)/etc/config
	if [ -f "./files/$(BOARD_NAME).config" ]; then \
		$(INSTALL_DATA) ./files/$(BOARD_NAME).config $(1)/etc/config/ble_chip; \
	else \
		$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip; \
	fi
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble_chip.init $(1)/etc/init.d/ble_chip
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/mcumgr $(1)/usr/sbin
	$(INSTALL_BIN) ./files/burning_hci_uart_image $(1)/usr/sbin
endef

$(eval $(call BuildPackage,nrf54l15))
