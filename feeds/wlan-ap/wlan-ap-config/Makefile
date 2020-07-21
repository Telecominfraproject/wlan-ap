include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-ap-config
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/wlan-ap-config
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=wlan-ap configuration
endef

define Package/wlan-ap-config/description
 The default configuration of the AP.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/wlan-ap-config/install
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,wlan-ap-config))
