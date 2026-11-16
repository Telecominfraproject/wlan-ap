include $(TOPDIR)/rules.mk

PKG_NAME:=nrf52840
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/target.mk

define Package/nrf52840
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=nrf52840 -- Nordic bluetooth firmware upgrade tool
	DEPENDS:=+kmod-btusb +kmod-crypto-user +socat
endef

define Package/nrf52840/description
	nrf52840 rcp for hci uart
endef

define Build/Compile
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

BOARD_NAME:=$(call qstrip,$(subst DEVICE_,,$(PROFILE)))

define Package/nrf52840/install
	$(INSTALL_DIR) $(1)/etc/config
	if [ -f "./files/$(BOARD_NAME).config" ]; then \
		$(INSTALL_DATA) ./files/$(BOARD_NAME).config $(1)/etc/config/ble_chip; \
	else \
		$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip; \
	fi
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble_chip.init $(1)/etc/init.d/ble_chip
endef

$(eval $(call BuildPackage,nrf52840))
