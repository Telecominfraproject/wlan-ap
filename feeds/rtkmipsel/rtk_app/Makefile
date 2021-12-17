#
## Copyright (C) 2006-2009 OpenWrt.org
#
## This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
# 

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=rtk_app
PKG_VERSION:=0.1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(KERNEL_BUILD_DIR)/$(PKG_NAME)
STAMP_PREPARED := $(STAMP_PREPARED)_$(call confvar,CONFIG_MTD_REDBOOT_PARTS)

include $(INCLUDE_DIR)/package.mk

define Package/rtk_app
  SECTION:=Tool
  CATEGORY:=Utilities
  DEPENDS:=+libpcap
  TITLE:=For some realtek useful utilities(for example,WLAN TX calibration..)
  DEFAULT:=y
endef

define Package/rtk_app/description
 This Package is for realtek to set WLAN TX calibration parameters 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

target=$(firstword $(subst -, ,$(BOARD)))

MAKE_FLAGS += TARGET="$(target)"
TARGET_CFLAGS += -Dtarget_$(target)=1 -Wall

ifdef CONFIG_MTD_REDBOOT_PARTS
  MAKE_FLAGS += FIS_SUPPORT=1
  TARGET_CFLAGS += -DFIS_SUPPORT=1
endif

ifdef CONFIG_RTL_819XD
	TARGET_CFLAGS += -DCONFIG_RTL_819XD=1
endif
ifdef CONFIG_RTL_8196E
	TARGET_CFLAGS += -DCONFIG_RTL_8196E=1
endif
ifdef CONFIG_RTL_8881A
	TARGET_CFLAGS += -DCONFIG_RTL_8881A=1 -DCONFIG_RTL_819XD=1
endif
ifdef CONFIG_WLAN_HAL_8197F
	TARGET_CFLAGS += -DCONFIG_WLAN_HAL_8197F=1
endif
ifdef CONFIG_RTL_8197F
	TARGET_CFLAGS += -DCONFIG_RTL_8197F=1
endif
ifeq ($(CONFIG_RTL_8198C),y)
	TARGET_CFLAGS += -DCONFIG_RTL_8198C=1
endif

ifdef CONFIG_WLAN_HAL_8881A
	TARGET_CFLAGS += -DHAVE_RTK_AC_SUPPORT=1
endif
ifdef CONFIG_RTL_8812_SUPPORT
	TARGET_CFLAGS += -DHAVE_RTK_AC_SUPPORT=1
endif
ifdef CONFIG_WLAN_HAL_8822BE
	TARGET_CFLAGS += -DHAVE_RTK_AC_SUPPORT=1
endif
ifdef CONFIG_RTL_HW_SETTING_OFFSET
	TARGET_CFLAGS += -DRTK_HW_OFFSET=$(CONFIG_RTL_HW_SETTING_OFFSET)
endif
ifeq ($(CONFIG_WLAN_HAL_8881A),y)
ifeq ($(CONFIG_USE_PCIE_SLOT_0),y)
	TARGET_CFLAGS += -DHAVE_RTK_DUAL_BAND_SUPPORT=1
endif
endif
ifeq ($(CONFIG_WLAN_HAL_8197F),y)
ifeq ($(CONFIG_USE_PCIE_SLOT_0),y)
        TARGET_CFLAGS += -DHAVE_RTK_DUAL_BAND_SUPPORT=1
endif
endif
ifeq ($(CONFIG_USE_PCIE_SLOT_0),y)
ifeq ($(CONFIG_USE_PCIE_SLOT_1),y)
	TARGET_CFLAGS += -DHAVE_RTK_DUAL_BAND_SUPPORT=1
endif
endif
ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
	TARGET_CFLAGS += -DHAVE_RTK_92D_SUPPORT=1
endif
ifeq ($(CONFIG_ENABLE_EFUSE),y)
	TARGET_CFLAGS += -DHAVE_RTK_EFUSE=1
endif
ifeq ($(CONFIG_RTL_WPS2_SUPPORT),y)
	TARGET_CFLAGS += -DHAVE_WIFI_SIMPLE_CONFIG=1
endif
ifeq ($(CONFIG_RTL_VAP_SUPPORT),y)
	TARGET_CFLAGS += -DHAVE_WIFI_MBSSID=8
endif

ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
	TARGET_CFLAGS += -DHAVE_RTK_4T4R_AC_SUPPORT
endif

ifeq ($(CONFIG_MTD_NAND),y)
	TARGET_CFLAGS += -DCONFIG_MTD_NAND=1
endif

define Package/rtk_app/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rtk_txcalr $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rtk_bootinfo $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/flash $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpsled $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rtk_pcap $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/config
	#$(INSTALL_DATA) ./files/rtk_app.conf $(1)/etc/config/rtk_app
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/rtk_app.init $(1)/etc/init.d/rtk_app
endef

$(eval $(call BuildPackage,rtk_app))
