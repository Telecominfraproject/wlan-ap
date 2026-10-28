#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the 3-Clause BSD License.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=hostapd_s1g
PKG_RELEASE=1

PKG_LICENSE:=BSD-3-Clause

PKG_VERSION:=mm6108-2.0.1

PKG_SOURCE_URL:=https://github.com/MorseMicro/hostap.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=70cfa617f96e1176ce7c50debb30ecd74fc3e20d
PKG_SOURCE_MIRROR:=0
PKG_MIRROR_HASH:=23e212617736fe88282b6f0b788a8e47234116e14cc7969197f60610389ed0f8

# ./src/ holds the ubus sources the mm61x patches build on. OpenWrt copies it
# into the build directory before applying the patches.
PATCH_DIR:=./patches/mm61x

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

PKG_CONFIG_DEPENDS:= \
	CONFIG_MORSE_HOSTAPD_S1G_ACS \
	CONFIG_MORSE_HOSTAPD_S1G_EAP \
	CONFIG_MORSE_WPA_SUPPLICANT_S1G_MESH_NETWORKING \
	CONFIG_MORSE_WPA_SUPPLICANT_S1G_EAP \
	CONFIG_MORSE_WPA_SUPPLICANT_S1G_DBUS

include $(INCLUDE_DIR)/package.mk

define Package/hostapd_s1g
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=Morse Micro HaLow hostapd
  DEPENDS:= +kmod-morse +kmod-cfg80211 +morsecli +libnl +libopenssl +libubus +libblobmsg-json
  USERID:=network=101:network=101
  PROVIDES:=hostapd_s1g
endef

define Package/hostapd_s1g/description
 This package contains a full featured Morse Micro patched
 Authenticator.
endef

define Package/hostapd_s1g/config
	if PACKAGE_hostapd_s1g

		config MORSE_HOSTAPD_S1G_EAP
			bool
			default n
			prompt "Enable Extensible Authentication Protocol (EAP) support"

		config MORSE_HOSTAPD_S1G_ACS
			bool
			default n
			prompt "Enable automatic channel selection (ACS) support"
			help
				This will allow hostapd to pick the channel automatically when channel is set
				to "acs_survey" or "0". Eventually, other ACS algorithms can be added in
				similar way.

	endif
endef

define Package/wpa_supplicant_s1g
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=Morse Micro HaLow wpa_supplicant
  DEPENDS:= +kmod-morse +kmod-cfg80211 +morsecli +libnl +libopenssl +libubus +libblobmsg-json \
	+MORSE_WPA_SUPPLICANT_S1G_DBUS:dbus +MORSE_WPA_SUPPLICANT_S1G_DBUS:libdbus
  USERID:=network=101:network=101
  PROVIDES:=wpa_supplicant_s1g
endef

define Package/wpa_supplicant_s1g/config
	if PACKAGE_wpa_supplicant_s1g

		config MORSE_WPA_SUPPLICANT_S1G_MESH_NETWORKING
			bool
			default n
			prompt "Enable mesh networking support"

		config MORSE_WPA_SUPPLICANT_S1G_EAP
			bool
			default n
			prompt "Enable Extensible Authentication Protocol (EAP) support"

		config MORSE_WPA_SUPPLICANT_S1G_DBUS
			bool
			default n
			prompt "Enable dbus support"

	endif
endef

define Package/libwpa_client_s1g
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=libwpa_client_s1g
  DEPENDS:=wpa_supplicant_s1g
endef

define Package/libwpa_client_s1g/description
  wpa_supplicant_s1g client library
endef

MAKE_FLAGS += \
	MORSE_VERSION=$(PKG_VERSION)

MMHOSTAPD_CONFIG_ENABLE = \
	NEED_LINUX_IOCTL \
	CONFIG_INTERNAL_LIBTOMMATH \
	CONFIG_DEBUG_FILE \
	CONFIG_DEBUG_SYSLOG \
	CONFIG_PMKSA_CACHE_EXTERNAL \
	CONFIG_SAE \
	CONFIG_SAE_PK \
	CONFIG_TESTING_OPTIONS \
	CONFIG_WNM \
	CONFIG_DRIVER_ACS \
	CONFIG_DRIVER_HOSTAP \
	CONFIG_DRIVER_NL80211 \
	CONFIG_DRIVER_NL80211_QCA \
	CONFIG_HS20 \
	CONFIG_IEEE80211AC \
	CONFIG_IEEE80211N \
	CONFIG_IEEE80211AH \
	CONFIG_IEEE80211R \
	CONFIG_INTERWORKING \
	CONFIG_FULL_DYNAMIC_VLAN \
	CONFIG_VLAN_NETLINK \
	CONFIG_OWE \
	CONFIG_LIBNL32 \
	CONFIG_DPP \
	CONFIG_DPP2 \
	CONFIG_DPP3 \
	CONFIG_WPS \
	CONFIG_WPS_UPNP \
	CONFIG_WPS_NFC

