#
# Copyright (C) 2009-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=netfilter_flowtable
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0+
#PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/netfilter-flowtable
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  DEPENDS:=+libnfnetlink +libmnl +kmod-nf-flow-netlink
  TITLE:=API to the in-kernel flow offload table
  SUBMENU:=Applications
endef

define Package/netfilter-flowtable/description
  API to the in-kernel flow offload table
endef

TARGET_CFLAGS += $(FPIC)

TARGET_CPPFLAGS := \
	-D_GNU_SOURCE \
	-I$(LINUX_DIR)/user_headers/include \
	-I$(PKG_BUILD_DIR) \
	$(TARGET_CPPFLAGS) \

define Build/Compile
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LIBS="$(TARGET_LDFLAGS) -lnfnetlink  -lm"
endef

define Package/netfilter-flowtable/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/ftnl $(1)/usr/bin/
endef

$(eval $(call BuildPackage,netfilter-flowtable))