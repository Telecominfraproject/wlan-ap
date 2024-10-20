# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2006-2021 OpenWrt.org

include $(TOPDIR)/rules.mk

PKG_NAME:=hostapd-qca
PKG_RELEASE:=6

PKG_SOURCE_URL:=http://w1.fi/hostap.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2023-06-22
PKG_SOURCE_VERSION:=599d00be9de2846c6ea18c1487d8329522ade22b
PKG_MIRROR_HASH:=c05a31e0b7481544f99ea494676a43497403a918aeeb4121b9c7fc488f7df2b6

PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>
PKG_LICENSE:=BSD-3-Clause
PKG_CPE_ID:=cpe:/a:w1.fi:hostapd

PKG_BUILD_PARALLEL:=1
PKG_ASLR_PIE_REGULAR:=1

PKG_CONFIG_DEPENDS:= \
	CONFIG_PACKAGE_kmod-ath9k-qca \
	CONFIG_PACKAGE_kmod-cfg80211-qca \
	CONFIG_PACKAGE_hostapd-qca \
	CONFIG_PACKAGE_hostapd-qca-basic \
	CONFIG_PACKAGE_hostapd-qca-mini \
	CONFIG_WPA_RFKILL_SUPPORT \
	CONFIG_DRIVER_WEXT_SUPPORT \
	CONFIG_DRIVER_11AC_SUPPORT \
	CONFIG_DRIVER_11AX_SUPPORT \
	CONFIG_WPA_ENABLE_WEP

PKG_BUILD_FLAGS:=gc-sections lto

EAPOL_TEST_PROVIDERS:=eapol-test-qca eapol-test-qca-openssl eapol-test-qca-wolfssl

SUPPLICANT_PROVIDERS:=
HOSTAPD_PROVIDERS:=

LOCAL_TYPE=$(strip \
		$(if $(findstring wpad-qca,$(BUILD_VARIANT)),wpad-qca, \
		$(if $(findstring supplicant-qca,$(BUILD_VARIANT)),supplicant-qca, \
		hostapd-qca \
		)))

LOCAL_AND_LIB_VARIANT=$(patsubst hostapd-qca-%,%,\
		      $(patsubst wpad-qca-%,%,\
		      $(patsubst supplicant-qca-%,%,\
		      $(BUILD_VARIANT)\
		      )))

LOCAL_VARIANT=$(patsubst %-internal,%,\
	      $(patsubst %-openssl,%,\
	      $(patsubst %-wolfssl,%,\
	      $(patsubst %-mbedtls,%,\
	      $(LOCAL_AND_LIB_VARIANT)\
	      ))))

SSL_VARIANT=$(strip \
		$(if $(findstring openssl,$(LOCAL_AND_LIB_VARIANT)),openssl,\
		$(if $(findstring wolfssl,$(LOCAL_AND_LIB_VARIANT)),wolfssl,\
		$(if $(findstring mbedtls,$(LOCAL_AND_LIB_VARIANT)),mbedtls,\
		internal\
		))))

CONFIG_VARIANT:=$(LOCAL_VARIANT)
ifeq ($(LOCAL_VARIANT),mesh)
  CONFIG_VARIANT:=full
endif

include $(INCLUDE_DIR)/package.mk

STAMP_CONFIGURED:=$(STAMP_CONFIGURED)_$(CONFIG_WPA_MSG_MIN_PRIORITY)

ifneq ($(CONFIG_DRIVER_11AC_SUPPORT),)
  HOSTAPD_IEEE80211AC:=y
endif

ifneq ($(CONFIG_DRIVER_11AX_SUPPORT),)
  HOSTAPD_IEEE80211AX:=y
endif

