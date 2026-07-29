#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the GPL 2 license.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morse_driver
PKG_RELEASE=4

PKG_VERSION:=1.17.8

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=LICENSE

PKG_SOURCE_URL:=https://github.com/MorseMicro/morse_driver.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=3eef5a0a43645808e501ff4b83f29d675588bd9b
PKG_MIRROR_HASH:=skip
PKG_HASH:=skip

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

DTC=$(wildcard $(LINUX_DIR)/scripts/dtc/dtc)

define KernelPackage/morse
  SUBMENU:=Wireless Drivers
  TITLE:=Morse Micro WIFI HaLow driver
  DEPENDS:= +kmod-mmc +kmod-usb-core +kmod-mac80211 +kmod-trelay +kmod-lib-crc7 +morse-fw +morse-board-config
  FILES:=\
    $(PKG_BUILD_DIR)/morse.ko \
    $(PKG_BUILD_DIR)/dot11ah/dot11ah.ko
  AUTOLOAD:=$(call AutoProbe,morse)
  MODPARAMS.morse:=country=US enable_watchdog_reset=Y enable_ps=0
  PROVIDES:=kmod-morse
endef

define KernelPackage/morse/config

       config MORSE_SDIO
            bool "SDIO support "
            default n
            depends on PACKAGE_kmod-morse

       config MORSE_SPI
            bool "SPI support "
            default n
            depends on PACKAGE_kmod-morse

       config MORSE_USB
            bool "USB support "
            default y
            depends on PACKAGE_kmod-morse

       config MORSE_USER_ACCESS
            bool "User space access support "
            default y
            depends on PACKAGE_kmod-morse

       config MORSE_VENDOR_COMMAND
            bool "Vendor command support "
            default y
            depends on PACKAGE_kmod-morse

       config MORSE_MONITOR
            bool "Monitor mode support "
            default n
            depends on PACKAGE_kmod-morse

       config MORSE_DEBUG
            bool "Enable debug "
            default n
            depends on PACKAGE_kmod-morse
            help
              Compiles the driver with debug info and some additional debug features.

      config MORSE_DEBUG_LOGGING
            bool "Enable debug logging"
            default n
            depends on PACKAGE_kmod-morse
            select KERNEL_DYNAMIC_DEBUG_CORE
            help
              Enables debug level logging from the driver. Logging must be enabled at run time.
              There may be a binary size and performance impact when this is y.

       config MORSE_RC
            bool "Enable MMRC (Morse Micro Rate Control) "
            default y
            depends on PACKAGE_kmod-morse

       config MORSE_SDIO_ALIGNMENT
            int "Required alignment for bulk SDIO reads/writes"
            default 2
            range 2 8
            depends on MORSE_SDIO
            help
              Number of bytes to align data buffers for bulk SDIO transactions. Some hosts have
              specific alignment requirements for DMA, use this setting to
              adjust the alignment required.

              Must be a power of 2.

endef

ifeq ($(CONFIG_MORSE_SDIO),y)
  NOSTDINC_FLAGS += -DCONFIG_MORSE_SDIO
  MORSE_MAKEDEFS += CONFIG_MORSE_SDIO=y
endif

ifeq ($(CONFIG_MORSE_SPI),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_SPI=y
endif

ifeq ($(CONFIG_MORSE_USB),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_USB=y
endif

ifeq ($(CONFIG_MORSE_USER_ACCESS),y)
  NOSTDINC_FLAGS += -DCONFIG_MORSE_USER_ACCESS
  MORSE_MAKEDEFS += CONFIG_MORSE_USER_ACCESS=y
endif

ifeq ($(CONFIG_MORSE_VENDOR_COMMAND),y)
  NOSTDINC_FLAGS += -DCONFIG_MORSE_VENDOR_COMMAND
  MORSE_MAKEDEFS += CONFIG_MORSE_VENDOR_COMMAND=y
endif

ifeq ($(CONFIG_MORSE_MONITOR),y)
  NOSTDINC_FLAGS += -DCONFIG_MORSE_MONITOR
  MORSE_MAKEDEFS += CONFIG_MORSE_MONITOR=y
endif

ifeq ($(CONFIG_MORSE_DEBUG),y)
  # This DEBUG (used by the driver Makefile) should not be confused with
  # -DDEBUG (used by the kernel build system).
  MORSE_MAKEDEFS += DEBUG=y
  MORSE_MAKEDEFS += CONFIG_MORSE_DEBUGFS=y
  MORSE_MAKEDEFS += CONFIG_MORSE_ENABLE_TEST_MODES=y
else
  MORSE_MAKEDEFS += DEBUG=n
endif

ifeq ($(CONFIG_MORSE_RC),y)
  NOSTDINC_FLAGS += -DCONFIG_MORSE_RC
  MORSE_MAKEDEFS += CONFIG_MORSE_RC=y
endif

MORSE_MAKEDEFS += \
  MORSE_VERSION=0-$(PKG_VERSION) \
  KERNEL_SRC=$(LINUX_DIR) \
  CONFIG_MORSE_SDIO_ALIGNMENT=$(CONFIG_MORSE_SDIO_ALIGNMENT) \
  CONFIG_BACKPORT_VERSION=v6.18
# TIP platform is locked to mac80211 backports 6.18.7 (cannot downgrade).
# Morse 1.17.8 is officially validated only up to Linux 6.12; set the knob
# to v6.18 so MAC80211_BACKPORT_VERSION_CODE selects 6.18 API prototypes,
# combined with compat patches 030 (S1G chan-flags) and 031 (twt union).

NOSTDINC_FLAGS = \
	-I$(PKG_BUILD_DIR) \
	-I$(STAGING_DIR)/usr/include/mac80211-backport/uapi \
	-I$(STAGING_DIR)/usr/include/mac80211-backport \
	-I$(STAGING_DIR)/usr/include/mac80211/uapi \
	-I$(STAGING_DIR)/usr/include/mac80211 \
	-include backport/autoconf.h \
	-include backport/backport.h

ifeq ($(CONFIG_MORSE_DEBUG_LOGGING),y)
  NOSTDINC_FLAGS += -DDYNAMIC_DEBUG_MODULE
  NOSTDINC_FLAGS += -DDEBUG
endif

MORSE_MAKEDEFS += CONFIG_WLAN_VENDOR_MORSE=m
MORSE_MAKEDEFS += V=1

NOSTDINC_FLAGS += -DMORSE_TRACE_PATH=.

include $(INCLUDE_DIR)/kernel-defaults.mk

define Build/Compile
	$(MAKE) $(MORSE_MAKEDEFS) $(PKG_JOBS) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		M="$(PKG_BUILD_DIR)" \
		NOSTDINC_FLAGS="$(NOSTDINC_FLAGS)" \
		modules
endef

$(eval $(call KernelPackage,morse))
