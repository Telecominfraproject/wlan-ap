#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the MIT License.
#
# This package installs Morse Micro board configuration binaries, which are
# distributed under the Morse Micro Binary Distribution License.
# This license is available in LICENSE.binaries.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morse-board-config
PKG_RELEASE=6

PKG_VERSION:=rel_1_12_4_2024_Jun_11

PKG_SOURCE:=morsemicro-bcf-rel_1_12_4_2024_Jun_11.tar
PKG_SOURCE_URL:=https://github.com/MorseMicro/bcf_binaries/releases/download/v1.12.4/
PKG_HASH:=e403764730aa149e78874135da154bab2a24574308d3f2df88c9b4c125766ae3

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>
PKG_BUILD_PARALLEL:=1

RSTRIP:=:

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define Package/morse-board-config
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  TITLE:=Morse Micro WIFI HaLow Board Config File
  PROVIDES:=morse-board-config
endef

define Package/morse-board-config-hotplug-model
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=Hotplug scripts for module type reporting
  DEPENDS:=+morse-board-config
endef

define Package/morse-board-config-hotplug-model/description
  When enabled, a hotplug script will be installed to
  append a relevant module type suffix based on the value
  programmed into the Morse chips OTP bits.
endef

TAR_CMD=$(HOST_TAR) -C $(1) $(TAR_OPTIONS)

define Build/Compile

endef

define Package/morse-board-config/install
	$(INSTALL_DIR) $(1)/usr/share/morse-bcf
	$(INSTALL_DIR) $(1)/lib/firmware/morse

	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lib/firmware/morse/* $(1)/lib/firmware/morse/

	# Install all files from override folder, including failsafe.
	$(INSTALL_DATA) ./files/lib/firmware/morse/* $(1)/lib/firmware/morse/

ifeq ($(CONFIG_TARGET_PROFILE), "DEVICE_edgecore_eap112")
	ln -s /lib/firmware/morse/bcf_edgecore_eap112_fcc.bin $(1)/lib/firmware/morse/bcf_default.bin
else
	# Link the failsafe bcf by default so userspace can query the driver.
	ln -s /lib/firmware/morse/bcf_failsafe.bin $(1)/lib/firmware/morse/bcf_default.bin
endif
	# Create symlinks for the production programmed otp devices
	while IFS=, read -r board_type suffix serial; do \
		ln -s /lib/firmware/morse/bcf_$$$${serial}.bin $(1)/lib/firmware/morse/bcf_boardtype_$$$${board_type}.bin; \
	done < ./files/usr/share/morse-bcf/db.txt

	$(INSTALL_DATA) ./files/usr/share/morse-bcf/db.txt $(1)/usr/share/morse-bcf/db.txt
endef

define Package/morse-board-config-hotplug-model/install
	$(INSTALL_DIR) $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DATA) ./files/etc/hotplug.d/ieee80211/20-module-type $(1)/etc/hotplug.d/ieee80211/20-module-type
endef

$(eval $(call BuildPackage,morse-board-config))
$(eval $(call BuildPackage,morse-board-config-hotplug-model))
