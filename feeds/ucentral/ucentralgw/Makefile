include $(TOPDIR)/rules.mk

PKG_NAME:=ucentralgw
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=git@github.com:stephb9959/ucentralgw.git
PKG_SOURCE_DATE:=2021-03-22
PKG_SOURCE_VERSION:=d4ae407d8b2fabc6ea4ba555061adf3e11025188
#PKG_MIRROR_HASH:=3f6569a5e63fdfd032976ac0f79d736d3935101ac1b97fb370514b013c5e6bb6
CMAKE_INSTALL:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

HOST_BUILD_PREFIX:=$(STAGING_DIR_HOST)

define Package/ucentralgw
  SECTION:=ucentral
  CATEGORY:=uCentral
  DEPENDS:=+libstdcpp +poco-all +boost +boost-system +libyaml-cpp +zlib
  TITLE:= uCentral Gateway
endef

CMAKE_OPTIONS += \
	-DSMALL_BUILD=1

ifeq ($(ARCH),aarch64)
  CMAKE_OPTIONS+=-DCMAKE_SYSTEM_PROCESSOR=aarch64
endif

define Package/ucentralgw/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/usr/libexec/ucentral/

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/ucentral $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cert_scripts/* $(1)/usr/libexec/ucentral/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ucentralgw))