DRIVER_MAKEOPTS= \
	CONFIG_ACS=$(CONFIG_PACKAGE_kmod-cfg80211-qca) \
	CONFIG_DRIVER_NL80211=$(CONFIG_PACKAGE_kmod-cfg80211-qca) \
	CONFIG_IEEE80211AC=$(HOSTAPD_IEEE80211AC) \
	CONFIG_IEEE80211AX=$(HOSTAPD_IEEE80211AX) \
	CONFIG_DRIVER_WEXT=$(CONFIG_DRIVER_WEXT_SUPPORT) \
	CONFIG_MBO=$(CONFIG_WPA_MBO_SUPPORT)

ifeq ($(SSL_VARIANT),openssl)
  DRIVER_MAKEOPTS += CONFIG_TLS=openssl CONFIG_SAE=y
  TARGET_LDFLAGS += -lcrypto -lssl

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y
  endif
endif

ifeq ($(SSL_VARIANT),wolfssl)
  DRIVER_MAKEOPTS += CONFIG_TLS=wolfssl CONFIG_SAE=y
  TARGET_LDFLAGS += -lwolfssl

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
endif

ifeq ($(SSL_VARIANT),mbedtls)
  DRIVER_MAKEOPTS += CONFIG_TLS=mbedtls CONFIG_SAE=y
  TARGET_LDFLAGS += -lmbedcrypto -lmbedx509 -lmbedtls

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
endif

ifneq ($(LOCAL_TYPE),hostapd-qca)
  ifdef CONFIG_WPA_RFKILL_SUPPORT
    DRIVER_MAKEOPTS += NEED_RFKILL=y
  endif
endif

DRV_DEPENDS:=+PACKAGE_kmod-cfg80211-qca:libnl


define Package/hostapd-qca/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Authenticator
  URL:=http://hostap.epitest.fi/
  DEPENDS:=$(DRV_DEPENDS) +hostapd-qca-common +libubus
  EXTRA_DEPENDS:=hostapd-qca-common (=$(PKG_VERSION)-$(PKG_RELEASE))
  USERID:=network=101:network=101
  PROVIDES:=hostapd-qca
  CONFLICTS:=$(HOSTAPD_PROVIDERS)
  HOSTAPD_PROVIDERS+=$(1)
endef

define Package/hostapd-qca
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=full-internal
endef

define Package/hostapd-qca/description
 This package contains a full featured IEEE 802.1x/WPA/EAP/RADIUS
 Authenticator.
endef

define Package/hostapd-qca-openssl
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=full-openssl
  DEPENDS+=+PACKAGE_hostapd-qca-openssl:libopenssl
endef

Package/hostapd-qca-openssl/description = $(Package/hostapd-qca/description)

define Package/hostapd-qca-macsec
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (macsec)
  DEPENDS+= @TARGET_ipq||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq807x +kmod-qca-nss-macsec +libopenssl
  PROVIDES:=hostapd-qca-macsec
  CONFLICTS:=
  VARIANT:=macsec
endef

define Package/hostapd-qca-macsec/description
 This package is Hostapd Authenticator for macsec support.
endef

define Package/hostapd-qca-wolfssl
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=full-wolfssl
  DEPENDS+=+PACKAGE_hostapd-qca-wolfssl:libwolfssl
endef

Package/hostapd-qca-wolfssl/description = $(Package/hostapd-qca/description)

define Package/hostapd-qca-mbedtls
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=full-mbedtls
  DEPENDS+=+PACKAGE_hostapd-qca-mbedtls:libmbedtls
endef

Package/hostapd-qca-mbedtls/description = $(Package/hostapd-qca/description)

define Package/hostapd-qca-basic
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (WPA-PSK, 11r, 11w)
  VARIANT:=basic
endef

define Package/hostapd-qca-basic/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-qca-basic-openssl
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-openssl
  DEPENDS+=+PACKAGE_hostapd-qca-basic-openssl:libopenssl
endef

define Package/hostapd-qca-basic-openssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-qca-basic-wolfssl
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-wolfssl
  DEPENDS+=+PACKAGE_hostapd-qca-basic-wolfssl:libwolfssl
endef

