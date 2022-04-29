#
# Copyright (C) 2017 minim.co
#

include $(TOPDIR)/rules.mk

PKG_NAME:=unum
PKG_VERSION:=v2
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/MinimSecure/unum-sdk.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-12-02
PKG_SOURCE_VERSION:=2fe70bc92514aafc2194352ee88017a89c0c8236
PKG_MIRROR_HASH:=a7618f5175b77ebd908e9691041f2ac24153c27e8097a2cfaa0eb814f059ec55

include $(INCLUDE_DIR)/package.mk

define Package/unum
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Unum-v2 agent
  URL:=http://www.minim.co
  MAINTAINER:=minim.co <support@minim.co>
  DEPENDS:=+libpthread +librt +libuci +jansson +libiwinfo \
           +libcurl +libmbedtls +libnl-tiny +zlib
  # Unable to enforce +@LIBCURL_MBEDTLS and have binary package dependencies
  # at the same time (will have to figure out later). The conditional 
  # dependencies like +!LIBCURL_MBEDTLS:libcurl aren't helping.
  # Note: careful here, it fails without failing the build
endef

define Package/unum/description
 Unum-v2 agent is a software component allowing a wireless router to be 
 managed and continuously monitored by the Minim cloud. See http://www.minim.co
 for more details.
endef

# Agent MODEL name for all the unum agent builds from LEDE w/ sources
AGENT_MODEL_ID=lede_generic

# Agent hardware ID for this particular hardware target/subtarget/device
ifeq ($(patsubst "%",%,$(CONFIG_VERSION_HWREV)),)
  AGENT_HARDWARE_ID=$(patsubst "%",%,$(CONFIG_VERSION_PRODUCT))
else
  AGENT_HARDWARE_ID=$(patsubst "%",%,$(CONFIG_VERSION_PRODUCT))_$(patsubst "%",%,$(CONFIG_VERSION_HWREV))
endif

# We do not have configure (stop LEDE from even trying it)
define Build/Configure
endef

MAKE_FLAGS += \
	TARGET_CFLAGS="$(TARGET_CFLAGS) \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	$(TARGET_CPPFLAGS) -Wno-array-bounds \
	-DAGENT_VERSION_IS_FW_VERSION \
	-DCONFIG_LIBNL20" \
	TARGET_LDFLAGS="$(TARGET_LDFLAGS)" \
	MODEL=$(AGENT_MODEL_ID) HARDWARE=$(AGENT_HARDWARE_ID) \
	TARGET_OBJ="$(PKG_BUILD_DIR)/src"

define Package/unum/install
	# untar the unum rootfs into the package install dir
	tar -zxvf $(PKG_BUILD_DIR)/out/$(AGENT_MODEL_ID)/$(AGENT_MODEL_ID)*.tgz -C $(1) > /dev/null

	# Installing properties.json to rootfs under /etc/unum to avoid
	# dealing with owrt build folders (although we do not need it on target)
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/out/$(AGENT_MODEL_ID)/release_properties.json $(1)/etc/unum/release_properties.json
endef

$(eval $(call BuildPackage,unum))
