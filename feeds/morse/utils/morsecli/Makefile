#
# Copyright 2023 Morse Micro
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morsecli
PKG_RELEASE=5

PKG_VERSION:=1.12.4

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

PKG_SOURCE_VERSION:=$(PKG_VERSION)
PKG_SOURCE_URL:=https://github.com/MorseMicro/morse_cli.git
PKG_HASH:=04c88a3aea99082a29eb1f84a9b62dcbb85980fa040e0e34901c1826f33ae74c
PKG_SOURCE_PROTO:=git

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/morsecli
  SECTION:=utils
  CATEGORY:=Network
  TITLE:=Morse Micro WIFI HaLow driver command line tool
  DEPENDS:= +kmod-morse +libnl
  FILES:=\
	$(PKG_BUILD_DIR)/morse_cli
  PROVIDES:=morsectrl
endef

MAKE_FLAGS += \
    MORSECTRL_VERSION_STRING=$(PKG_VERSION) \
    CONFIG_MORSE_TRANS_NL80211=1 \

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/libnl3

define Package/morsecli/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/morse_cli $(1)/sbin/
endef

$(eval $(call BuildPackage,morsecli))