define Package/hostapd-qca-basic-wolfssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-qca-basic-mbedtls
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-mbedtls
  DEPENDS+=+PACKAGE_hostapd-qca-basic-mbedtls:libmbedtls
endef

define Package/hostapd-qca-basic-mbedtls/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-qca-mini
$(call Package/hostapd-qca/Default,$(1))
  TITLE+= (WPA-PSK only)
  VARIANT:=mini
endef

define Package/hostapd-qca-mini/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator (WPA-PSK only).
endef


define Package/wpad-qca/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Auth/Supplicant
  DEPENDS:=$(DRV_DEPENDS) +hostapd-qca-common +libubus
  EXTRA_DEPENDS:=hostapd-qca-common (=$(PKG_VERSION)-$(PKG_RELEASE))
  USERID:=network=101:network=101
  URL:=http://hostap.epitest.fi/
  PROVIDES:=hostapd-qca wpa-supplicant-qca
  CONFLICTS:=$(HOSTAPD_PROVIDERS) $(SUPPLICANT_PROVIDERS)
  HOSTAPD_PROVIDERS+=$(1)
  SUPPLICANT_PROVIDERS+=$(1)
endef

define Package/wpad-qca
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=wpad-qca-full-internal
endef

define Package/wpad-qca/description
 This package contains a full featured IEEE 802.1x/WPA/EAP/RADIUS
 Authenticator and Supplicant
endef

define Package/wpad-qca-openssl
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=wpad-qca-full-openssl
  DEPENDS+=+PACKAGE_wpad-qca-openssl:libopenssl
endef

Package/wpad-qca-openssl/description = $(Package/wpad-qca/description)

define Package/wpad-qca-wolfssl
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=wpad-qca-full-wolfssl
  DEPENDS+=+PACKAGE_wpad-qca-wolfssl:libwolfssl
endef

Package/wpad-qca-wolfssl/description = $(Package/wpad-qca/description)

define Package/wpad-qca-mbedtls
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=wpad-qca-full-mbedtls
  DEPENDS+=+PACKAGE_wpad-qca-mbedtls:libmbedtls
endef

Package/wpad-qca-mbedtls/description = $(Package/wpad-qca/description)

define Package/wpad-qca-basic
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (WPA-PSK, 11r, 11w)
  VARIANT:=wpad-qca-basic
endef

define Package/wpad-qca-basic/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/wpad-qca-basic-openssl
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (OpenSSL, 11r, 11w)
  VARIANT:=wpad-basic-openssl
  DEPENDS+=+PACKAGE_wpad-qca-basic-openssl:libopenssl
endef

define Package/wpad-qca-basic-openssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-qca-basic-wolfssl
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (wolfSSL, 11r, 11w)
  VARIANT:=wpad-qca-basic-wolfssl
  DEPENDS+=+PACKAGE_wpad-qca-basic-wolfssl:libwolfssl
endef

define Package/wpad-qca-basic-wolfssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-qca-basic-mbedtls
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (mbedTLS, 11r, 11w)
  VARIANT:=wpad-qca-basic-mbedtls
  DEPENDS+=+PACKAGE_wpad-qca-basic-mbedtls:libmbedtls
endef

define Package/wpad-qca-basic-mbedtls/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-qca-mini
$(call Package/wpad-qca/Default,$(1))
  TITLE+= (WPA-PSK only)
  VARIANT:=wpad-qca-mini
endef

define Package/wpad-qca-mini/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator and Supplicant (WPA-PSK only).
endef

define Package/wpad-qca-mesh
$(call Package/wpad-qca/Default,$(1))
  DEPENDS+=@PACKAGE_kmod-cfg80211-qca @(!TARGET_uml||BROKEN)
  PROVIDES+=wpa-supplicant-qca-mesh wpad-qca-mesh
endef

define Package/wpad-qca-mesh/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator and Supplicant (with 802.11s mesh and SAE support).
endef

