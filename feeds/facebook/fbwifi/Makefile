# Copyright (c) Facebook, Inc. and its affiliates.
# All rights reserved.
#
# This source code is licensed under the license found in the
# LICENSE file in the root directory of this source tree.
#


include $(TOPDIR)/rules.mk

PKG_NAME:=fbwifi
PKG_VERSION:=2
PKG_RELEASE:=0
PKG_LICENSE:=GPL-2.0

PKG_MAINTAINER:=Simon Kinane <skinane@fb.com>

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/fbwifi
  SUBMENU:=Captive Portals
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+iptables +luasec +luasocket \
	+libuci-lua +luaposix \
	+lua-cjson +uhttpd
  TITLE:=Facebook Wi-Fi
  PKGARCH:=all
endef

define Package/fbwifi/description
  Facebook Wi-Fi, an AP authorisation solution
endef


define Package/luci-app-fbwifi
  SUBMENU:=3. Applications
  SECTION:=luci
  CATEGORY:=LuCI
  TITLE:=LuCI support for Facebook Wi-Fi
  DEPENDS:= \
	+fbwifi \
	+luci-base +luci-mod-network +luci-mod-status +luci-theme-bootstrap
endef

define Package/luci-app-fbwifi/description
  LuCI support for Facebook Wi-Fi
endef


define Package/fbwifi/conffiles
/etc/config/fbwifi
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/fbwifi/install
	$(INSTALL_DIR) $(1)
	$(CP) ./files/fbwifi/* $(1)/
endef

define Package/luci-app-fbwifi/install
	$(INSTALL_DIR) $(1)
	$(CP) ./files/luci-app-fbwifi/* $(1)
endef

$(eval $(call BuildPackage,fbwifi))
$(eval $(call BuildPackage,luci-app-fbwifi))
