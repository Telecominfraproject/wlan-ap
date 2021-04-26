include $(TOPDIR)/rules.mk

PKG_NAME:=ucentralgw
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=git@github.com:Telecominfraproject/wlan-cloud-ucentralgw.git
PKG_SOURCE_DATE:=2021-03-28
PKG_SOURCE_VERSION:=4fe4a865b7ddf43991efec5fd3f21c34f89b7146
CMAKE_INSTALL:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SHARED_LDFLAGS=""

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

define Package/lldpd/conffiles
/etc/ucentral/
endef

define Package/ucentralgw/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/usr/libexec/ucentral/

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/ucentralgw $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cert_scripts/* $(1)/usr/libexec/ucentral/
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ucentralgw))
