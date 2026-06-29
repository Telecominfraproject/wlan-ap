include $(TOPDIR)/rules.mk

PKG_NAME:=ucrun
PKG_VERSION:=2022.02.19~5be6abeb
PKG_RELEASE:=1

CMAKE_INSTALL:=1

PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=GPL

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(Build/Patch)
endef

define Package/ucrun
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libubox +ucode +ucode-mod-uci +ucode-mod-ubus +ucode-mod-fs
  TITLE:=uCode main-loop daemon
endef

define Package/ucrun/install
	$(INSTALL_DIR) $(1)/usr/bin

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/ucrun $(1)/usr/bin
endef

$(eval $(call BuildPackage,ucrun))
