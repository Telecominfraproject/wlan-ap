include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-ap-keys
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/wlan-ap-keys
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=wlan-ap keys
endef

define Package/wlan-ap-keys/description
 The keys required by a wlan-ap to connect to the cloud
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/wlan-ap-keys/install
	$(INSTALL_DIR) $(1)/usr/opensync/certs/
	$(INSTALL_DATA) $(TOPDIR)/../keys/cacert.pem $(1)/usr/opensync/certs/ca.pem
	$(INSTALL_DATA) $(TOPDIR)/../keys/clientcert.pem $(1)/usr/opensync/certs/client.pem
	$(INSTALL_DATA) $(TOPDIR)/../keys/clientkey_dec.pem $(1)/usr/opensync/certs/client_dec.key
endef

$(eval $(call BuildPackage,wlan-ap-keys))