MMHOSTAPD_CONFIG_SET = \
	CONFIG_UBUS

MMHOSTAPD_CONFIG_DISABLE = \
    CONFIG_DRIVER_NONE \
	CONFIG_DRIVER_RTW \
	CONFIG_DRIVER_WIRED

MMHOSTAPD_CONFIG_EDITS += 's/\#\(CONFIG_TLS=openssl\)/\1/'

ifeq ($(CONFIG_MORSE_HOSTAPD_S1G_ACS),y)
	MMHOSTAPD_CONFIG_ENABLE += CONFIG_ACS
endif

ifeq ($(CONFIG_MORSE_HOSTAPD_S1G_EAP),y)
	MMHOSTAPD_CONFIG_EDITS += 's/\#\(CONFIG_EAP\)/\1/'
	MMHOSTAPD_CONFIG_ENABLE += \
		CONFIG_RADIUS_SERVER \
		CONFIG_TLSV1 \
		CONFIG_TLSV11 \
		CONFIG_TLSV12
else
	MMHOSTAPD_CONFIG_DISABLE += CONFIG_HOSTAPD_EAP
	MMHOSTAPD_CONFIG_ENABLE += \
		CONFIG_NO_ACCOUNTING \
		CONFIG_NO_RADIUS
endif

MMWPA_SUPPLICANT_CONFIG_SET = \
	CONFIG_UBUS

MMWPA_SUPPLICANT_CONFIG_ENABLE = \
	CONFIG_IEEE80211AC \
	CONFIG_IEEE80211N \
	CONFIG_IEEE80211R \
	CONFIG_INTERNAL_LIBTOMMATH \
	CONFIG_DEBUG_FILE \
	CONFIG_MATCH_IFACE \
	CONFIG_LIBNL32 \
	CONFIG_DRIVER_WEXT \
	CONFIG_DRIVER_NL80211 \
	CONFIG_DRIVER_NL80211_QCA \
	CONFIG_DRIVER_WIRED \
	CONFIG_IEEE8021X_EAPOL \
	CONFIG_WPS \
	CONFIG_WPS_ER \
	CONFIG_WPS_NFC \
	CONFIG_WPS_REG_DISABLE_OPEN \
	CONFIG_PKCS12 \
	CONFIG_CTRL_IFACE \
	CONFIG_READLINE \
	CONFIG_IEEE80211W \
	CONFIG_INTERNAL_LIBTOMMATH_FAST \
	CONFIG_DEBUG_SYSLOG \
	CONFIG_DEBUG_SYSLOG_FACILITY \
	CONFIG_INTERWORKING \
	CONFIG_HS20 \
	CONFIG_MATCH_IFACE \
	CONFIG_AP \
	CONFIG_P2P \
	CONFIG_IBSS_RSN \
	CONFIG_OWE \
	CONFIG_DPP \
	CONFIG_DPP2 \
	CONFIG_DPP3 \
	CONFIG_SAE \
	CONFIG_SAE_PK \
	CONFIG_TESTING_OPTIONS \
	CONFIG_PMKSA_CACHE_EXTERNAL \
	CONFIG_WNM \
	CONFIG_IPV6

MMWPA_SUPPLICANT_CONFIG_DISABLE = \
	CONFIG_BGSCAN_SIMPLE \
	CONFIG_SMARTCARD \
	CONFIG_READLINE

MMWPA_SUPPLICANT_CONFIG_EDITS += 's/\#\(CONFIG_TLS=openssl\)/\1/'

ifeq ($(CONFIG_MORSE_WPA_SUPPLICANT_S1G_MESH_NETWORKING),y)
	MMWPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_MESH
endif

ifeq ($(CONFIG_MORSE_WPA_SUPPLICANT_S1G_EAP),y)
	MMWPA_SUPPLICANT_CONFIG_ENABLE += \
		CONFIG_EAP_PEAP \
		CONFIG_EAP_MD5 \
		CONFIG_EAP_MSCHAPV2 \
		CONFIG_EAP_TLS \
		CONFIG_EAP_TLSV1_3 \
		CONFIG_EAP_TTLS \
		CONFIG_EAP_GTC \
		CONFIG_EAP_PWD
endif

ifeq ($(CONFIG_MORSE_WPA_SUPPLICANT_S1G_DBUS),y)
	MMWPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_CTRL_IFACE_DBUS_NEW
