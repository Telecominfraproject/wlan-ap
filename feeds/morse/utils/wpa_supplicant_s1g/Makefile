#
# Copyright 2022 Morse Micro
#
# This is free software, licensed under the 3-Clause BSD License.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wpa_supplicant_s1g
PKG_RELEASE=2

PKG_VERSION:=1.12.4

PKG_LICENSE:=BSD-3-Clause

PKG_SOURCE_VERSION:=$(PKG_VERSION)
PKG_SOURCE_URL:=https://github.com/MorseMicro/hostap.git
PKG_HASH:=c342d1489d03363b79b5b4e185ccbceca53aab9e634daefb81180ad1012b77b7
PKG_SOURCE_PROTO:=git

PKG_MAINTAINER:=Morse Micro <info@morsemicro.com>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

MAKE_FLAGS += \
	MORSEWPA_SUPPLICANT_VERSION_STRING=$(PKG_VERSION)

# Add support for simple background scan
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
	CONFIG_DRIVER_WIRED \
	CONFIG_IEEE8021X_EAPOL \
	CONFIG_WPS \
	CONFIG_PKCS12 \
	CONFIG_CTRL_IFACE \
	CONFIG_READLINE \
	CONFIG_IEEE80211W \
	CONFIG_INTERNAL_LIBTOMMATH_FAST \
	CONFIG_CTRL_IFACE_DBUS_INTRO \
	CONFIG_DEBUG_SYSLOG \
	CONFIG_INTERWORKING \
	CONFIG_HS20 \
	CONFIG_MATCH_IFACE \
	CONFIG_AP \
	CONFIG_P2P \
	CONFIG_IBSS_RSN \
	CONFIG_OWE \
	CONFIG_DPP \
	CONFIG_SAE \
	CONFIG_TESTING_OPTIONS \
	CONFIG_PMKSA_CACHE_EXTERNAL \
	CONFIG_WNM

MMWPA_SUPPLICANT_CONFIG_DISABLE = \
	CONFIG_BGSCAN_SIMPLE \
	CONFIG_SMARTCARD \
	CONFIG_CTRL_IFACE_DBUS_NEW \
	CONFIG_CTRL_IFACE_DBUS_INTRO \
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
		CONFIG_EAP_TTLS \
		CONFIG_EAP_GTC \
		CONFIG_EAP_PWD
endif

define Package/wpa_supplicant_s1g
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=Morse Micro HaLow wpa_supplicant
  DEPENDS:= +kmod-morse +libnl +libopenssl +libubus
  USERID:=network=101:network=101
  PROVIDES:=wpa_supplicant_s1g
endef

define Package/libwpa_client
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=libwpa_client
  DEPENDS:=wpa_supplicant_s1g
endef

define Package/libwpa_client/description
  wpa_supplicant_s1g client liberary
endef

define Package/wpa_supplicant_s1g/config
    source "$(SOURCE)/Config.in"
endef

MMWPA_SUPPLICANT_CONFIG = $(PKG_BUILD_DIR)/wpa_supplicant/.config

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/defconfig $(MMWPA_SUPPLICANT_CONFIG)
	sed -i $(patsubst %,-e 's/^#\(%\)/\1/',$(MMWPA_SUPPLICANT_CONFIG_ENABLE)) \
		$(patsubst %,-e 's/^\(%\)/#\1/',$(MMWPA_SUPPLICANT_CONFIG_DISABLE)) \
		$(patsubst %,-e '1i%=y',$(MMWPA_SUPPLICANT_CONFIG_SET)) \
		$(patsubst %,-e %,$(MMWPA_SUPPLICANT_CONFIG_EDITS)) \
		$(MMWPA_SUPPLICANT_CONFIG)
endef

TARGET_CPPFLAGS := \
	-I$(STAGING_DIR)/usr/include/libnl3 \
	-I$(PKG_BUILD_DIR)/src/crypto \
	$(TARGET_CPPFLAGS) \
	-DCONFIG_LIBNL20 \
	-D_GNU_SOURCE

TARGET_LDFLAGS += -lnl-3 -lnl-genl-3 -lnl-route-3
TARGET_LDFLAGS += -lm -lpthread -lcrypto -lssl

define Build/RunMake
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/$(1) \
		$(TARGET_CONFIGURE_OPTS) \
		$(DRIVER_MAKEOPTS) \
		LIBS="$(TARGET_LDFLAGS)" \
		LIBS_c="$(TARGET_LDFLAGS_C)" \
		AR="$(TARGET_CROSS)gcc-ar" \
		BCHECK= \
		$(2)
endef

define Build/Compile
	+$(call Build/RunMake,wpa_supplicant, \
		wpa_supplicant_s1g wpa_cli_s1g \
	)
	+$(call Build/RunMake,wpa_supplicant, libwpa_client.so)
endef

define Package/wpa_supplicant_s1g/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant_s1g $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_cli_s1g $(1)/sbin/
endef

define Package/libwpa_client/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/libwpa_client.so $(1)/usr/lib/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/libwpa_client.so		$(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/src/common/wpa_ctrl.h				$(1)/usr/include/wpa_ctrl.h
endef

$(eval $(call BuildPackage,wpa_supplicant_s1g))
$(eval $(call BuildPackage,libwpa_client))
