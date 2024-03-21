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
PKG_SOURCE_DATE:=2024-02-02
PKG_SOURCE_VERSION:=ed34b2816f2b65ee0687944a214fdf6ffd94b85f

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=Piotr Dymacz <pepe2k@gmail.com>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_INSTALL:=1

CMAKE_OPTIONS += -DTI_SERIAL_BL_SUPPORT=ON
CMAKE_OPTIONS += -DTI_SERIAL_BL_DUMPCMD=ON
CMAKE_OPTIONS += -DCONFIRM_TEST_SUPPORT=OFF

define Package/umcumgr
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libubox
  TITLE:=User space tool for MCUmgr SMP based management
endef

define Package/umcumgr/description
  Simple user space tool for MCU management over MCUmgr SMP protocol
  and Texas Instruments CC13x2/CC26x2 ROM serial bootloader
endef

define Package/umcumgr/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/umcumgr $(1)/usr/bin/
endef

$(eval $(call BuildPackage,umcumgr))
