#
# Copyright (C) 2007-2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mac80211

PKG_VERSION:=6.6.15
PKG_RELEASE:=2
PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=COPYING

PKG_SOURCE_URL:=http://mirror2.openwrt.org/sources/
PKG_HASH:=3bbc461121134fda9089c084a5eed577d05e7837a157edf9a3797937172a3ece

PKG_BACKPORTS_VERSION:=965f73fc
PKG_BACKPORTS_SOURCE_URL:=git://git.kernel.org/pub/scm/linux/kernel/git/backports/backports.git
PKG_BACKPORTS_BRANCH:=master

PKG_SOURCE:=backports-$(PKG_VERSION).tar.xz
#PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(if $(BUILD_VARIANT),$(PKG_NAME)-$(BUILD_VARIANT)/)backports-$(PKG_VERSION)
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/backports-$(PKG_VERSION)
PKG_BUILD_PARALLEL:=1

LOCAL_SRC:=$(TOPDIR)/feeds/qca/mac80211/src/
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_DRIVERS = \
	mac80211-hwsim

PKG_CONFIG_DEPENDS:= \
	CONFIG_PACKAGE_kmod-mac80211 \
	CONFIG_PACKAGE_CFG80211_TESTMODE \
	CONFIG_PACKAGE_MAC80211_MESSAGE_TRACING \
	CONFIG_PACKAGE_MAC80211_DEBUGFS \
	CONFIG_PACKAGE_MAC80211_MESH \
	CONFIG_PACKAGE_MAC80211_TRACING \
	CONFIG_PACKAGE_IWLWIFI_DEBUG \
	CONFIG_PACKAGE_IWLWIFI_DEBUGFS \
	CONFIG_PACKAGE_RTLWIFI_DEBUG \
	CONFIG_PACKAGE_QCN_EXTN \

include $(INCLUDE_DIR)/package.mk

WMENU:=Wireless Drivers

define KernelPackage/mac80211/Default
  SUBMENU:=$(WMENU)
  URL:=https://wireless.wiki.kernel.org/
  MAINTAINER:=Felix Fietkau <nbd@nbd.name>
endef

config_package=$(if $(CONFIG_PACKAGE_kmod-$(1)),m)

# config_pacakge:
# 1 = OpenWrt KernelPackage name
# 2 = Optional list of variants for which the module may be selected:
#     If empty, the module is only selected with the first variant
#     Use $(ALL_VARIANTS) to allow module selection in all variants
#config_package=$(if $(and $(CONFIG_PACKAGE_kmod-$(1)),$(call mac80211_variant_check,$(2))),m)
#mac80211_variant_check=$(if $(ALL_VARIANTS),$(filter $(BUILD_VARIANT),$(if $(1),$(1),$(firstword $(ALL_VARIANTS)))),y)

config-y:= \
	WLAN \
	NL80211_TESTMODE \
	CFG80211_CERTIFICATION_ONUS \
	MAC80211_RC_MINSTREL \
	MAC80211_RC_MINSTREL_HT \
	MAC80211_RC_MINSTREL_VHT \
	MAC80211_RC_DEFAULT_MINSTREL \
	WLAN_VENDOR_ADMTEK \
	WLAN_VENDOR_ATH \
	WLAN_VENDOR_RSI \

#config-$(call config_package,cfg80211,$(ALL_VARIANTS)) += CFG80211
config-$(call config_package,cfg80211) += CFG80211
config-$(CONFIG_PACKAGE_CFG80211_TESTMODE) += NL80211_TESTMODE

#config-$(call config_package,mac80211,$(ALL_VARIANTS)) += MAC80211
config-$(call config_package,mac80211) += MAC80211
config-$(CONFIG_PACKAGE_MAC80211_MESH) += MAC80211_MESH

config-$(CONFIG_PACKAGE_kmod-mac80211) += MAC80211_DEBUG_MENU MAC80211_STA_DEBUG MAC80211_MLME_DEBUG
include ath.mk

PKG_CONFIG_DEPENDS += \
	$(patsubst %,CONFIG_PACKAGE_kmod-%,$(PKG_DRIVERS))

define KernelPackage/cfg80211
  $(call KernelPackage/mac80211/Default)
  TITLE:=cfg80211 - wireless configuration API
  DEPENDS+= +iw +wireless-regdb +USE_RFKILL:kmod-rfkill +wifi-scripts
  ABI_VERSION:=$(PKG_VERSION)-$(PKG_RELEASE)
  FILES:= \
	$(PKG_BUILD_DIR)/compat/compat.ko \
	$(PKG_BUILD_DIR)/net/wireless/cfg80211.ko
