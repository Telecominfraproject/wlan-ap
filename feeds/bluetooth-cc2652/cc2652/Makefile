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
	$(INSTALL_DIR) $(1)/lib/firmware/cc2562
	$(INSTALL_BIN) ./files/firmware/* $(1)/lib/firmware/cc2562
	
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tisbl $(1)/usr/bin/
	$(INSTALL_BIN) ./files/*.sh $(1)/usr/bin/
endef

$(eval $(call BuildPackage,cc2652))

