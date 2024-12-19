include $(TOPDIR)/rules.mk

PKG_NAME:=udevmand
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/blogic/udevmand.git
PKG_MIRROR_HASH:=b643cae7315b6bc92262b9b632fe39bd9f766525a5fb69e8a8a5c37f4e26867b
PKG_SOURCE_DATE:=20241219
PKG_SOURCE_VERSION:=be0f45e7bcf6bad8143222c541c3d077bdadbecc
CMAKE_INSTALL:=1

PKG_LICENSE:=LGPL-2.1
PKG_LICENSE_FILES:=

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/udevmand
  SECTION:=ucentral
  CATEGORY:=uCentral
  DEPENDS:=+libubus +libblobmsg-json +ubusd +libjson-c +libnl-tiny +libuci
  TITLE:=Network devman mapping daemon
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/libnl-tiny

define Package/udevmand/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) ./files/* $(1)/
	$(CP) $(PKG_INSTALL_DIR)/usr/sbin/udevmand $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,udevmand))