endif

MMWPA_SUPPLICANT_CONFIG = $(PKG_BUILD_DIR)/wpa_supplicant/.config
MMHOSTAPD_CONFIG = $(PKG_BUILD_DIR)/hostapd/.config

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/defconfig $(MMWPA_SUPPLICANT_CONFIG)
	sed -i $(patsubst %,-e 's/^#\(%=\)/\1/',$(MMWPA_SUPPLICANT_CONFIG_ENABLE)) \
		$(patsubst %,-e 's/^\(%=\)/#\1/',$(MMWPA_SUPPLICANT_CONFIG_DISABLE)) \
		$(patsubst %,-e '1i%=y',$(MMWPA_SUPPLICANT_CONFIG_SET)) \
		$(patsubst %,-e %,$(MMWPA_SUPPLICANT_CONFIG_EDITS)) \
		$(MMWPA_SUPPLICANT_CONFIG)
	$(CP) $(PKG_BUILD_DIR)/hostapd/defconfig $(MMHOSTAPD_CONFIG)
	sed -i $(patsubst %,-e 's/^#\(%=\)/\1/',$(MMHOSTAPD_CONFIG_ENABLE)) \
		$(patsubst %,-e 's/^\(%=\)/#\1/',$(MMHOSTAPD_CONFIG_DISABLE)) \
		$(patsubst %,-e '1i%=y',$(MMHOSTAPD_CONFIG_SET)) \
		$(patsubst %,-e %,$(MMHOSTAPD_CONFIG_EDITS)) \
		$(MMHOSTAPD_CONFIG)
endef

TARGET_CPPFLAGS := \
	-I$(STAGING_DIR)/usr/include/libnl3 \
	-I$(PKG_BUILD_DIR)/src/crypto \
	$(TARGET_CPPFLAGS) \
	-DCONFIG_LIBNL20 \
	-D_GNU_SOURCE

TARGET_LDFLAGS += -lnl-3 -lnl-genl-3 -lnl-route-3
TARGET_LDFLAGS += -lm -lpthread -lcrypto -lssl

TARGET_LDFLAGS += -lrt
TARGET_LDFLAGS_C += -lrt

TARGET_LDFLAGS += -lubox -lubus -lblobmsg_json -lm

TARGET_CFLAGS += -Wno-deprecated-declarations

define Build/RunMake
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) $(MAKE_FLAGS) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/$(1) \
		$(TARGET_CONFIGURE_OPTS) \
		$(DRIVER_MAKEOPTS) \
		LIBS="$(TARGET_LDFLAGS)" \
		LIBS_c="$(TARGET_LDFLAGS_C)" \
		AR="$(TARGET_CROSS)gcc-ar" \
		BCHECK= \
		$(2)
endef

define Build/Compile
	+$(call Build/RunMake,hostapd, \
		hostapd_s1g hostapd_cli_s1g \
	)
	+$(call Build/RunMake,wpa_supplicant, \
		wpa_supplicant_s1g wpa_cli_s1g \
	)
	+$(call Build/RunMake,wpa_supplicant, libwpa_client_s1g.so)
endef

# The /sbin links keep callers that use the old binary location working.
define Package/hostapd_s1g/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_s1g $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_cli_s1g $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/sbin
	$(LN) ../usr/sbin/hostapd_s1g $(1)/sbin/hostapd_s1g
	$(LN) ../usr/sbin/hostapd_cli_s1g $(1)/sbin/hostapd_cli_s1g
endef

define Package/wpa_supplicant_s1g/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant_s1g $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_cli_s1g $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/sbin
	$(LN) ../usr/sbin/wpa_supplicant_s1g $(1)/sbin/wpa_supplicant_s1g
	$(LN) ../usr/sbin/wpa_cli_s1g $(1)/sbin/wpa_cli_s1g

ifeq ($(CONFIG_MORSE_WPA_SUPPLICANT_S1G_DBUS),y)
	$(INSTALL_DIR) $(1)/etc/dbus-1/system.d/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/wpa_supplicant/dbus/dbus-wpa_supplicant_s1g.conf $(1)/etc/dbus-1/system.d/wpa_supplicant_s1g.conf
endif
endef

define Package/libwpa_client_s1g/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/libwpa_client_s1g.so $(1)/usr/lib/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/libwpa_client_s1g.so		$(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/src/common/wpa_ctrl.h				$(1)/usr/include/wpa_ctrl.h
endef

$(eval $(call BuildPackage,hostapd_s1g))
$(eval $(call BuildPackage,wpa_supplicant_s1g))
$(eval $(call BuildPackage,libwpa_client_s1g))
