#
# Copyright 2022-2026 Morse Micro
#
# This is free software, licensed under the GPL 2 license.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morse_driver
PKG_RELEASE:=1

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

PKG_VERSION:=mm6108-2.0.1

PKG_SOURCE_URL:=https://github.com/MorseMicro/morse_driver.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=98e1936c04ef9a62212c1c64b970218ecf08d15d
PKG_SOURCE_MIRROR:=0
PKG_MIRROR_HASH:=a4e648ce6189ceef7a0c535d25dc4e8528ecf3a775e2bf76146e83e6ee9fb55c

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

DTC=$(wildcard $(LINUX_DIR)/scripts/dtc/dtc)

define KernelPackage/morse
  SUBMENU:=Wireless Drivers
  TITLE:=Morse Micro WIFI HaLow driver
  DEPENDS:= \
    +kmod-mmc \
    +kmod-mac80211 \
    +kmod-trelay \
    +kmod-lib-crc7 \
    +morse-fw \
    +morse-board-config \
    +MORSE_USB:kmod-usb-core
  FILES:=\
	$(PKG_BUILD_DIR)/morse.ko \
	$(PKG_BUILD_DIR)/dot11ah/dot11ah.ko
  # Loaded by kmodloader at boot. Modules at the same dependency depth are
  # inserted in name order, so morse registers before mt7915e and the HaLow
  # radio keeps phy0, which the rest of the system expects.
  AUTOLOAD:=$(call AutoProbe,morse)
  # enable_watchdog also enables recovery by bus reset when a firmware restart
  # fails; this driver release has no separate enable_watchdog_reset.
  MODPARAMS.morse:=country=US enable_watchdog=1
  PROVIDES:=kmod-morse
endef

define KernelPackage/morse/config

       config MORSE_SDIO
            bool "SDIO support "
            default n
            depends on PACKAGE_kmod-morse

       config MORSE_SPI
            bool "SPI support "
            default y
            depends on PACKAGE_kmod-morse

       # No "depends on PACKAGE_kmod-morse" here: kmod-morse already depends on
       # MORSE_USB through +MORSE_USB:kmod-usb-core, and the reverse dependency
       # makes Kconfig fail with a recursive dependency error.
       config MORSE_USB
            bool "USB support "
            default n
            depends on USB_SUPPORT

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
            bool "Enable debug logging "
            default n
            select KERNEL_DYNAMIC_DEBUG
            depends on PACKAGE_kmod-morse
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
  MORSE_MAKEDEFS += CONFIG_MORSE_SDIO=y
endif

ifeq ($(CONFIG_MORSE_SPI),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_SPI=y
endif

ifeq ($(CONFIG_MORSE_USB),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_USB=y
endif

ifeq ($(CONFIG_MORSE_USER_ACCESS),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_USER_ACCESS=y
endif

ifeq ($(CONFIG_MORSE_VENDOR_COMMAND),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_VENDOR_COMMAND=y
endif

ifeq ($(CONFIG_MORSE_MONITOR),y)
  MORSE_MAKEDEFS += CONFIG_MORSE_MONITOR=y
endif

ifeq ($(CONFIG_MORSE_DEBUG),y)
  MORSE_MAKEDEFS += DEBUG=y
  MORSE_MAKEDEFS += CONFIG_MORSE_DEBUGFS=y
else
  MORSE_MAKEDEFS += DEBUG=n
endif

ifneq ($(CONFIG_MORSE_RC),y)
  MORSE_MAKEDEFS += CONFIG_DISABLE_MORSE_RC=y
endif

ifneq ($(CONFIG_MORSE_SDIO_ALIGNMENT),)
  MORSE_MAKEDEFS += CONFIG_MORSE_SDIO_ALIGNMENT=$(CONFIG_MORSE_SDIO_ALIGNMENT)
endif

# Must equal PKG_VERSION of the mac80211 package whose headers this driver is
# built against: feeds/mediatek/mac80211 (6.12.6). It is not derived from
# $(TOPDIR)/package/kernel/mac80211, which is the unused 23.05 copy (6.1.24).
# The driver compares it with KERNEL_VERSION(), so a wrong value shows up as
# missing struct members or mismatched prototypes.
MORSE_BACKPORT_VERSION:=v6.12.6

MORSE_MAKEDEFS += \
  MORSE_VERSION=0-$(PKG_VERSION) \
  KERNEL_SRC=$(LINUX_DIR) \
  CONFIG_BACKPORT_VERSION=$(MORSE_BACKPORT_VERSION)

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
# The driver builds with -Werror. spi.c emits a #warning when the kernel does
# not define SPI_CONTROLLER_ENABLE_CS_GPIOD, which this kernel does not. The
# macro only matters for GPIO chip selects; the MM6108 uses the controller's
# native chip select.
MORSE_MAKEDEFS += EXTRA_CFLAGS+="-Wno-error=int-in-bool-context -Wno-error=cpp"

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
