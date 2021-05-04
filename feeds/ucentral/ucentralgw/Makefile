include $(TOPDIR)/rules.mk

PKG_NAME:=ucentralgw
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=git@github.com:Telecominfraproject/wlan-cloud-ucentralgw.git
PKG_SOURCE_DATE:=2021-03-28
PKG_SOURCE_VERSION:=a42e9608df8a465f9899cc23c5261a263a00b154
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

define Package/mdadm/conffiles
/etc/ucentral/
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include
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
