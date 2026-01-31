include $(TOPDIR)/rules.mk

PKG_NAME:=cig-quectel-tool
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/cig-quectel-tool
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=cig-quectel-tool
  DEPENDS:= +libpthread +librt 
endef

define Package/cig-quectel-tool/description
  CIG 5G cellular tool
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		 CFLAGS="$(TARGET_CFLAGS) -Wall" 	
endef

define Package/cig-quectel-tool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/lib/netifd/proto
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-CM $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-atc-proxy $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-mbim-proxy $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-qmi-proxy $(1)/usr/bin/
	$(INSTALL_BIN) ./files/qmi.sh $(1)/lib/netifd/proto/
	$(INSTALL_BIN) ./files/dhcp-qmi.script $(1)/lib/netifd/
endef

$(eval $(call BuildPackage,cig-quectel-tool))