define Package/wpad-qca-mesh-openssl
$(call Package/wpad-qca-mesh,$(1))
  TITLE+= (OpenSSL, 11s, SAE)
  DEPENDS+=+PACKAGE_wpad-qca-mesh-openssl:libopenssl
  VARIANT:=wpad-qca-mesh-openssl
endef

Package/wpad-qca-mesh-openssl/description = $(Package/wpad-qca-mesh/description)

define Package/wpad-qca-mesh-wolfssl
$(call Package/wpad-qca-mesh,$(1))
  TITLE+= (wolfSSL, 11s, SAE)
  DEPENDS+=+PACKAGE_wpad-qca-mesh-wolfssl:libwolfssl
  VARIANT:=wpad-qca-mesh-wolfssl
endef

Package/wpad-qca-mesh-wolfssl/description = $(Package/wpad-qca-mesh/description)

define Package/wpad-qca-mesh-mbedtls
$(call Package/wpad-qca-mesh,$(1))
  TITLE+= (mbedTLS, 11s, SAE)
  DEPENDS+=+PACKAGE_wpad-qca-mesh-mbedtls:libmbedtls
  VARIANT:=wpad-qca-mesh-mbedtls
endef

Package/wpad-qca-mesh-mbedtls/description = $(Package/wpad-mesh/description)


define Package/wpa-supplicant-qca/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=WPA Supplicant
  URL:=http://hostap.epitest.fi/wpa_supplicant/
  DEPENDS:=$(DRV_DEPENDS) +hostapd-qca-common +libubus
  EXTRA_DEPENDS:=hostapd-qca-common (=$(PKG_VERSION)-$(PKG_RELEASE))
  USERID:=network=101:network=101
  PROVIDES:=wpa-supplicant-qca
  CONFLICTS:=$(SUPPLICANT_PROVIDERS)
  SUPPLICANT_PROVIDERS+=$(1)
endef

define Package/wpa-supplicant-qca
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=supplicant-qca-full-internal
endef

define Package/wpa-supplicant-qca-openssl
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=supplicant-qca-full-openssl
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-openssl:libopenssl
endef

define Package/wpa-supplicant-qca-wolfssl
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=supplicant-full-wolfssl
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-wolfssl:libwolfssl
endef

define Package/wpa-supplicant-qca-mbedtls
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=supplicant-qca-full-mbedtls
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-mbedtls:libmbedtls
endef

define Package/wpa-supplicant-qca/config
	source "$(SOURCE)/Config.in"
endef

define Package/wpa-supplicant-qca-macsec
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE:=WPA Supplicant (MACSEC)
  DEPENDS+= @TARGET_ipq||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq807x +kmod-qca-nss-macsec +libopenssl
  PROVIDES:=wpa-supplicant-qca-macsec
  CONFLICTS:=
  VARIANT:=supplicant-qca-macsec
endef

define Package/wpa-supplicant-qca-macsec/Description
  WPA Supplicant with MACSEC support.
endef

define Package/wpa-supplicant-qca-p2p
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (Wi-Fi P2P support)
  DEPENDS+=@PACKAGE_kmod-cfg80211-qca
  VARIANT:=supplicant-qca-p2p-internal
endef

define Package/wpa-supplicant-qca-mesh/Default
$(call Package/wpa-supplicant-qca/Default,$(1))
  DEPENDS+=@PACKAGE_kmod-cfg80211-qca @(!TARGET_uml||BROKEN)
  PROVIDES+=wpa-supplicant-qca-mesh
endef

define Package/wpa-supplicant-qca-mesh-openssl
$(call Package/wpa-supplicant-qca-mesh/Default,$(1))
  TITLE+= (OpenSSL, 11s, SAE)
  VARIANT:=supplicant-qca-mesh-openssl
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-mesh-openssl:libopenssl
endef

