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
PKG_SOURCE_VERSION:=a0ea92b74334c70d758b8076bebd5bf052cb768c
PKG_MIRROR_HASH:=acc1347d2ec886c1f67c7719cbad1a066d8fac99c9cec5647f70e2893b6e6bd6

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


# We build from the local sources only, here are defines to 
# replace if we decide to publish it and make a download tarball.
AGENT_SOURCE_DIR=$(MINIM_UNUM_DIR)/src/unum
AGENT_FILES_DIR=$(MINIM_UNUM_DIR)/files

# Path where we store sources and file in the LEDE build tree (making it the
# same as in our build system tree)
PKG_BUILD_DIR_UNUM=$(PKG_BUILD_DIR)/src/unum
PKG_BUILD_DIR_FILES=$(PKG_BUILD_DIR)/files

# Subdir where the agent sources are (where to cd to run make)
MAKE_PATH:=src/unum

# Agent MODEL name for all the unum agent builds from LEDE w/ sources
#AGENT_MODEL_ID=openwifi_generic
AGENT_MODEL_ID=lede_generic

# Trusted CA list location on the target
AGENT_TRUSTED_CA_LIST=/etc/ssl/certs/unum-ca-certificates.crt
# Agent config folder on the target
AGENT_ETC_DIR=/etc/unum
# Agent system config on the target
AGENT_SYSTEM_CONF=$(AGENT_ETC_DIR)/system.conf
# Agent user config on the target
AGENT_USER_CONF=$(AGENT_ETC_DIR)/user.conf
# Agent SSH files folder
AGENT_SSH_DIR=/etc/unum/.ssh
# Agent support SSH authorized keys file on the target
AGENT_SSH_AKEYS=$(AGENT_SSH_DIR)/authorized_keys
# Agent support SSH RSA key file on the target
AGENT_SSH_MYKEY=$(AGENT_SSH_DIR)/id_rsa
# Agent known hosts file on the target
AGENT_SSH_KNOWN=$(AGENT_SSH_DIR)/known_hosts

# Agent hardware ID for this particular hardware target/subtarget/device
ifeq ($(patsubst "%",%,$(CONFIG_VERSION_HWREV)),)
  AGENT_HARDWARE_ID=$(patsubst "%",%,$(CONFIG_VERSION_PRODUCT))
else
  AGENT_HARDWARE_ID=$(patsubst "%",%,$(CONFIG_VERSION_PRODUCT))_$(patsubst "%",%,$(CONFIG_VERSION_HWREV))
endif

# Agent version. This is based on time if just the agent package is
# built. The normal firmware build overrides this w/ its version number.
ifeq ($(MINIM_VERSION_NUMBER),)
  AGENT_VERSION_NUMBER=$(shell date +"%y%m%d.%H%M.%S.0" | sed -E 's/\b0+([1-9][0-9]*)\b/\1/g')
else
  AGENT_VERSION_NUMBER=$(MINIM_VERSION_NUMBER)
endif

# Add common, platform and target specific features to the release feature list
RELEASE_FEATURES := $(shell cat "$(AGENT_FILES_DIR)/features-common.txt")
ifneq ("$(wildcard $(AGENT_FILES_DIR)/$(AGENT_MODEL_ID)/features.txt)","")
  RELEASE_FEATURES := $(RELEASE_FEATURES) $(shell cat "$(AGENT_FILES_DIR)/$(AGENT_MODEL_ID)/features.txt")
endif
ifneq ("$(wildcard $(AGENT_FILES_DIR)/$(AGENT_MODEL_ID)/features-$(AGENT_HARDWARE_ID).txt)","")
  RELEASE_FEATURES := $(RELEASE_FEATURES) $(shell cat "$(AGENT_FILES_DIR)/$(AGENT_MODEL_ID)/features-$(AGENT_HARDWARE_ID).txt")
endif
# Create comma separated quoted release features list
quote := "
space := $(null) #
comma := ,
dash  := -
under := _
uc = $(subst a,A,$(subst b,B,$(subst c,C,$(subst d,D,$(subst e,E,$(subst f,F,$(subst \
             g,G,$(subst h,H,$(subst i,I,$(subst j,J,$(subst k,K,$(subst l,L,$(subst \
             m,M,$(subst n,N,$(subst o,O,$(subst p,P,$(subst q,Q,$(subst r,R,$(subst \
             s,S,$(subst t,T,$(subst u,U,$(subst v,V,$(subst w,W,$(subst x,X,$(subst \
             y,Y,$(subst z,Z,$1))))))))))))))))))))))))))
RELEASE_FEATURES := $(sort $(strip $(RELEASE_FEATURES)))
RELEASE_DEFINES := $(patsubst %,-DFEATURE_%,$(subst $(dash),$(under),$(call uc,$(RELEASE_FEATURES))))
RELEASE_FEATURES := $(subst $(space),$(comma),$(patsubst %,$(quote)%$(quote),$(RELEASE_FEATURES)))

