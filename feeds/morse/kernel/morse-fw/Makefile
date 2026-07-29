#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the MIT License.
#
# This package installs Morse Micro firmware binaries, which are
# distributed under the Morse Micro Binary Distribution License.
# This license is available in LICENSE.binaries.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morse-fw
PKG_RELEASE=3

PKG_VERSION:=1.17.8
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/MorseMicro/morse-firmware.git
PKG_SOURCE_VERSION:=$(PKG_VERSION)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_MIRROR_HASH:=skip

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>
PKG_BUILD_PARALLEL:=1

RSTRIP:=:

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define Package/morse-fw
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow firmware for 8108
  PROVIDES:=morse-fw
endef

define Build/Compile

endef

# Files from the git repo are overridden by a file in this path (for local dev).
define Package/morse-fw/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse
	if [ -e ./files/lib/firmware/morse/mm8108b2-rl.bin ]; then \
		$(INSTALL_DATA) ./files/lib/firmware/morse/mm8108b2-rl.bin $(1)/lib/firmware/morse/; \
	else \
		$(INSTALL_DATA) $(PKG_BUILD_DIR)/firmware/mm8108b2-rl.bin $(1)/lib/firmware/morse/; \
	fi
endef

$(eval $(call BuildPackage,morse-fw))
