#
# Copyright 2023 Morse Micro
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=morsecli
PKG_RELEASE=1

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

PKG_VERSION:=mm6108-2.0.1

PKG_SOURCE_URL:=https://github.com/MorseMicro/morse_cli.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=a21c7d0dcb0e211e7a84a9ac4d4fffffdce72554
PKG_SOURCE_MIRROR:=0
PKG_MIRROR_HASH:=6643eedcef2c996e900668372eeb5427f2e59133fff61548d01633fbd90e081b

PKG_MAINTAINER:=Morse Micro
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

# Installs morse_cli only. /sbin/morsectrl is built by the morsectrl package,
# so this package does not PROVIDE morsectrl.
define Package/morsecli
  SECTION:=utils
  CATEGORY:=Network
  TITLE:=Morse Micro WIFI HaLow driver command line tool
  DEPENDS:= +kmod-morse +libnl +libusb-1.0
  FILES:=\
	$(PKG_BUILD_DIR)/morse_cli
endef

MAKE_FLAGS += \
    MORSECTRL_VERSION_STRING=$(PKG_VERSION) \
    CONFIG_MORSE_TRANS_NL80211=1 \

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/libnl3

define Package/morsecli/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/morse_cli $(1)/sbin/

	$(INSTALL_DIR) $(1)/etc/profile.d
	$(CP) $(CURDIR)/files/morse_cli.sh $(1)/etc/profile.d/
endef

$(eval $(call BuildPackage,morsecli))