endef

define Package/ath-legacy-wifi-scripts
  SECTION:=QCA
  CATEGORY:=QCA software
  TITLE:= ATH legacy wifi scripts for kernel6.6
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
endef

define Package/ath-legacy-wifi-scripts/install
	$(INSTALL_DIR) $(1)/lib/wifi/
	$(INSTALL_DIR) $(1)/lib/netifd/
	$(INSTALL_DIR) $(1)/lib/netifd/wireless/
	$(INSTALL_BIN) ./files-qca/lib/wifi/mac80211.sh $(1)/lib/wifi/mac80211.sh
	$(INSTALL_BIN) ./files-qca/lib/netifd-wlan/wireless/mac80211.sh $(1)/lib/netifd/wireless/
endef

define KernelPackage/cfg80211/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/devcoredump
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/lib/netifd/wireless
	$(INSTALL_DIR) $(1)/lib/wifi/sawf
	$(INSTALL_DIR) $(1)/lib/wifi/sawf/telemetry
	$(INSTALL_DIR) $(1)/ini
	$(INSTALL_DIR) $(1)/ini/internal
	$(INSTALL_DATA) ./files-qca/lib/wifi/def_service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) ./files-qca/lib/wifi/service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) ./files-qca/lib/wifi/telemetry/config.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) ./files-qca/lib/wifi/telemetry/sla.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) ./files-qca/lib/wifi/telemetry/sla_detect.json $(1)/lib/wifi/sawf/telemetry/
ifeq ($(CONFIG_KERNEL_FTRACE),y)
	$(INSTALL_DATA) ./files-qca/lib/ftrace_enable_events.sh $(1)/lib
endif
ifneq ($(wildcard ./files-qca/lib/wifi/pre_post_wifi_operations.sh),)
	$(INSTALL_DATA) ./files-qca/lib/wifi/pre_post_wifi_operations.sh $(1)/lib/wifi/
endif
	$(INSTALL_BIN) ./files-qca/lib/performance.sh $(1)/lib
	$(INSTALL_BIN) ./files-qca/lib/smp_affinity_settings.sh $(1)/lib
	$(INSTALL_BIN) ./files-qca/lib/boost_performance.sh $(1)/lib
	$(INSTALL_BIN) ./files-qca/lib/mandatory_dp_stats.sh $(1)/lib
	$(INSTALL_BIN) ./files-qca/lib/ds_enable.sh $(1)/lib
	$(INSTALL_BIN) ./files-qca/lib/apsta_mode.sh $(1)/lib
ifeq ($(CONFIG_KERNEL_ATHMEMDEBUG),y)
	$(INSTALL_BIN) ./files-qca/lib/memleak_check.sh $(1)/lib
