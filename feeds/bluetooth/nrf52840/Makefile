include $(TOPDIR)/rules.mk

PKG_NAME:=nrf52840
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/nrf52840
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=TIP NRF52840 support package
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/nrf52840/install
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,nrf52840))
