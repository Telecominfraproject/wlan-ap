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
PKG_RELEASE:=1

PKG_VERSION:=mm6108-2.0.1

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

RSTRIP:=:

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

# BCF files installed per profile and chip. mm6108-2.0.1 firmware only accepts
# board config version 12; an older file makes the chip start in failsafe mode.
BCF_PROFILE:=$(patsubst DEVICE_%,%,$(call qstrip,$(CONFIG_TARGET_PROFILE)))

BCF_SET_edgecore_eap112_MM6108:= \
  bcf_mf08651_us.bin \
  bcf_mf08651_jp.bin \
  bcf_mf08651_eu.bin
BCF_SET_edgecore_eap112_MM8108:=
BCF_CHIPS_edgecore_eap112:=MM6108
# Build-time target of bcf_default.bin. 21-morse-bcf-select re-points the link
# at run time from the detected chip, OTP board type and UCI country.
BCF_DEFAULT_edgecore_eap112:=bcf_mf08651_us.bin

BCF_CHIPS:=$(BCF_CHIPS_$(BCF_PROFILE))
BCF_FILES:=$(sort $(foreach chip,$(BCF_CHIPS),$(BCF_SET_$(BCF_PROFILE)_$(chip))))
BCF_DEFAULT:=$(or $(BCF_DEFAULT_$(BCF_PROFILE)),bcf_failsafe.bin)

ifeq ($(filter $(BCF_DEFAULT),bcf_failsafe.bin $(BCF_FILES)),)
  $(error morse-board-config: BCF_DEFAULT_$(BCF_PROFILE)=$(BCF_DEFAULT) is not \
    part of the BCFs installed for profile $(BCF_PROFILE) ($(BCF_FILES)))
endif

define Package/morse-board-config
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=https://www.morsemicro.com/
  TITLE:=Morse Micro WIFI HaLow Board Config File
  PROVIDES:=morse-board-config
endef

define Package/morse-board-config/description
  Board configuration files (BCF) for the Morse Micro HaLow chip, plus the
  hotplug selector that points bcf_default.bin at the BCF matching the
  detected chip, OTP board type and regulatory region.
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

define Build/Compile
endef

define Package/morse-board-config/install
	$(INSTALL_DIR) $(1)/lib/firmware/morse
	$(INSTALL_DIR) $(1)/usr/share/morse-board-config
	$(INSTALL_DIR) $(1)/etc/hotplug.d/ieee80211

	for bcf in $(BCF_FILES); do \
		if [ ! -e ./files/lib/firmware/morse/$$$$bcf ]; then \
			echo "Error: BCF $$$$bcf is listed in BCF_SET_* but missing from ./files/lib/firmware/morse/." ; \
			exit 1 ; \
		fi ; \
		$(INSTALL_DATA) ./files/lib/firmware/morse/$$$$bcf $(1)/lib/firmware/morse/ ; \
	done

	if [ -e ./files/lib/firmware/morse/bcf_failsafe.bin ]; then \
		$(INSTALL_DATA) ./files/lib/firmware/morse/bcf_failsafe.bin $(1)/lib/firmware/morse/ ; \
	elif [ "$(BCF_DEFAULT)" = bcf_failsafe.bin ]; then \
		echo "Error: bcf_failsafe.bin is the bcf_default.bin target for profile $(BCF_PROFILE) but is missing from ./files/lib/firmware/morse/." ; \
		exit 1 ; \
	else \
		echo "Warning: bcf_failsafe.bin not available; installing without it." ; \
	fi

	if [ -e $(1)/lib/firmware/morse/$(BCF_DEFAULT) ]; then \
		ln -s /lib/firmware/morse/$(BCF_DEFAULT) $(1)/lib/firmware/morse/bcf_default.bin ; \
	else \
		echo "Warning: $(BCF_DEFAULT) not installed; skipping the bcf_default.bin symlink rather than leaving it dangling." ; \
	fi

	$(INSTALL_DATA) ./files/usr/share/morse-board-config/db.txt $(1)/usr/share/morse-board-config/db.txt
	$(INSTALL_DATA) ./files/usr/share/morse-board-config/bcf-select.conf $(1)/usr/share/morse-board-config/bcf-select.conf
	$(INSTALL_BIN) ./files/etc/hotplug.d/ieee80211/21-morse-bcf-select $(1)/etc/hotplug.d/ieee80211/21-morse-bcf-select
endef

define Package/morse-board-config-hotplug-model/install
	$(INSTALL_DIR) $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DATA) ./files/etc/hotplug.d/ieee80211/20-module-type $(1)/etc/hotplug.d/ieee80211/20-module-type
endef

$(eval $(call BuildPackage,morse-board-config))
$(eval $(call BuildPackage,morse-board-config-hotplug-model))
