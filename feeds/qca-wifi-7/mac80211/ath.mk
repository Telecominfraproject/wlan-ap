PKG_DRIVERS += \
	ath-qca ath11k-qca ath11k-qca-ahb ath11k-qca-pci ath12k-qca carl9170 owl-loader ar5523 wil6210

PKG_CONFIG_DEPENDS += \
	CONFIG_PACKAGE_ATH_DEBUG \
	CONFIG_PACKAGE_ATH_SPECTRAL \
	CONFIG_ATH11K_THERMAL \
	CONFIG_ATH_USER_REGD

ifdef CONFIG_PACKAGE_MAC80211_DEBUGFS
  config-y += \
	ATH11K_DEBUGFS \
	ATH11K_SPECTRAL \
	ATH11K_PKTLOG \
	ATH11K_CFR \
	ATH11K_SMART_ANT_ALG \
	ATH12K_DEBUGFS \
	ATH12K_CFR \
	ATH12K_POWER_BOOST \
	ATH12K_PKTLOG \
	CARL9170_DEBUGFS \
	WIL6210_DEBUGFS
endif

ifdef CONFIG_PACKAGE_MAC80211_TRACING
  config-y += \
	ATH11K_TRACING \
	ATH12K_TRACING \
	ATH_TRACEPOINTS \
	WIL6210_TRACING
endif

config-$(call config_package,ath-qca,regular smallbuffers) += ATH_CARDS ATH_COMMON
config-$(CONFIG_PACKAGE_ATH_DEBUG) += ATH_DEBUG ATH11K_DEBUG ATH12K_DEBUG
config-$(CONFIG_PACKAGE_ATH_SPECTRAL) += ATH11K_SPECTRAL
config-$(CONFIG_ATH_USER_REGD) += ATH_USER_REGD ATH_REG_DYNAMIC_USER_REG_HINTS
config-$(CONFIG_ATH11K_THERMAL) += ATH11K_THERMAL

config-$(CONFIG_TARGET_ipq53xx) += ATH12K_AHB ATH12K_POWER_OPTIMIZATION

config-$(call config_package,ath11k-qca) += ATH11K ATH11K_AHB ATH11K_PCI
config-$(call config_package,ath12k-qca) += ATH12K ATH12K_CFR

config-$(CONFIG_PACKAGE_kmod-ath12k-qca) += ATH12K_SPECTRAL
config-$(CONFIG_PACKAGE_ATH12K_SAWF) += ATH12K_SAWF
config-$(CONFIG_PACKAGE_ATH12K_TEST_FW_FOR_11S_MLO) += ATH12K_TEST_FW_FOR_11S_MLO
ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),512)
config-y += ATH12K_MEM_PROFILE_512M
endif

config-$(call config_package,carl9170) += CARL9170
config-$(call config_package,ar5523) += AR5523
config-$(call config_package,wil6210) += WIL6210

define KernelPackage/ath-qca/config
  if PACKAGE_kmod-ath-qca
	config ATH_USER_REGD
		bool "Force Atheros drivers to respect the user's regdomain settings"
		default y
		help
		  Atheros' idea of regulatory handling is that the EEPROM of the card defines
		  the regulatory limits and the user is only allowed to restrict the settings
		  even further, even if the country allows frequencies or power levels that
		  are forbidden by the EEPROM settings.

		  Select this option if you want the driver to respect the user's decision about
		  regulatory settings.

	config PACKAGE_ATH_DEBUG
		bool "Atheros wireless debugging"
		help
		  Say Y, if you want to debug atheros wireless drivers.
		  Only ath9k & ath10k & ath11k make use of this.

	config PACKAGE_ATH_DFS
		bool "Enable DFS support"
		default y
		help
		  Dynamic frequency selection (DFS) is required for most of the 5 GHz band
		  channels in Europe, US, and Japan.

		  Select this option if you want to use such channels.

	config PACKAGE_ATH_SPECTRAL
		bool "Atheros spectral scan support"
		depends on PACKAGE_ATH_DEBUG
		select KERNEL_RELAY
		help
		  Say Y to enable access to the FFT/spectral data via debugfs.

	config PACKAGE_ATH_DYNACK
		bool "Enable Dynack support"
		depends on PACKAGE_kmod-ath9k-qca-common
		help
		  Enables support for Dynamic ACK estimation, which allows the fastest possible speed
		  at any distance automatically by increasing/decreasing the max frame ACK time for
		  the most remote station detected.  It can be enabled by using iw (iw phy0 set distance auto),
		  or by sending the NL80211_ATTR_WIPHY_DYN_ACK flag to mac80211 driver using netlink.

		  Select this option if you want to enable this feature

  endif
endef

define KernelPackage/ath-qca
  $(call KernelPackage/mac80211/Default)
  TITLE:=Atheros common driver part
  DEPENDS+= @PCI_SUPPORT||USB_SUPPORT||TARGET_ath79 +kmod-mac80211
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath.ko
  MENU:=1
endef

define KernelPackage/ath-qca/description
 This module contains some common parts needed by Atheros Wireless drivers.
endef

define KernelPackage/ath11k-qca
  $(call KernelPackage/mac80211/Default)
  TITLE:=Qualcomm 802.11ax wireless chipset support (common code)
  URL:=https://wireless.wiki.kernel.org/en/users/drivers/ath11k
  DEPENDS+= +kmod-ath-qca +@DRIVER_11AC_SUPPORT +@DRIVER_11AX_SUPPORT \
  +kmod-crypto-michael-mic +ATH11K_THERMAL:kmod-hwmon-core +ATH11K_THERMAL:kmod-thermal
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath11k/ath11k.ko \
        $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath11k/ath11k_ahb.ko \
        $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath11k/ath11k_pci.ko
  AUTOLOAD:=$(call AutoProbe,ath11k ath11k_ahb ath11k_pci)