# We do not have configure (stop LEDE from even trying it)
define Build/Configure
endef

TARGET_CPPFLAGS:= \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	$(TARGET_CPPFLAGS) -Wno-array-bounds \
	-DAGENT_VERSION_IS_FW_VERSION \
	-DCONFIG_LIBNL20

define Build/Compile
	$(call Build/Compile/Default,\
	  AGENT_VERSION=$(patsubst "%",%,$(AGENT_VERSION_NUMBER))\
	  MODEL=$(AGENT_MODEL_ID) HARDWARE=$(AGENT_HARDWARE_ID)\
	  TARGET_OBJ="$(PKG_BUILD_DIR)/src" RELEASE_DEFINES="$(RELEASE_DEFINES)")
endef

define Package/unum/install
	# Agent executable
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR_UNUM)/unum $(1)/usr/bin/
	# Agent configuration folder
	$(INSTALL_DIR) $(1)$(AGENT_ETC_DIR)
	# Trusted certificates (install our own CA list)
	$(INSTALL_DIR) $(1)/etc/ssl/certs
	cat $(PKG_BUILD_DIR_FILES)/ca/*.pem > $(1)$(AGENT_TRUSTED_CA_LIST)
	# SSH keys
	$(INSTALL_DIR) $(1)$(AGENT_SSH_DIR)
	$(INSTALL_DATA) $(PKG_BUILD_DIR_FILES)/ssh_keys/authorized_keys $(1)$(AGENT_SSH_AKEYS)
	$(INSTALL_DATA) $(PKG_BUILD_DIR_FILES)/ssh_keys/id_rsa $(1)$(AGENT_SSH_MYKEY)
	$(INSTALL_DATA) $(PKG_BUILD_DIR_FILES)/ssh_keys/known_hosts $(1)$(AGENT_SSH_KNOWN)
	# Default agent config file
	echo "export UNUM_CA_FILE=$(AGENT_TRUSTED_CA_LIST)" > $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_SSH_DIR=$(AGENT_SSH_DIR)" >> $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_SSH_AKEYS=$(AGENT_SSH_AKEYS)" >> $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_SSH_MYKEY=$(AGENT_SSH_MYKEY)" >> $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_SSH_KNOWN=$(AGENT_SSH_KNOWN)" >> $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_PID_FILE_PREFIX=/var/run/unum" >> $(1)$(AGENT_SYSTEM_CONF)
	echo "export UNUM_NICENESS=13" >> $(1)$(AGENT_SYSTEM_CONF)
	# Agent init files
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/unum-updater.init $(1)/etc/init.d/unum-updater
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/unum.init $(1)/etc/init.d/unum
	$(INSTALL_DATA) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/unum.common $(1)$(AGENT_ETC_DIR)/unum.common
	# LAN/WAN IP range overlap recovery scripts
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/fix_lan_addr.init $(1)/etc/init.d/fix_lan_addr
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/fix_lan_addr.sh $(1)/usr/bin/fix_lan_addr.sh
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/restart_config.sh $(1)/usr/bin/restart_config.sh
	# Agent files to keep through the firmware upgrade
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d
	echo "/etc/unum/unum.pem" > $(1)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/unum.key" >> $(1)/lib/upgrade/keep.d/unum.upgrade
	echo "$(AGENT_USER_CONF)" >> $(1)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/.credentials_provisioned" >> $(1)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/.wifi_provisioned" >> $(1)/lib/upgrade/keep.d/unum.upgrade
	#Files to set default password and default SSID and key
	if [ -d $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/$(AGENT_HARDWARE_ID)/. ]; then \
		$(CP) -L $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/$(AGENT_HARDWARE_ID)/* $(1)/; \
	fi
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/switch_mode.sh $(1)/sbin/switch_mode.sh
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/run_on_board_sta.sh $(1)/sbin/run_on_board_sta.sh
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/create_on_board_sta.sh $(1)/sbin/create_on_board_sta.sh
	# Re-arrange the config as per the mode
	$(INSTALL_BIN) $(PKG_BUILD_DIR_FILES)/$(AGENT_MODEL_ID)/restart_config.init $(1)/etc/init.d/restart_config
	# Installing properties.json to rootfs under $(AGENT_ETC_DIR) to avoid
	# dealing with owrt build folders (although we do not need it on target)
	RELEASE_FEATURES='$(RELEASE_FEATURES)' envsubst < $(PKG_BUILD_DIR_FILES)/release_properties.json > $(1)$(AGENT_ETC_DIR)/release_properties.json
endef

$(eval $(call BuildPackage,unum,zlib))
