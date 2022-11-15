#
# Copyright (C) 2023 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=umcumgr
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://gitlab.com/pepe2k/umcumgr
PKG_SOURCE_DATE:=2023-03-06
PKG_SOURCE_VERSION:=966606a2868b8e6b0a2c7e129dc45a30e0d9ef87

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=Piotr Dymacz <pepe2k@gmail.com>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_INSTALL:=1

define Package/umcumgr
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libubox
  TITLE:=User space tool for MCUmgr SMP based management
endef

define Package/umcumgr/description
  Simple user space tool for MCU management over MCUmgr SMP
  (Simple Management Protocol) protocol
endef

define Package/umcumgr/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/umcumgr $(1)/usr/bin/
endef

$(eval $(call BuildPackage,umcumgr))
