include $(TOPDIR)/rules.mk

PKG_NAME:=mt76-vendor
PKG_RELEASE=1

PKG_LICENSE:=GPLv2
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR:=$(PKG_BUILD_DIR)
CMAKE_BINARY_DIR:=$(PKG_BUILD_DIR)

define Package/mt76-vendor
  SECTION:=devel
  CATEGORY:=Development
  TITLE:=vendor cmd for mt76
  DEPENDS:=+libnl-tiny
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/libnl-tiny

define Package/mt76-vendor/install
	mkdir -p $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mt76-vendor $(1)/usr/sbin
endef

$(eval $(call BuildPackage,mt76-vendor))
