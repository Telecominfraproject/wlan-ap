include $(TOPDIR)/rules.mk

PKG_NAME:=cc2652r1
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/cc2652r1
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=cc2652r1 -- TI bluetooth firmware upgrade tool 
	DEPENDS:=+libstdcpp +zlib 
endef

define Package/cc2652r1/description
	cc2652r1 ncp for ti hci uart
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/cc2652r1/install
	$(INSTALL_DIR) $(1)/etc/ble_fw/ti
	$(INSTALL_BIN) ./files/firmware/ble5_multi_role_sdk8_33_bd9.bin $(1)/etc/ble_fw/ti
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/ble_chip.config $(1)/etc/config/ble_chip
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ble_chip.init $(1)/etc/init.d/ble_chip
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tisbl $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/*.sh $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,cc2652r1))

