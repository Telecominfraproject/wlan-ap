include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-ap-config
PKG_RELEASE:=1

PKG_CONFIG_DEPENDS:=CONFIG_WLAN_AP_CLOUD_IP
include $(INCLUDE_DIR)/package.mk

define Package/wlan-ap-config
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=wlan-ap configuration
endef

define Package/wlan-ap-config/description
 The default configuration of the AP.
endef

define Package/wlan-ap-config/config
menu "Cloud IP"
config WLAN_AP_CLOUD_IP
	string
	prompt "The default IP of the cloud"
endmenu
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/wlan-ap-config/install
	$(CP) ./files/* $(1)
	$(SED) "s/CLOUD_IP=/CLOUD_IP=$(CONFIG_WLAN_AP_CLOUD_IP)/g" $(1)/etc/uci-defaults/99-tip-cloud-ip
endef

$(eval $(call BuildPackage,wlan-ap-config))