endef

define KernelPackage/ath11k-qca/description
This module adds support for Qualcomm Technologies 802.11ax family of
chipsets.
endef

define KernelPackage/ath11k-qca/config

       config ATH11K_THERMAL
               bool "Enable thermal sensors and throttling support"
               depends on PACKAGE_kmod-ath11k-qca
               default y if TARGET_ipq807x

endef

define KernelPackage/ath12k-qca
  $(call KernelPackage/mac80211/Default)
  TITLE:=QTI 802.11be wireless cards support
  URL:=https://wireless.wiki.kernel.org/en/users/drivers/ath12k
  DEPENDS+= +kmod-ath-qca +@DRIVER_11N_SUPPORT +@DRIVER_11W_SUPPORT +@DRIVER_11AC_SUPPORT +@DRIVER_11AX_SUPPORT +kmod-hwmon-core
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath12k.ko \
         $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/ath12k_wifi7.ko

ifeq ($(CONFIG_PACKAGE_MAC80211_ATHDEBUG),y)
  FILES+=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath_debug/ath_debug.ko
endif
  AUTOLOAD:=$(call AutoProbe,ath12k ath12k_wifi7 ath_debug)
endef
  MODPARAMS.ath12k:=frame_mode=1 cold_boot_cal=0

define KernelPackage/ath12k-qca/description
This module adds support for Qualcomm Technologies 802.11ax family of
chipsets.
endef

define KernelPackage/ath12k-qca/config
	config PACKAGE_ATH12K_SAWF
		bool "SAWF and Telemetry support in ATH12K"
		default y
		help
			This option enables support for SAWF and Telemetry
			in ATH12K.

	config PACKAGE_ATH12K_TEST_FW_FOR_11S_MLO
		bool "Enable 11S MLO host test Framework in ATH12K"
		default n
		help
			This option enables support for 11S MLO Host Test Framework
			in ATH12K.
endef

define KernelPackage/ath11k-qca-ahb
  $(call KernelPackage/mac80211/Default)
  TITLE:=Qualcomm 802.11ax AHB wireless chipset support
  URL:=https://wireless.wiki.kernel.org/en/users/drivers/ath11k
  DEPENDS+= @TARGET_ipq807x +kmod-ath11k-qca +kmod-qrtr-smd
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath11k/ath11k_ahb.ko
  AUTOLOAD:=$(call AutoProbe,ath11k_ahb)
endef

define KernelPackage/ath11k-qca-ahb/description
This module adds support for Qualcomm Technologies 802.11ax family of
chipsets with AHB bus.
endef

define KernelPackage/ath11k-qca-pci
  $(call KernelPackage/mac80211/Default)
  TITLE:=Qualcomm 802.11ax PCI wireless chipset support
  URL:=https://wireless.wiki.kernel.org/en/users/drivers/ath11k
  DEPENDS+= @PCI_SUPPORT +kmod-qrtr-mhi +kmod-ath11k-qca
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath11k/ath11k_pci.ko
  AUTOLOAD:=$(call AutoProbe,ath11k_pci)
endef

define KernelPackage/ath11k-qca-pci/description
This module adds support for Qualcomm Technologies 802.11ax family of
chipsets with PCI bus.
endef

define KernelPackage/carl9170
  $(call KernelPackage/mac80211/Default)
  TITLE:=Driver for Atheros AR9170 USB sticks
  DEPENDS:=@USB_SUPPORT +kmod-mac80211 +kmod-ath +kmod-usb-core +kmod-input-core +carl9170-firmware
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/carl9170/carl9170.ko
  AUTOLOAD:=$(call AutoProbe,carl9170)
endef

define KernelPackage/owl-loader
  $(call KernelPackage/mac80211/Default)
  TITLE:=Owl loader for initializing Atheros PCI(e) Wifi chips
  DEPENDS:=@PCI_SUPPORT +kmod-ath9k-qca
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath9k/ath9k_pci_owl_loader.ko
  AUTOLOAD:=$(call AutoProbe,ath9k_pci_owl_loader)
endef

define KernelPackage/owl-loader/description
  Kernel module that helps to initialize certain Qualcomm
  Atheros' PCI(e) Wifi chips, which have the init data
  (which contains the PCI device ID for example) stored
  together with the calibration data in the file system.

  This is necessary for devices like the Cisco Meraki Z1.
endef

define KernelPackage/ar5523
  $(call KernelPackage/mac80211/Default)
  TITLE:=Driver for Atheros AR5523 USB sticks
  DEPENDS:=@USB_SUPPORT +kmod-mac80211 +kmod-ath-qca +kmod-usb-core +kmod-input-core
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ar5523/ar5523.ko
  AUTOLOAD:=$(call AutoProbe,ar5523)
endef

define KernelPackage/wil6210
  $(call KernelPackage/mac80211/Default)
  TITLE:=QCA/Wilocity 60g WiFi card wil6210 support
  DEPENDS+= @PCI_SUPPORT +kmod-mac80211 +wil6210-firmware
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/ath/wil6210/wil6210.ko
  AUTOLOAD:=$(call AutoProbe,wil6210)
endef
