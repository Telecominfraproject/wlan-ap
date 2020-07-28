include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-ap-keys
PKG_RELEASE:=1

PKG_CONFIG_DEPENDS:=CONFIG_TIP_KEY_DEFAULT CONFIG_TIP_KEY_LOCAL
include $(INCLUDE_DIR)/package.mk

define Package/wlan-ap-keys
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=wlan-ap keys
endef

define Package/wlan-ap-keys/description
 The keys required by a wlan-ap to connect to the cloud
endef

define Package/wlan-ap-keys/config
menu "Cloud Key"
choice
	prompt "Keys installed in image"
	depends on PACKAGE_wlan-ap-keys 
	default TIP_KEY_DEFAULT

	config TIP_KEY_DEFAULT
	bool "Default"

	config TIP_KEY_LOCAL
	bool "Local Developer"

endchoice
endmenu
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/wlan-ap-keys/install
	$(INSTALL_DIR) $(1)/usr/opensync/certs/
ifeq ($(CONFIG_TIP_KEY_DEFAULT),y)
	$(INSTALL_DATA) ./files/* $(1)/usr/opensync/certs/
endif
ifeq ($(CONFIG_TIP_KEY_LOCAL),y)
	$(INSTALL_DATA) $(TOPDIR)/../keys/cacert.pem $(1)/usr/opensync/certs/ca.pem
	$(INSTALL_DATA) $(TOPDIR)/../keys/clientcert.pem $(1)/usr/opensync/certs/client.pem
	$(INSTALL_DATA) $(TOPDIR)/../keys/clientkey_dec.pem $(1)/usr/opensync/certs/client_dec.key
endif
endef

$(eval $(call BuildPackage,wlan-ap-keys))
