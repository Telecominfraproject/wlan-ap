#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=fips_test_tool
PKG_VERSION:=1.0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/fips_test_tool
  SECTION:=Utilities
  CATEGORY:=Utilities
  TITLE:=FIPS_TEST_TOOL
  SUBMENU:=FIPS
  DEPENDS:=+python3-light
endef

define Package/fips_test_tool/description
  fips_test_tool is a script to pass fips140-3.
  You can run this tool to pass test vector
  from NIST in local.
endef

Build/Compile/Default:=

define Package/fips_test_tool/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,fips_test_tool))
