include $(TOPDIR)/rules.mk

PKG_NAME:=indio
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/indio
        SECTION:=base
        CATEGORY:=Utilities
        TITLE:=indio
endef

define Build/Prepare
        mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef

Build/Compile = $(Build/Compile/Default)

define Package/indio/install
	cp -rf ./files/* $(1)
endef

$(eval $(call BuildPackage,indio))
