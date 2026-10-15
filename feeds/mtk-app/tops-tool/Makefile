# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Mediatek Inc. All Rights Reserved.
# Author: Alvin Kuo <alvin.kuo@mediatek.com>
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tops-tool
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/package-defaults.mk


TOPS_TOOL_CONFIGS += \
	CONFIG_MTK_TOPS_TOOL_SAVE_LOG=$(CONFIG_MTK_TOPS_TOOL_SAVE_LOG)

MAKE_VARS += $(TOPS_TOOL_CONFIGS)

EXTRA_CFLAGS += \
	-I$(PKG_BUILD_DIR)/inc \
	$(patsubst CONFIG_%=y, -DCONFIG_%=1, $(filter %=y,$(TOPS_TOOL_CONFIGS)))

define Package/tops-tool
  TITLE:=Mediatek Tunnel Offload Processor System User Tool
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  DEFAULT:=n
  SUBMENU:=Applications
endef

define Package/tops-tool/description
  This package is support for MediaTek tunnel offload processor system user tool.
  The tool will monitor TOPS status, such as TOPS core utilization rate and core
  debug dump info. Some features are not monitored by default unless user trigger
  it to start. The tool will support logging functionality in the future.
endef

define Package/tops-tool/config
	source "$(SOURCE)/Config.in"
endef

define Package/tops-tool/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tops-tool $(1)/usr/sbin/
endef

define Package/tops-tool-autoload
  TITLE:=Mediatek Tunnel Offload Processor System User Tool Auto Load
  CATEGORY:=MTK Properties
  SUBMENU:=Applications
  DEPENDS:=tops-tool
  DEFAULT:=y if PACKAGE_kmod-tops-release-autoload || PACKAGE_kmod-tops-dev-autoload
endef

define Package/tops-tool-autoload/description
  This package is support for MediaTek tunnel offload processor system user tool
  autoload. The tool will automatically start monitor TOPS status when system
  boot up.
endef

TOPS_TOOL_INIT_FILES := \
	./files/tops-tool.init \
	./files/tops-tool-dump.init
ifeq ($(CONFIG_MTK_TOPS_TOOL_SAVE_LOG), y)
TOPS_TOOL_INIT_FILES += \
	./files/tops-tool-log.init
endif

define Package/tops-tool-autoload/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(foreach file, $(TOPS_TOOL_INIT_FILES), \
		$(INSTALL_BIN) $(file) $(1)/etc/init.d/$(notdir $(basename $(file)));)
endef

$(eval $(call BuildPackage,tops-tool))
$(eval $(call BuildPackage,tops-tool-autoload))
