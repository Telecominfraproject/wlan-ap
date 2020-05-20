include $(TOPDIR)/rules.mk

PKG_NAME:=opensync
PKG_VERSION:=1.0
PKG_RELEASE:=1

OPENSYNC_ROOTFS:=work/$(TARGET)/rootfs/
PKG_BUILD_DEPENDS:=python3-kconfiglib/host

include $(INCLUDE_DIR)/package.mk

define Package/opensync
	CATEGORY:=Network
	TITLE:=cloud network management system
	DEPENDS:=+libev +jansson +protobuf +libprotobuf-c +libmosquitto +libopenssl +openvswitch +libpcap +libuci +libiwinfo
endef

define Package/opensync/description
	OpenSync is a cloud-agnostic software for the delivery, curation, and management of services for the modern home.
	For more info please visit: https://opensync.io
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp -ra $(OPENSYNC_SRC)/opensync $(PKG_BUILD_DIR)
	cp -ra $(OPENSYNC_SRC)/.git $(PKG_BUILD_DIR)
	cp -ra $(OPENSYNC_SRC)/.gitmodules $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/opensync/core TARGET=$(TARGET) TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" TOOLCHAIN_PREFIX="$(TARGET_CROSS)" STAGING_DIR="$(STAGING_DIR)"
	$(MAKE) -C $(PKG_BUILD_DIR)/opensync/core rootfs TARGET=$(TARGET) TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" TOOLCHAIN_PREFIX="$(TARGET_CROSS)"
endef

define Package/opensync/install
	$(INSTALL_DIR) $(1)/usr/plume/bin
	$(INSTALL_DIR) $(1)/usr/plume/etc
	$(INSTALL_DIR) $(1)/usr/plume/certs
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/opensync/core/$(OPENSYNC_ROOTFS)/usr/plume/bin/* $(1)/usr/plume/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/opensync/core/$(OPENSYNC_ROOTFS)/usr/plume/etc/* $(1)/usr/plume/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/opensync/core/$(OPENSYNC_ROOTFS)/usr/plume/tools/ovsh $(1)/usr/plume/bin
	$(MAKE) -C $(PKG_BUILD_DIR)/opensync/core TARGET=$(TARGET) rootfs-copy-only INSTALL_ROOTFS_DIR=$(1)
endef

$(eval $(call BuildPackage,opensync))
