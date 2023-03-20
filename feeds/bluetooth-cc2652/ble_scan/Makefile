include $(TOPDIR)/rules.mk

PKG_NAME:=ble_scan
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/ble_scan
	SECTION:=base
	CATEGORY:=Utilities
	TITLE:=ble_scan
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/ble_scan/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ble_scan $(1)/bin/
endef

define Package/ble_scan/extra_provides
    echo "libc.so.6";
endef

$(eval $(call BuildPackage,ble_scan))
