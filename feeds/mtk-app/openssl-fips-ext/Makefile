#
# Copyright (C) 2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=openssl-fips-ext
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/openssl-fips-ext
	CATEGORY:=Utilities
	SECTION:=Utilities
	SUBMENU:=FIPS
	TITLE:=openssl-fips-ext -- simple tool for gcm and ccm based on openssl api
	DEPENDS:= +libopenssl +libgnutls +kmod-cryptodev
endef

define Package/openssl-fips-ext/description
	Simple tool for gcm and ccm
endef

define Build/Compile
	make -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		OPENSSL_INCS_LOCATION=-I$(STAGING_DIR)/usr/include/openssl \
		OPENSSL_LIBS_LOCATION=-L$(STAGING_DIR)/usr/lib \
		CRYPTODEV_INCS=-I$(KERNEL_BUILD_DIR)/cryptodev-linux-cryptodev-linux-1.12
endef

define Package/openssl-fips-ext/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/openssl-fips-ext $(1)/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hmac_sha $(1)/bin/
endef

$(eval $(call BuildPackage,openssl-fips-ext))
