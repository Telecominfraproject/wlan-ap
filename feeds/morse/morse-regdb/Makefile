#
# Copyright 2023 Morse Micro
#
# This is free software, licensed under the 3-Clause BSD License.
#

# Because the contents of morse_regdb are private, this package does
# some tricky things to _avoid_ downloading under normal circumstances,
# instead using a cached file stored in this repository. If you'd like
# to regenerate the cached file, do:
#
#   make package/morse-regdb/clean package/morse-regdb/download package/morse-regdb/compile V=sc REGENERATE=1
#
include $(TOPDIR)/rules.mk


PKG_NAME:=morse-regdb
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:=python3/host

PKG_SOURCE_VERSION:=v2.1.1
PKG_SOURCE_URL:=https://github.com/MorseMicro/morse_regdb.git
PKG_SOURCE_PROTO:=git
PKG_MIRROR_HASH:=0d074d2810f520c138427ab63ce0b8ab61f0f073cf7cd91177ad840a3322540a

PKG_VERSION:=$(PKG_SOURCE_VERSION)

include $(INCLUDE_DIR)/package.mk

# Skip download unless REGENERATE is on.
# This must occur after including package.mk, as PKG_SKIP_DOWNLOAD is defined there.
PKG_SKIP_DOWNLOAD=$(USE_SOURCE_DIR)$(USE_GIT_TREE)$(USE_GIT_SRC_CHECKOUT)$(if $(REGENERATE),,1)

define Package/morse-regdb
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Regulatory data for 802.11ah
  URL:=https://bitbucket.org/morsemicro/morse_regdb/src/master/
  PKGARCH:=all
endef

define Package/morse-regdb/description
	Regulatory data for 802.11ah in a csv.
endef

define Build/Prepare
	$(if $(REGENERATE),$(call Build/Prepare/Default))
endef

define Build/Compile
	$(if $(REGENERATE),
		mkdir -p artefacts
		python3 $(PKG_BUILD_DIR)/regdbconverter.py -v -f csv $(PKG_BUILD_DIR)/regdb_Sub-1_GHz.tsv > artefacts/repo_channels.csv
	)
	mkdir -p $(PKG_BUILD_DIR)/artefacts
	# Mash the 5g shim channels in.
	python3 add_5g_shim_channels.py < artefacts/repo_channels.csv > $(PKG_BUILD_DIR)/artefacts/channels.csv
endef

define Package/morse-regdb/install
	$(INSTALL_DIR) $(1)/usr/share/morse-regdb
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/artefacts/* $(1)/usr/share/morse-regdb
	# Make channels.csv accessible from the web.
	$(INSTALL_DIR) $(1)/www
	ln -sf /usr/share/morse-regdb/channels.csv $(1)/www/halow-channels.csv
endef

$(eval $(call BuildPackage,morse-regdb))