define Package/wpa-supplicant-qca-mesh-wolfssl
$(call Package/wpa-supplicant-qca-mesh/Default,$(1))
  TITLE+= (wolfSSL, 11s, SAE)
  VARIANT:=supplicant-qca-mesh-wolfssl
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-mesh-wolfssl:libwolfssl
endef

define Package/wpa-supplicant-qca-mesh-mbedtls
$(call Package/wpa-supplicant-qca-mesh/Default,$(1))
  TITLE+= (mbedTLS, 11s, SAE)
  VARIANT:=supplicant-qca-mesh-mbedtls
  DEPENDS+=+PACKAGE_wpa-supplicant-qca-mesh-mbedtls:libmbedtls
endef

define Package/wpa-supplicant-qca-basic
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (11r, 11w)
  VARIANT:=supplicant-qca-basic
endef

define Package/wpa-supplicant-qca-mini
$(call Package/wpa-supplicant-qca/Default,$(1))
  TITLE+= (minimal)
  VARIANT:=supplicant-qca-mini
endef


define Package/hostapd-qca-common
  TITLE:=hostapd/wpa_supplicant common support files
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
endef

define Package/hostapd-qca-utils
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Authenticator (utils)
  URL:=http://hostap.epitest.fi/
  DEPENDS:=@$(subst $(space),||,$(foreach pkg,$(HOSTAPD_PROVIDERS),PACKAGE_$(pkg)))
  VARIANT:=*
endef

define Package/hostapd-qca-utils/description
 This package contains a command line utility to control the
 IEEE 802.1x/WPA/EAP/RADIUS Authenticator.
endef

define Package/wpa-qca-cli
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  DEPENDS:=@$(subst $(space),||,$(foreach pkg,$(SUPPLICANT_PROVIDERS),PACKAGE_$(pkg)))
  TITLE:=WPA Supplicant command line control utility
  VARIANT:=*
endef

define Package/eapol-test-qca/Default
  TITLE:=802.1x auth test utility
  SECTION:=net
  SUBMENU:=WirelessAPD
  CATEGORY:=Network
  DEPENDS:=$(DRV_DEPENDS) +libubus
endef

