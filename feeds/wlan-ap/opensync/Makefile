include $(TOPDIR)/rules.mk

PKG_NAME:=opensync
PKG_VERSION:=2.0.5.0
PKG_RELEASE:=1
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/plume-design/opensync.git
PKG_SOURCE_DATE:=2020-06-28
PKG_SOURCE_VERSION:=ee8945e44366087832bdafdf69784c6a87e3a5e6

PKG_BUILD_DEPENDS:=python3-kconfiglib/host python3-jinja2/host python3-markupsafe/host

include $(INCLUDE_DIR)/package.mk

define Package/opensync
	CATEGORY:=Network
	TITLE:=cloud network management system
	DEPENDS:=+libev +jansson +protobuf +libprotobuf-c +libmosquitto +libopenssl +openvswitch +libpcap +libuci +libcurl +libnl-tiny +libubus +libblobmsg-json +tcpdump +curl +lldpd +wlan-ap-keys
endef

define Package/opensync/description
	OpenSync is a cloud-agnostic software for the delivery, curation, and management of services for the modern home.
	For more info please visit: https://opensync.io
endef

define Package/opensync/conffiles
/usr/opensync/certs/
/usr/opensync/etc/
endef

define Build/Prepare
	$(PKG_UNPACK)
	rm $(PKG_BUILD_DIR)/{vendor,platform}
	mkdir $(PKG_BUILD_DIR)/core
	[ ! -d ./src/ ] || $(CP) ./src/. $(PKG_BUILD_DIR)
	$(Build/Patch)
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/libnl-tiny

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		OWRT_CFLAGS="$(TARGET_CFLAGS)" \
		TARGET=tip TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" \
		TOOLCHAIN_PREFIX="$(TARGET_CROSS)" STAGING_DIR="$(STAGING_DIR)"
	$(MAKE) -C $(PKG_BUILD_DIR) rootfs \
		TARGET=tip TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" \
		TOOLCHAIN_PREFIX="$(TARGET_CROSS)"
endef

define Package/opensync/install
	$(CP) ./files/* $(1)
	$(INSTALL_DIR) $(1)/usr/opensync/{bin,etc,certs}
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/tip/rootfs/usr/opensync/bin/* $(1)/usr/opensync/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/tip/rootfs/usr/opensync/etc/* $(1)/usr/opensync/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/work/tip/rootfs/usr/opensync/tools/ovsh $(1)/usr/opensync/bin
	$(MAKE) -C $(PKG_BUILD_DIR)/ TARGET=tip rootfs-copy-only INSTALL_ROOTFS_DIR=$(1)
ifdef CONFIG_PACKAGE_wlan-ap-keys
	rm -f $(1)/usr/opensync/certs/*
endif
endef

$(eval $(call BuildPackage,opensync))
