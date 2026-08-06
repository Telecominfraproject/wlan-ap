#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=netifd-morse
PKG_RELEASE=1

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>

include $(INCLUDE_DIR)/package.mk

define Package/netifd-morse
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Morse Micro HaLow support for netifd
  DEPENDS:= netifd \
	+@BUSYBOX_CONFIG_USLEEP +@BUSYBOX_CONFIG_BASE64 +@BUSYBOX_CONFIG_XXD \
	+morse-regdb +wpa_event_listener +wpa_supplicant_s1g +hostapd_s1g +morsecli
endef

define Build/Compile
endef

define Package/netifd-morse/install
	# morse-halow-native-netifd: morse runs on 25.12 NATIVE netifd (no
	# downgrade). The netifd busy-loop is now root-fixed by patch 0186
	# (handler_load EOF-safety), so morse radios default to enabled
	# (disabled=0, like 7EC) and no boot-time disable fuse is needed.
	# 30-morse-detect still handles the USB hotplug case: it polls until the
	# phy netdev is populated (fully ready) before bringing the radio up, so
	# find_phy() in drv_morse_setup always succeeds.
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DIR) $(1)/etc/hotplug.d/net
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) ./lib/* $(1)/lib/
	# WDS 4-addr broadcast loop fix: on every (re)association hostapd
	# recreates the AP_VLAN child (wlan0.staN) and bridges it next to the AP
	# master port (wlan0) on the SAME radio, so the STA's uplink broadcast is
	# flooded back out the air to itself ("received packet ... own address as
	# source"). This netdev hotplug puts both morse WDS ports into bridge
	# "isolated" mode (cross-port block, wired uplink still works), which was
	# verified on HW to stop the loop. Must be a hotplug (not one-shot setup)
	# because wlan0.staN is dynamic per association.
	$(INSTALL_BIN) ./lib/hotplug.d/net/25-morse-wds-isolate $(1)/etc/hotplug.d/net/
	$(INSTALL_BIN) ./etc/init.d/halow-gpio-reset $(1)/etc/init.d/
	# ieee80211 hotplug is only scanned under /etc/hotplug.d/ieee80211, NOT
	# /lib/hotplug.d. Install the morse PHY detect/remove hooks there.
	$(INSTALL_BIN) ./lib/hotplug.d/ieee80211/30-morse-detect $(1)/etc/hotplug.d/ieee80211/
	$(INSTALL_BIN) ./lib/hotplug.d/ieee80211/40-morse-remove $(1)/etc/hotplug.d/ieee80211/
	# HaLow capabilities hotplug sync [ECCLOUD-10057]: keep
	# /etc/ucentral/capabilities.json in sync with the USB HaLow dongle so
	# the cloud stops rejecting "band HaLow". capabilities.json is generated
	# once at boot (often before the morse firmware/debugfs is ready), so it
	# must be regenerated on hotplug events + a boot/cron watchdog.
	$(INSTALL_BIN) ./usr/sbin/halow-capabilities-regen $(1)/usr/sbin/
	$(INSTALL_BIN) ./usr/sbin/halow-capabilities-sync $(1)/usr/sbin/
	$(INSTALL_BIN) ./etc/init.d/halow-capabilities-boot-sync $(1)/etc/init.d/
	$(INSTALL_BIN) ./etc/uci-defaults/99-halow-capabilities-sync $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,netifd-morse))