define Package/eapol-test-qca
  $(call Package/eapol-test-qca/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=supplicant-qca-full-internal
endef

define Package/eapol-test-qca-openssl
  $(call Package/eapol-test-qca/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=supplicant-qca-full-openssl
  CONFLICTS:=$(filter-out eapol-test-qca-openssl ,$(EAPOL_TEST_PROVIDERS))
  DEPENDS+=+PACKAGE_eapol-test-qca-openssl:libopenssl
  PROVIDES:=eapol-test-qca
endef

define Package/eapol-test-qca-wolfssl
  $(call Package/eapol-test-qca/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=supplicant-qca-full-wolfssl
  CONFLICTS:=$(filter-out eapol-test-qca-openssl ,$(filter-out eapol-test-qca-wolfssl ,$(EAPOL_TEST_PROVIDERS)))
  DEPENDS+=+PACKAGE_eapol-test-qca-wolfssl:libwolfssl
  PROVIDES:=eapol-test-qca
endef

define Package/eapol-test-qca-mbedtls
  $(call Package/eapol-test-qca/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=supplicant-qca-full-mbedtls
  CONFLICTS:=$(filter-out eapol-test-qca-openssl ,$(filter-out eapol-test-qca-mbedtls ,$(EAPOL_TEST_PROVIDERS)))
  DEPENDS+=+PACKAGE_eapol-test-qca-mbedtls:libmbedtls
  PROVIDES:=eapol-test-qca
endef


ifneq ($(wildcard $(PKG_BUILD_DIR)/.config_*),$(subst .configured_,.config_,$(STAMP_CONFIGURED)))
  define Build/Configure/rebuild
	$(FIND) $(PKG_BUILD_DIR) -name \*.o -or -name \*.a | $(XARGS) rm -f
	rm -f $(PKG_BUILD_DIR)/hostapd/hostapd
	rm -f $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant
	rm -f $(PKG_BUILD_DIR)/.config_*
	touch $(subst .configured_,.config_,$(STAMP_CONFIGURED))
  endef
endif

EXTERNAL_PATCH_DIR:=.

define Build/Configure
	$(Build/Configure/rebuild)
	$(if $(wildcard $(EXTERNAL_PATCH_DIR)/files/hostapd-$(CONFIG_VARIANT).config), \
		$(CP) $(EXTERNAL_PATCH_DIR)/files/hostapd-$(CONFIG_VARIANT).config $(PKG_BUILD_DIR)/hostapd/.config \
	)
	$(if $(wildcard $(EXTERNAL_PATCH_DIR)/files/wpa_supplicant-$(CONFIG_VARIANT).config), \
		$(CP) $(EXTERNAL_PATCH_DIR)/files/wpa_supplicant-$(CONFIG_VARIANT).config $(PKG_BUILD_DIR)/wpa_supplicant/.config
	)
endef

TARGET_CPPFLAGS := \
	-I$(STAGING_DIR)/usr/include/libnl3 \
	-I$(PKG_BUILD_DIR)/src/crypto \
	$(TARGET_CPPFLAGS) \
	-D_GNU_SOURCE \
	$(if $(CONFIG_WPA_MSG_MIN_PRIORITY),-DCONFIG_MSG_MIN_PRIORITY=$(CONFIG_WPA_MSG_MIN_PRIORITY))

TARGET_LDFLAGS += -lubox -lubus

ifdef CONFIG_PACKAGE_kmod-cfg80211-qca
  TARGET_LDFLAGS += -lm -lnl-3 -lnl-genl-3
endif

ifdef CONFIG_WPA_ENABLE_WEP
    DRIVER_MAKEOPTS += CONFIG_WEP=y
endif

ifeq ($(LOCAL_VARIANT),macsec)
	TARGET_LDFLAGS += -lcrypto -lssl -lfal -L$(STAGING_DIR)/usr/lib
	TARGET_CPPFLAGS += \
	-I$(STAGING_DIR)/usr/include/qca-nss-macsec
endif

define Build/RunMake
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/$(1) \
		$(TARGET_CONFIGURE_OPTS) \
		$(DRIVER_MAKEOPTS) \
		LIBS="$(TARGET_LDFLAGS)" \
		LIBS_c="$(TARGET_LDFLAGS_C)" \
		AR="$(TARGET_CROSS)gcc-ar" \
		BCHECK= \
		$(if $(findstring s,$(OPENWRT_VERBOSE)),V=1) \
		$(2)
endef

define Build/Compile/wpad-qca
	echo ` \
		$(call Build/RunMake,hostapd,-s MULTICALL=1 dump_cflags); \
		$(call Build/RunMake,wpa_supplicant,-s MULTICALL=1 dump_cflags) | \
		sed -e 's,-n ,,g' -e 's^$(TARGET_CFLAGS)^^' \
	` > $(PKG_BUILD_DIR)/.cflags
	sed -i 's/"/\\"/g' $(PKG_BUILD_DIR)/.cflags
	+$(call Build/RunMake,hostapd, \
		CFLAGS="$$$$(cat $(PKG_BUILD_DIR)/.cflags)" \
		MULTICALL=1 \
		hostapd_cli hostapd_multi.a \
	)
	+$(call Build/RunMake,wpa_supplicant, \
		CFLAGS="$$$$(cat $(PKG_BUILD_DIR)/.cflags)" \
		MULTICALL=1 \
		wpa_cli wpa_supplicant_multi.a \
	)
	+export MAKEFLAGS="$(MAKE_JOBSERVER)"; $(TARGET_CC) -o $(PKG_BUILD_DIR)/wpad \
		$(TARGET_CFLAGS) \
		./files/multicall.c \
		$(PKG_BUILD_DIR)/hostapd/hostapd_multi.a \
		$(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant_multi.a \
		$(TARGET_LDFLAGS)
endef

define Build/Compile/hostapd-qca
	+$(call Build/RunMake,hostapd, \
		hostapd hostapd_cli \
	)
endef

define Build/Compile/supplicant-qca
	+$(call Build/RunMake,wpa_supplicant, \
		wpa_cli wpa_supplicant \
	)
endef

define Build/Compile/supplicant-qca-full-internal
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-qca-full-openssl
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-qca-full-wolfssl
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-qca-full-mbedtls
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile
	$(Build/Compile/$(LOCAL_TYPE))
	$(Build/Compile/$(BUILD_VARIANT))
endef

define Install/hostapd-qca
	$(INSTALL_DIR) $(1)/usr/sbin
endef

define Install/supplicant-qca
	$(INSTALL_DIR) $(1)/usr/sbin
endef

define Package/hostapd-qca-common/install
	$(INSTALL_DIR) $(1)/etc/capabilities $(1)/etc/rc.button $(1)/etc/hotplug.d/ieee80211 $(1)/etc/init.d $(1)/lib/netifd  $(1)/usr/share/acl.d
	$(INSTALL_BIN) ./files/dhcp-get-server.sh $(1)/lib/netifd/dhcp-get-server.sh
	$(INSTALL_DATA) ./files/hostapd.sh $(1)/lib/netifd/hostapd.sh
	$(INSTALL_BIN) ./files/wpad.init $(1)/etc/init.d/wpad
	$(INSTALL_BIN) ./files/wps-hotplug.sh $(1)/etc/rc.button/wps
	$(INSTALL_DATA) ./files/wpad_acl.json $(1)/usr/share/acl.d
	$(INSTALL_DATA) ./files/wpad.json $(1)/etc/capabilities
endef

define Package/hostapd-qca/install
	$(call Install/hostapd-qca,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd $(1)/usr/sbin/
endef
Package/hostapd-qca-basic/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-basic-openssl/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-basic-wolfssl/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-basic-mbedtls/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-mini/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-openssl/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-wolfssl/install = $(Package/hostapd-qca/install)
Package/hostapd-qca-mbedtls/install = $(Package/hostapd-qca/install)

define Package/hostapd-qca-macsec/install
	$(call Install/hostapd-qca,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd $(1)/usr/sbin/hostapd-macsec
endef

ifneq ($(LOCAL_TYPE),supplicant-qca)
ifneq ($(LOCAL_VARIANT),macsec)
  define Package/hostapd-qca-utils/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_cli $(1)/usr/sbin/
  endef
endif
endif

define Package/wpad-qca/install
	$(call Install/hostapd-qca,$(1))
	$(call Install/supplicant-qca,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpad $(1)/usr/sbin/
	$(LN) wpad $(1)/usr/sbin/hostapd
	$(LN) wpad $(1)/usr/sbin/wpa_supplicant
endef
Package/wpad-qca-basic/install = $(Package/wpad-qca/install)
Package/wpad-qca-basic-openssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-basic-wolfssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-basic-mbedtls/install = $(Package/wpad-qca/install)
Package/wpad-qca-mini/install = $(Package/wpad-qca/install)
Package/wpad-qca-openssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-wolfssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-mbedtls/install = $(Package/wpad-qca/install)
Package/wpad-qca-mesh-openssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-mesh-wolfssl/install = $(Package/wpad-qca/install)
Package/wpad-qca-mesh-mbedtls/install = $(Package/wpad-qca/install)

define Package/wpa-supplicant-qca/install
	$(call Install/supplicant-qca,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant $(1)/usr/sbin/
endef
Package/wpa-supplicant-qca-basic/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-mini/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-p2p/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-openssl/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-wolfssl/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-mbedtls/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-mesh-openssl/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-mesh-wolfssl/install = $(Package/wpa-supplicant-qca/install)
Package/wpa-supplicant-qca-mesh-mbedtls/install = $(Package/wpa-supplicant-qca/install)

define Package/wpa-supplicant-qca-macsec/install
	$(call Install/supplicant-qca,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant $(1)/usr/sbin/wpa_supplicant-macsec
endef

ifneq ($(LOCAL_TYPE),hostapd-qca)
ifneq ($(LOCAL_VARIANT),macsec)
  define Package/wpa-qca-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_cli $(1)/usr/sbin/
  endef
endif
endif

ifeq ($(BUILD_VARIANT),supplicant-qca-full-internal)
  define Package/eapol-test-qca/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-qca-full-openssl)
  define Package/eapol-test-qca-openssl/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-qca-full-wolfssl)
  define Package/eapol-test-qca-wolfssl/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-qca-full-mbedtls)
  define Package/eapol-test-qca-mbedtls/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

# Build hostapd-qca-common before its dependents, to avoid
# spurious rebuilds when building multiple variants.
$(eval $(call BuildPackage,hostapd-qca-common))
#$(eval $(call BuildPackage,hostapd-qca))
#$(eval $(call BuildPackage,hostapd-qca-basic))
#$(eval $(call BuildPackage,hostapd-qca-basic-openssl))
#$(eval $(call BuildPackage,hostapd-qca-basic-wolfssl))
#$(eval $(call BuildPackage,hostapd-qca-basic-mbedtls))
#$(eval $(call BuildPackage,hostapd-qca-mini))
#$(eval $(call BuildPackage,hostapd-qca-openssl))
#$(eval $(call BuildPackage,hostapd-qca-wolfssl))
#$(eval $(call BuildPackage,hostapd-qca-mbedtls))
#$(eval $(call BuildPackage,wpad-qca))
#$(eval $(call BuildPackage,wpad-qca-mesh-openssl))
#$(eval $(call BuildPackage,wpad-qca-mesh-wolfssl))
#$(eval $(call BuildPackage,wpad-qca-mesh-mbedtls))
#$(eval $(call BuildPackage,wpad-qca-basic))
#$(eval $(call BuildPackage,wpad-qca-basic-openssl))
#$(eval $(call BuildPackage,wpad-qca-basic-wolfssl))
#$(eval $(call BuildPackage,wpad-qca-basic-mbedtls))
#$(eval $(call BuildPackage,wpad-qca-mini))
$(eval $(call BuildPackage,wpad-qca-openssl))
#$(eval $(call BuildPackage,wpad-qca-wolfssl))
#$(eval $(call BuildPackage,wpad-qca-mbedtls))
#$(eval $(call BuildPackage,wpa-supplicant-qca))
#$(eval $(call BuildPackage,wpa-supplicant-qca-mesh-openssl))
#$(eval $(call BuildPackage,wpa-supplicant-qca-mesh-wolfssl))
#$(eval $(call BuildPackage,wpa-supplicant-qca-mesh-mbedtls))
#$(eval $(call BuildPackage,wpa-supplicant-qca-basic))
#$(eval $(call BuildPackage,wpa-supplicant-qca-mini))
#$(eval $(call BuildPackage,wpa-supplicant-qca-p2p))
#$(eval $(call BuildPackage,wpa-supplicant-qca-openssl))
#$(eval $(call BuildPackage,wpa-supplicant-qca-wolfssl))
#$(eval $(call BuildPackage,wpa-supplicant-qca-mbedtls))
$(eval $(call BuildPackage,wpa-qca-cli))
$(eval $(call BuildPackage,hostapd-qca-utils))
$(eval $(call BuildPackage,eapol-test-qca))
$(eval $(call BuildPackage,eapol-test-qca-openssl))
$(eval $(call BuildPackage,eapol-test-qca-wolfssl))
$(eval $(call BuildPackage,eapol-test-qca-mbedtls))
$(eval $(call BuildPackage,hostapd-qca-macsec))
$(eval $(call BuildPackage,wpa-supplicant-qca-macsec))
