include $(TOPDIR)/rules.mk

PKG_NAME:=cc2652
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/cc2652
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=cc2652 -- TI CC26x2 firmware upgrade tool 
	DEPENDS:=+libstdcpp +zlib 
endef

define Package/cc2652/description
	TI CC26x2 firmware upgrade tool 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/cc2652/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/tisbl.init $(1)/etc/init.d/tisbl

	$(INSTALL_DIR) $(1)/etc/tifirmware
	$(INSTALL_BIN) ./files/firmware/* $(1)/etc/tifirmware/
	
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tisbl $(1)/usr/bin/
	$(INSTALL_BIN) ./files/*.sh $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/tisbl.config $(1)/etc/config/tisbl

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/tisbl.defaults $(1)/etc/uci-defaults
endef

$(eval $(call BuildPackage,cc2652))

