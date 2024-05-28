include $(TOPDIR)/rules.mk

PKG_NAME:=hfcl
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/hfcl
        SECTION:=base
        CATEGORY:=Utilities
        TITLE:=hfcl
endef

define Build/Prepare
        mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef

Build/Compile = $(Build/Compile/Default)

define Package/hfcl/install
	cp -rf ./files/* $(1)
endef

$(eval $(call BuildPackage,hfcl))
