#
# Copyright (C) 2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=atfpolicy
PKG_VERSION:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/atfpolicy
  SECTION:=net
  CATEGORY:=Network
  TITLE:=A simple daemon for handling airtime fairness prioritization
  DEPENDS:=+libubox +libubus +libnl-tiny 
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/libnl-tiny

define Package/atfpolicy/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/atfpolicy $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/atfpolicy.init $(1)/etc/init.d/atfpolicy
	$(INSTALL_DATA) ./files/atfpolicy.conf $(1)/etc/config/atfpolicy
endef

$(eval $(call BuildPackage,atfpolicy))
