include $(TOPDIR)/rules.mk

PKG_NAME:=opensync
PKG_VERSION:=1.4.0.1
PKG_RELEASE:=1
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/plume-design/opensync.git
PKG_SOURCE_DATE:=2019-10-25
PKG_SOURCE_VERSION:=d46964b2588800e92ac96f562054d6f45cb81147

PKG_BUILD_DEPENDS:=python3-kconfiglib/host

include $(INCLUDE_DIR)/package.mk

define Package/opensync/default
	CATEGORY:=Network
	TITLE:=cloud network management system
	DEPENDS:=+libev +jansson +protobuf +libprotobuf-c +libmosquitto +libopenssl +openvswitch +libpcap +libuci +libiwinfo +iw
endef

define Package/opensync-ap2220
  $(call Package/opensync/default)
  TITLE += (AP2220)
  VARIANT:=AP2220
  DEPENDS+=@TARGET_ipq40xx
endef

define Package/opensync-ecw5410
  $(call Package/opensync/default)
  TITLE += (ECW5410)
  VARIANT:=ECW5410
  DEPENDS+=@TARGET_ipq806x
endef

define Package/opensync-ecw5211
  $(call Package/opensync/default)
  TITLE += (ECW5211)
  VARIANT:=ECW5211
  DEPENDS+=@TARGET_ipq40xx
endef

define Package/opensync-ea8300
  $(call Package/opensync/default)
  TITLE += (EA8300)
  VARIANT:=IPQ40XX
  DEPENDS+=@TARGET_ipq40xx
endef

define Package/opensync/description
	OpenSync is a cloud-agnostic software for the delivery, curation, and management of services for the modern home.
	For more info please visit: https://opensync.io
endef

Package/opensync-ap2220/description=$(call Package/opensync/description)
Package/opensync-ecw5410/description=$(call Package/opensync/description)
Package/opensync-ecw5211/description=$(call Package/opensync/description)
Package/opensync-ea8300/description=$(call Package/opensync/description)

define Build/Prepare
	$(PKG_UNPACK)
	rm $(PKG_BUILD_DIR)/{vendor,platform}
	mkdir $(PKG_BUILD_DIR)/core
	[ ! -d ./src/ ] || $(CP) ./src/. $(PKG_BUILD_DIR)
	$(Build/Patch)
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		OWRT_CFLAGS="$(TARGET_CFLAGS)" \
		TARGET=$(BUILD_VARIANT) TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" \
		TOOLCHAIN_PREFIX="$(TARGET_CROSS)" STAGING_DIR="$(STAGING_DIR)"
	$(MAKE) -C $(PKG_BUILD_DIR) rootfs \
		TARGET=$(BUILD_VARIANT) TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" \
		TOOLCHAIN_PREFIX="$(TARGET_CROSS)"
endef

define Package/opensync/install
	$(INSTALL_DIR) $(1)/usr/plume/bin
	$(INSTALL_DIR) $(1)/usr/plume/etc
	$(INSTALL_DIR) $(1)/usr/plume/certs
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/$(BUILD_VARIANT)/rootfs//usr/plume/bin/* $(1)/usr/plume/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/$(BUILD_VARIANT)/rootfs/usr/plume/etc/* $(1)/usr/plume/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/$(BUILD_VARIANT)/rootfs/usr/plume/tools/ovsh $(1)/usr/plume/bin
	$(MAKE) -C $(PKG_BUILD_DIR)/ TARGET=$(BUILD_VARIANT) rootfs-copy-only INSTALL_ROOTFS_DIR=$(1)
endef

Package/opensync-ap2220/install=$(call Package/opensync/install,$(1))
Package/opensync-ecw5410/install=$(call Package/opensync/install,$(1))
Package/opensync-ecw5211/install=$(call Package/opensync/install,$(1))
Package/opensync-ea8300/install=$(call Package/opensync/install,$(1))

$(eval $(call BuildPackage,opensync-ap2220))
$(eval $(call BuildPackage,opensync-ecw5410))
$(eval $(call BuildPackage,opensync-ecw5211))
$(eval $(call BuildPackage,opensync-ea8300))
