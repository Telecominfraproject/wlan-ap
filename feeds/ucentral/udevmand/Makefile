include $(TOPDIR)/rules.mk

PKG_NAME:=udevmand
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/blogic/udevmand.git
PKG_MIRROR_HASH:=51bcf59754ef87913c40f2f1c708c8d2d2eb0ad5fc128a5c891e54ea4b3b035e
PKG_SOURCE_DATE:=20220112
PKG_SOURCE_VERSION:=3d2b67b180679a6f5687e8d318a66a7cbad3fa7b
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
