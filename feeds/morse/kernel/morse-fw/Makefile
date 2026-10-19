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
PKG_RELEASE:=1

PKG_VERSION:=mm6108-2.0.1

PKG_SOURCE_URL:=https://github.com/MorseMicro/morse-firmware.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=29fc28b2902ae71648757573caf148152fb31b50
PKG_SOURCE_MIRROR:=0
PKG_MIRROR_HASH:=af72c2964d93d95f50ff42aa3e74ab2b411748705fbb3023be874661cbd50485

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

# Firmware images must not be stripped.
RSTRIP:=:

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

# Every subpackage PROVIDES the virtual name morse-fw. Profiles must list the
# concrete package (morse-fw-6108 for MM6108): a virtual name has no config
# symbol of its own, so listing "morse-fw" would install no firmware.
define Package/morse-fw-6108
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow standard firmware for 6108
  PROVIDES:=morse-fw
  CONFLICTS:=morse-fw-8108 morse-fw-8108-tlm morse-fw-8108-flm
endef

define Package/morse-fw-6108-tlm
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow Thin LMAC firmware for 6108
  PROVIDES:=morse-fw
  CONFLICTS:=morse-fw-8108 morse-fw-8108-tlm morse-fw-8108-flm
endef

define Package/morse-fw-6108-tlm/description
  This firmware reduces the per-station table size in the chip to
  allow a large number of associations in AP mode.
endef

define Package/morse-fw-8108
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow standard firmware for 8108
  PROVIDES:=morse-fw
endef

define Package/morse-fw-8108-tlm
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow Thin LMAC firmware for 8108
  PROVIDES:=morse-fw
endef

define Package/morse-fw-8108-tlm/description
  This firmware reduces the per-station table size in the chip to
  allow a large number of associations in AP mode.
endef

define Package/morse-fw-8108-flm
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow FullMAC firmware for 8108
  PROVIDES:=morse-fw
endef

define Package/morse-fw-8108-flm/description
  This firmware implements the functionality of the mac80211
  module (and some wpa_supplicant functionality) on the 8108.
  This will allow you to reduce host processor CPU usage
  and maintain association if the host processor is turned off.
  However, it currently only supports station/client mode
  (i.e. not AP/mesh/adhoc).
endef

define Build/Compile
endef

define install_file_or_override
	if [ -e ./files/lib/firmware/morse/$(1) ]; then \
		$(INSTALL_DATA) ./files/lib/firmware/morse/$(1) $(2)/lib/firmware/morse/; \
	else \
		$(INSTALL_DATA) $(PKG_BUILD_DIR)/firmware/$(1) $(2)/lib/firmware/morse/; \
	fi
endef

define Package/morse-fw-6108/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse/.
	$(call install_file_or_override,mm6108.bin,$(1))
endef

define Package/morse-fw-6108-tlm/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse/.
	$(call install_file_or_override,mm6108-tlm.bin,$(1))
endef

define Package/morse-fw-8108/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse/.
	$(call install_file_or_override,mm8108b2-rl.bin,$(1))
endef

define Package/morse-fw-8108-tlm/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse/.
	$(call install_file_or_override,mm8108b2-tlm-rl.bin,$(1))
endef

define Package/morse-fw-8108-flm/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse/.
	$(call install_file_or_override,mm8108b2-flm-rl.bin,$(1))
endef

$(eval $(call BuildPackage,morse-fw-6108))
$(eval $(call BuildPackage,morse-fw-6108-tlm))
$(eval $(call BuildPackage,morse-fw-8108))
$(eval $(call BuildPackage,morse-fw-8108-tlm))
$(eval $(call BuildPackage,morse-fw-8108-flm))
