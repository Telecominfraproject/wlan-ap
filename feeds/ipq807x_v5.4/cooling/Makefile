include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
#PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

PKG_NAME:=cooling
PKG_RELEASE:=1

COOLING_MAKE_OPTS:= \
        CROSS=$(TARGET_CROSS) \
        COOLINGDIR=$(PKG_BUILD_DIR) \

include $(INCLUDE_DIR)/package.mk


ifeq ($(CONFIG_TARGET_ipq50xx_generic_DEVICE_sonicfi_rap630c_311g),y)
export TARGET_CFLAGS += -DPLATFORM_RAP630C_311G=1
endif
ifeq ($(CONFIG_TARGET_ipq50xx_generic_DEVICE_sonicfi_rap630w_311g),y)
export TARGET_CFLAGS += -DPLATFORM_RAP630W_311G=1
endif

define Package/cooling
  SECTION:=utils
  CATEGORY:=Utilities
  MAINTAINER:=Sonicfi
  DEPENDS:=+libubox +libubus +libuci @TARGET_ipq50xx
  TITLE:=Wifi Thermal Mitigation daemon for QCA platform
endef

define Package/cooling/description
  This package is wifi cooling mitigation daemon.
endef

define Build/Compile
	LDFLAGS="$(TARGET_LDFLAGS)" \
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(COOLING_MAKE_OPTS))
endef

define Package/cooling/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cooling $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/cooling
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sonicfi-rap630c-311g-cooling.conf $(1)/etc/cooling
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sonicfi-rap630w-311g-cooling.conf $(1)/etc/cooling
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/cooling.init $(1)/etc/init.d/cooling
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/cooling.config $(1)/etc/config/cooling
endef

$(eval $(call BuildPackage,cooling))
