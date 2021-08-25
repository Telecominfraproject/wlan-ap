#
# Copyright (C) 2014-2021 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ustp
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/nbd168/ustp
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-08-24
PKG_SOURCE_VERSION:=fd93aaef9ba99188e30731651c589a2ab9bfc421
PKG_MIRROR_HASH:=ef99770c4992e236feb46efef7ded68b77de8dbd0a8aa99ab71788e7b35f9b58

PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ustp
  SECTION:=net
  CATEGORY:=Network
  TITLE:=OpenWrt STP/RSTP daemon
  DEPENDS:=+libubox +libubus
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include -flto
TARGET_LDFLAGS += -flto -fuse-linker-plugin

define Package/ustp/install
	$(INSTALL_DIR) $(1)/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ipkg-install/sbin/* $(1)/sbin/
	$(INSTALL_BIN) ./files/ustpd.init $(1)/etc/init.d/ustpd
endef

$(eval $(call BuildPackage,ustp))
