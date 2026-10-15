#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libkcapi
PKG_RELEASE:=1.4.0
PKG_VERSION:=1.4.0
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_FIXUP:=autoreconf

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/smuellerDD/libkcapi/tar.gz/refs/tags/v$(PKG_VERSION)
PKG_HASH:=10f96ccb2799f3ef810f3956c48a2d791a9221c9a545de51bd79e5a81030f38e

include $(INCLUDE_DIR)/package.mk
MAKE_FLAGS:=

define Package/libkcapi
  SECTION:=Utilities
  CATEGORY:=Utilities
  TITLE:=libkcapi
  SUBMENU:=FIPS
endef

define Package/libkcapi/description
  Libkcapi allow user-space to access the Linux kernel crypto API.
endef

define Build/Configure
	cd $(PKG_BUILD_DIR) && $(STAGING_DIR_HOST)/bin/autoreconf -i
	cd $(PKG_BUILD_DIR) && CC=aarch64-openwrt-linux-musl-gcc \
		CFLAGS="-Wno-error" $(PKG_BUILD_DIR)/configure \
		--enable-kcapi-test  --enable-kcapi-speed \
		--enable-kcapi-hasher --enable-kcapi-rngapp \
		--enable-kcapi-encapp --enable-kcapi-dgstapp \
		--host=x86
endef

define Package/libkcapi/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/bin/* $(1)/usr/bin
endef

$(eval $(call BuildPackage,libkcapi))
