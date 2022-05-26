#
# Copyright (C) 2022 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=unetd
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/nbd168/unetd
PKG_SOURCE_DATE:=2022-05-23
PKG_SOURCE_VERSION:=f75c2e70c0e494566dbbb20d408309412b4110e6
PKG_MIRROR_HASH:=633e6b0406b89850d0caf933fb419345be37d5deab812251d6183a7ab3f2cd42

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/unetd
  SECTION:=utils
  CATEGORY:=Base system
  TITLE:=Wireguard network configuration service
  DEPENDS:=+libubox +libubus +libblobmsg-json +libnl-tiny +kmod-wireguard
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-I$(STAGING_DIR)/usr/include

CMAKE_OPTIONS += \
	-DLIBNL_LIBS=-lnl-tiny

define Package/unetd/install
	$(INSTALL_DIR) \
		$(1)/etc/init.d \
		$(1)/lib/netifd/proto \
		$(1)/usr/sbin
	$(INSTALL_BIN) \
		$(PKG_INSTALL_DIR)/usr/sbin/unetd \
		$(1)/usr/sbin/
	$(INSTALL_BIN) ./files/unetd.init $(1)/etc/init.d/unetd
	$(INSTALL_BIN) ./files/unetd.sh $(1)/lib/netifd/proto
endef

$(eval $(call BuildPackage,unetd))
