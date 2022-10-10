include $(TOPDIR)/rules.mk

PKG_NAME:=ucrun
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/ucentral-io/ucrun.git
PKG_MIRROR_HASH:=52aeece27348611197ae5f4b96b3bdf1b5d028ae4ae284806b216d502300d07a
PKG_SOURCE_DATE:=2022-02-19
PKG_SOURCE_VERSION:=5be6abebc4ae6057b47a5b3f0799d5ff01bc60c3
CMAKE_INSTALL:=1

PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=GPL

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

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
