#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=jitterentropy-library
PKG_RELEASE:=3.4.1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/smuellerDD/jitterentropy-library.git
PKG_SOURCE_VERSION:=eccabf2318c902285a8e3fbe7c2a7f84ab2e55ce
PKG_MIRROR_HASH:=08ccc536b7d291ce9715b746046b351710057a56c017b8c70a487f24c53392ba

include $(INCLUDE_DIR)/package.mk

define Package/jitterrng
  SECTION:=Utilities
  CATEGORY:=Utilities
  TITLE:=JITTER
  SUBMENU:=FIPS
endef

define Package/jitterrng/description
  Jitterrng provide a noise source using CPU execution timing jitter.
  In this package, we need getrawentropy binary to get noise source.
endef

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/bin
	$(TARGET_CROSS)gcc \
	$(PKG_BUILD_DIR)/tests/raw-entropy/recording_runtime_kernelspace/getrawentropy.c \
	-o $(PKG_BUILD_DIR)/bin/getrawentropy
endef

define Package/jitterrng/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/bin/* $(1)/usr/bin
endef

$(eval $(call BuildPackage,jitterrng))