endif
	$(INSTALL_BIN) ./files-qca/coredump.sh $(1)/etc/hotplug.d/devcoredump
	$(INSTALL_BIN) ./files-qca/lib/wifi-config.sh $(1)/etc/hotplug.d/ieee80211/01-wifi-detect
	$(INSTALL_BIN) ./files-qca/etc/init.d/ath11k_nss_enable.sh $(1)/etc/init.d
	$(INSTALL_BIN) ./files-qca/etc/init.d/ath11k_uboot_mod_params.sh $(1)/etc/init.d
	$(INSTALL_BIN) ./files-qca/etc/init.d/qca-nss-pbuf $(1)/etc/init.d
	$(INSTALL_BIN) ./files-qca/etc/init.d/diag_socket_app $(1)/etc/init.d
	$(INSTALL_BIN) ./files-qca/etc/init.d/ath12k_dyn_dbg_enable.sh $(1)/etc/init.d
	$(INSTALL_CONF) ./files-qca/ini/*.ini $(1)/ini/
	$(INSTALL_CONF) ./files-qca/ini/internal/*.ini $(1)/ini/internal/
endef

define KernelPackage/cfg80211/description
cfg80211 is the Linux wireless LAN (802.11) configuration API.
endef

define KernelPackage/cfg80211/config
  if PACKAGE_kmod-cfg80211

	config PACKAGE_CFG80211_TESTMODE
		bool "Enable testmode command support"
		default n
		help
		  This is typically used for tests and calibration during
		  manufacturing, or vendor specific debugging features

  endif
endef

define KernelPackage/mac80211
  $(call KernelPackage/mac80211/Default)
  TITLE:=Linux 802.11 Wireless Networking Stack
  # +kmod-crypto-cmac is a runtime only dependency of net/mac80211/aes_cmac.c
  DEPENDS+= +kmod-cfg80211 +kmod-crypto-cmac +kmod-crypto-ccm +kmod-crypto-gcm \
	+kmod-qca-nss-ppe \
	+kmod-qca-nss-ppe-vp \
	+kmod-qca-nss-ppe-ds \
	+kmod-qca-nss-sfe \
	+kmod-qca-nss-wifi-plugins \
	+kmod-bonding
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-qca-nss-drv:kmod-qca-nss-drv
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-qca-nss-sfe:kmod-qca-nss-sfe
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-qca-nss-ppe:kmod-qca-nss-ppe
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-qca-nss-ppe-vp:kmod-qca-nss-ppe-vp
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-qca-nss-ppe-ds:kmod-qca-nss-ppe-ds
  PKG_BUILD_DEPENDS+= PACKAGE_kmod-bonding:kmod-bonding

  KCONFIG:=\
	CONFIG_AVERAGE=y \
	CONFIG_QCOM_RPROC_DISABLE_MPD_SUPPORT=y \
	CONFIG_PCIE_QCOM_EARLY_CAL_SUPPORT=y

  FILES:= $(PKG_BUILD_DIR)/net/mac80211/mac80211.ko
  ABI_VERSION:=$(PKG_VERSION)-$(PKG_RELEASE)
  MENU:=1
endef

define KernelPackage/mac80211/config
  if PACKAGE_kmod-mac80211

        if PACKAGE_kmod-qca-nss-ppe-vp
	   config PACKAGE_MAC80211_PPE_SUPPORT
		   bool "Enable PPE support for IPQ platform"
		   default y
		   help
		     This option enables support for PPE in QCA boards
        endif

        if PACKAGE_kmod-qca-nss-ppe-ds
	   config PACKAGE_MAC80211_DS_SUPPORT
		   bool "Enable DS support for IPQ platform"
		   default y
		   help
		     This option enables support for ds netdev in QCA boards.
        endif

        if PACKAGE_kmod-qca-nss-sfe
	   config PACKAGE_MAC80211_SFE_SUPPORT
		   bool "Enable sfe support for IPQ platform"
		   default y
		   help
		     This option enables support for qca sfe in QCA boards
        endif

	config PACKAGE_MAC80211_ATHMEMDEBUG
		bool "Enable memory debug support"
		default n
		help
		  This option enables ATH memory debug support.

	config PACKAGE_MAC80211_DEBUGFS
		bool "Export mac80211 internals in DebugFS"
		select KERNEL_DEBUG_FS
		default y
		help
		  Select this to see extensive information about
		  the internal state of mac80211 in debugfs.

	config PACKAGE_MAC80211_TRACING
		bool "Enable tracing (mac80211 and supported drivers)"
		select KERNEL_FTRACE
		select KERNEL_ENABLE_DEFAULT_TRACERS
		default n
		help
		  Select this to enable tracing of mac80211 and
		  related wifi drivers (using trace-cmd).

	config PACKAGE_MAC80211_MESH
		bool "Enable 802.11s mesh support"
		default y

	config PACKAGE_MAC80211_ATHDEBUG
                bool "Enable athdebug module support"
                default y
                help
                  This option enables ATHDEBUG module support.

	config PACKAGE_QCN_EXTN
		bool "Enable QCN extensions support"
		default y

  endif
endef

define KernelPackage/mac80211/description
Generic IEEE 802.11 Networking Stack (mac80211)
endef

define KernelPackage/mac80211-hwsim
  $(call KernelPackage/mac80211/Default)
  TITLE:=mac80211 HW simulation device
  DEPENDS+= +kmod-mac80211 +@DRIVER_11AX_SUPPORT +@DRIVER_11AC_SUPPORT
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/virtual/mac80211_hwsim.ko
  AUTOLOAD:=$(call AutoProbe,mac80211_hwsim)
endef

define KernelPackage/wlcore
  $(call KernelPackage/mac80211/Default)
  TITLE:=TI common driver part
  DEPENDS+= +kmod-mmc +kmod-mac80211
  FILES:= \
	$(PKG_BUILD_DIR)/drivers/net/wireless/ti/wlcore/wlcore.ko \
	$(PKG_BUILD_DIR)/drivers/net/wireless/ti/wlcore/wlcore_sdio.ko
  AUTOLOAD:=$(call AutoProbe,wlcore wlcore_sdio)
endef

ifdef CONFIG_PACKAGE_MAC80211_DEBUGFS
  config-y += \
	CFG80211_DEBUGFS \
	MAC80211_DEBUGFS
endif

ifdef CONFIG_PACKAGE_MAC80211_TRACING
  config-y += \
	IWLWIFI_DEVICE_TRACING
endif

config-$(CONFIG_PACKAGE_MAC80211_PPE_SUPPORT) += MAC80211_PPE_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_DS_SUPPORT) += MAC80211_DS_SUPPORT ATH12K_PPE_DS_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_SFE_SUPPORT) += MAC80211_SFE_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_MESSAGE_TRACING) += MAC80211_MESSAGE_TRACING ATH10K_TRACING ATH11K_TRACING ATH12K_TRACING
config-$(CONFIG_PACKAGE_MAC80211_DEBUG_MENU) += MAC80211_DEBUG_MENU
config-$(CONFIG_PACKAGE_MAC80211_VERBOSE_DEBUG) += MAC80211_VERBOSE_DEBUG
config-$(CONFIG_PACKAGE_MAC80211_PS_DEBUG) += MAC80211_PS_DEBUG
config-$(CONFIG_PACKAGE_MAC80211_ATHMEMDEBUG) += MAC80211_ATHMEMDEBUG
config-$(CONFIG_PACKAGE_QCN_EXTN) += QCN_EXTN

config-$(call config_package,mac80211-hwsim) += MAC80211_HWSIM

config-$(CONFIG_PACKAGE_MAC80211_ATHDEBUG) += ATHDEBUG

config-y += WL_TI WILINK_PLATFORM_DATA

config-$(CONFIG_LEDS_TRIGGERS) += MAC80211_LEDS

C_DEFINES=

ifeq ($(BUILD_VARIANT),smallbuffers)
	C_DEFINES+= -DCONFIG_ATH10K_SMALLBUFFERS
endif

MAKE_OPTS:= \
	$(subst -C $(LINUX_DIR),-C "$(PKG_BUILD_DIR)",$(KERNEL_MAKEOPTS)) \
	EXTRA_CFLAGS="-I$(PKG_BUILD_DIR)/include $(IREMAP_CFLAGS) $(C_DEFINES) -I$(STAGING_DIR)/usr/include/qca-nss-drv -I$(STAGING_DIR)/usr/include/qca-nss-ppe -I$(STAGING_DIR)/usr/include/qca-nss-clients -I$(PKG_BUILD_DIR)/include/telemetry_agent/inc/ -Wall" \
	KLIB_BUILD="$(LINUX_DIR)" \
	MODPROBE=true \
	KLIB=$(TARGET_MODULES_DIR) \
	KERNEL_SUBLEVEL=$(lastword $(subst ., ,$(KERNEL_PATCHVER))) \
	KBUILD_LDFLAGS_MODULE_PREREQ=

define ConfigVars
$(subst $(space),,$(foreach opt,$(config-$(1)),CPTCFG_$(opt)=$(1)
))
endef

define mac80211_config
$(call ConfigVars,m)$(call ConfigVars,y)
endef
$(eval $(call shexport,mac80211_config))

SPATCH?=0
define Build/Prepare
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
ifdef CONFIG_PACKAGE_QCN_EXTN
	$(CP) ./wlan-open-extns/subsys/src $(PKG_BUILD_DIR)/net/mac80211/qcn_extns
	$(CP) ./wlan-open-extns/ath/ath12k/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qcn_extns
	$(CP) ./wlan-open-extns/ath/wifi7/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/qcn_extns
endif
ifneq ($(CONFIG_DEBUG_MEM_USAGE),y)
 ifneq ($(CONFIG_PACKAGE_MAC80211_ATHMEMDEBUG),y)
  ifeq ($(CONFIG_PACKAGE_MAC80211_ATHDEBUG),y)
    ifeq ($(SPATCH),1)
	$(Build/refactor)
    else
	$(Build/Patch)
    endif
  endif
 endif
endif

endef

ifneq ($(CONFIG_PACKAGE_kmod-cfg80211),)
 define Build/Compile/kmod
	rm -rf $(PKG_BUILD_DIR)/modules
	+$(MAKE) $(PKG_JOBS) $(MAKE_OPTS) modules
 endef
endif

EXTERNAL_PATCH_DIR:=$(CURDIR)/patches-qca
PATCH_TIP_DIR:=$(CURDIR)/patches-tip

define Build/Patch
	$(if $(QUILT),rm -rf $(PKG_BUILD_DIR)/patches; mkdir -p $(PKG_BUILD_DIR)/patches)
	$(call PatchDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath12k,ath12k/)
	$(if $(QUILT),touch $(PKG_BUILD_DIR)/.quilt_used)
endef

define Quilt/Refresh/Package
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/build,build/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/subsys,subsys/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath,ath/)
#	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath5k,ath5k/)
#	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath9k,ath9k/)
#	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath10k,ath10k/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath11k,ath11k/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath12k,ath12k/)
endef

define Build/refactor
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ahb.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ce.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/core.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/coredump.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/debugfs_htt_stats.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_htt.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_mon.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_peer.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dbring.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/debugfs.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_rx.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/mac.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/peer.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qmi.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/reg.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wmi.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wow.c
	spatch -cocci_file alloc.cocci --in-place -dir $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7
endef

define Build/Compile
	$(SH_FUNC) var2file "$(call shvar,mac80211_config)" $(PKG_BUILD_DIR)/.config
	$(MAKE) $(MAKE_OPTS) allnoconfig
	$(call Build/Compile/kmod)
endef

define Build/InstallDev
	mkdir -p \
		$(1)/usr/include/mac80211 \
		$(1)/usr/include/mac80211-backport \
		$(1)/usr/include/mac80211/ath \
		$(1)/usr/include/net/mac80211
	$(CP) $(PKG_BUILD_DIR)/net/mac80211/*.h $(PKG_BUILD_DIR)/include/* $(1)/usr/include/mac80211/
	$(CP) $(PKG_BUILD_DIR)/backport-include/* $(1)/usr/include/mac80211-backport/
	$(CP) $(PKG_BUILD_DIR)/net/mac80211/rate.h $(1)/usr/include/net/mac80211/
	$(CP) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/*.h $(1)/usr/include/mac80211/ath/
	rm -f $(1)/usr/include/mac80211-backport/linux/module.h
endef

define KernelPackage/ath-qca/install
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_sawf.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_fse.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_dp_accel_cfg.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ppe_public.h $(STAGING_DIR)/usr/include/
endef

define KernelPackage/ath12k-qca/install
	$(call KernelPackage/ath/install)
endef

$(eval EXT_KERNEL_DIR:=$(CONFIG_EXTERNAL_KERNEL_TREE))
IS_EXT_KERNEL_TREE_GIT:=$(shell cd $(EXT_KERNEL_DIR) && git rev-parse --is-inside-work-tree)

ifeq ($(IS_EXT_KERNEL_TREE_GIT), true)
  $(eval _LINUX_SRC:=$(CONFIG_EXTERNAL_KERNEL_TREE))
else
  _LINUX_SRC=$(CONFIG_KERNEL_GIT_CLONE_URI)
endif

ifneq ($(call qstrip,$(_LINUX_SRC)),)
# --reference option doesn't work on git trees synced with "repo", so instead
# we're manually using clone & fetch to speed up sync time
  define FastCloneKernel
	git clone file://$(_LINUX_SRC) $(2) || \
	(rm -rf $(2) && \
	git clone file://$(_LINUX_SRC) $(2))
	(cd $(2); git remote add src $(1); git fetch src)
  endef
else
  define FastCloneKernel
	GIT_NAME=$$$$(echo $(1) | sed -e 's:.*//[^/]*.::g'); \
	git clone $(1) $(2) || \
	([ -n "${CONFIG_GIT_MIRROR}" ] && \
	rm -rf $(2) && \
	git clone $(CONFIG_GIT_MIRROR)$$$$GIT_NAME $(2))
  endef
endif


ifeq (,$(wildcard $(DL_DIR)/$(PKG_SOURCE)))
$(shell [ -f "$(NFS_MIRROR_SERVER)/$(PKG_SOURCE)" ] && \
	cp "$(NFS_MIRROR_SERVER)/$(PKG_SOURCE)" "$(TOPDIR)/dl/")
endif

$(eval $(foreach drv,$(PKG_DRIVERS),$(call KernelPackage,$(drv))))
$(eval $(call KernelPackage,cfg80211))
$(eval $(call KernelPackage,mac80211))
$(eval $(call BuildPackage,ath-legacy-wifi-scripts))
