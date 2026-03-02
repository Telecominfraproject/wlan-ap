include $(TOPDIR)/rules.mk

PKG_NAME:=ucrun
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/ucentral-io/ucrun.git
PKG_MIRROR_HASH:=6e9e77fcb6720febd9bcffb00c1822fc2ea18dda809e99fd2dd675d727f89132
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
