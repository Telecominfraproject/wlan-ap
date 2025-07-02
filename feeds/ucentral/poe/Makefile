include $(TOPDIR)/rules.mk

PKG_NAME:=poe
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq50xx_generic_DEVICE_sonicfi_rap630w_311g),y)
TARGET_CFLAGS += -DPLATFORM_EWW631_B1=1
endif

define Package/poe
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Turn on/off PoE ports with TSP23861 chipset
	DEPENDS:= +libubox +libubus +libuci +libi2c
endef

define Package/poe/description
  Turn on/off PoE ports
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef


define Package/poe/install
	$(INSTALL_DIR) $(1)
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d
	$(INSTALL_BIN) ./files/poe.init $(1)/etc/init.d/poe
	$(INSTALL_BIN) ./files/poe.config $(1)/etc/config/poe

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tps23861-poe-ctrl $(1)/usr/bin
endef

$(eval $(call BuildPackage,poe))
