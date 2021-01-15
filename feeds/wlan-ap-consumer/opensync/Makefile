include $(TOPDIR)/rules.mk

PKG_NAME:=opensync
PKG_VERSION:=2.0.5.0
PKG_RELEASE:=1
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/plume-design/opensync.git
PKG_SOURCE_DATE:=2020-06-28
PKG_SOURCE_VERSION:=ee8945e44366087832bdafdf69784c6a87e3a5e6

PKG_BUILD_DEPENDS:=python3-kconfiglib/host python3-jinja2/host python3-markupsafe/host
HOST_BUILD_DEPENDS:=protobuf-c/host

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Host/Configure
endef

define Host/Compile
endef

define Host/Install
endef

define Host/Clean
endef

define Package/opensync
	CATEGORY := Network
	TITLE    := cloud network management system
	DEPENDS  := +libev +jansson +protobuf +libprotobuf-c +libmosquitto +libopenssl +openvswitch +libpcap +libuci +libcurl +libubus +libblobmsg-json +tcpdump +curl +lldpd +hostapd
	DEPENDS  += +libnl +@BUSYBOX_CUSTOM +@BUSYBOX_CONFIG_TIMEOUT +@PACKAGE_wpa-cli +@PACKAGE_hostapd-utils +@BUSYBOX_CONFIG_UDHCPD +@PACKAGE_ip-full +@BUSYBOX_CONFIG_LOGGER +@BUSYBOX_CONFIG_VCONFIG
	DEPENDS  += +libmnl +mxml
endef

define Package/opensync/description
	OpenSync is a cloud-agnostic software for the delivery, curation, and management of services for the modern home.
	For more info please visit: https://opensync.io
endef

define Package/opensync/config
	source "$(SOURCE)/Config.in"
endef

define Build/Prepare
	$(PKG_UNPACK)
	rm $(PKG_BUILD_DIR)/{vendor,platform,service-provider}
	mkdir $(PKG_BUILD_DIR)/core
	[ ! -d ./src/service-provider/ ] || $(CP) ./src/service-provider/. $(PKG_BUILD_DIR)/service-provider/
	git clone --single-branch --branch plume/dev git@github.com:plume-design/opensync-platform-cfg80211.git $(PKG_BUILD_DIR)/platform/cfg80211
	git clone --single-branch --branch plume/dev git@github.com:plume-design/opensync-vendor-plume-openwrt.git $(PKG_BUILD_DIR)/vendor/plume-openwrt
	$(Build/Patch)
	cp $(BUILD_DIR)/hostapd-wpad-full-openssl/hostapd-2020-06-08-5a8b3662/src/utils/os_unix.o $(STAGING_DIR)/usr/lib/
	cp $(BUILD_DIR)/hostapd-wpad-full-openssl/hostapd-2020-06-08-5a8b3662/src/common/wpa_ctrl.o $(STAGING_DIR)/usr/lib/
	cp $(BUILD_DIR)/hostapd-wpad-full-openssl/hostapd-2020-06-08-5a8b3662/src/common/wpa_ctrl.h $(STAGING_DIR)/usr/include/
endef

TARGET:=TARGET=$(CONFIG_OPENSYNC_TARGET)
OPENSYNC_ROOTFS:=work/$(CONFIG_OPENSYNC_TARGET)/rootfs/
PLATFORM:=PLATFORM=$(CONFIG_OPENSYNC_PLATFORM)
SERVICE_PROVIDERS:=SERVICE_PROVIDERS=$(CONFIG_OPENSYNC_SERVICE_PROVIDER)
IMAGE_DEPLOYMENT_PROFILE:=IMAGE_DEPLOYMENT_PROFILE=$(CONFIG_OPENSYNC_DEPLOYMENT_PROFILE)

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-ffunction-sections \
	-fdata-sections

TARGET_LDFLAGS += -Wl,--gc-sections

MAKE_FLAGS := \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	$(PLATFORM) $(TARGET) $(SERVICE_PROVIDERS) $(IMAGE_DEPLOYMENT_PROFILE) \
	TOOLCHAIN_DIR="$(TOOLCHAIN_DIR)" \
	TOOLCHAIN_PREFIX="$(TARGET_CROSS)" \
	NO_RDYNAMIC=1

define Build/Compile
	$(STAGING_DIR_HOSTPKG)/bin/protoc-c \
			--c_out=$(PKG_BUILD_DIR)/src/lib/datapipeline/inc \
			--proto_path=$(PKG_BUILD_DIR)/interfaces \
			$(PKG_BUILD_DIR)/interfaces/opensync_stats.proto
	$(CP) $(PKG_BUILD_DIR)/src/lib/datapipeline/inc/opensync_stats.pb-c.c $(PKG_BUILD_DIR)/src/lib/datapipeline/src
	$(RM) $(PKG_BUILD_DIR)/src/lib/datapipeline/inc/opensync_stats.pb-c.c

	$(MAKE) -C $(PKG_BUILD_DIR) V=1 \
		OWRT_CFLAGS="$(TARGET_CFLAGS)" \
		$(MAKE_FLAGS)
	$(MAKE) -C $(PKG_BUILD_DIR) rootfs \
		$(MAKE_FLAGS)
endef

define Package/opensync/install
	$(CP) ./files/* $(1)
	$(INSTALL_DIR) $(1)/usr/opensync/{bin,etc,certs}
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OPENSYNC_ROOTFS)/usr/opensync/bin/* $(1)/usr/opensync/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OPENSYNC_ROOTFS)/usr/opensync/etc/* $(1)/usr/opensync/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OPENSYNC_ROOTFS)/usr/opensync/tools/ovsh $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OPENSYNC_ROOTFS)/usr/opensync/lib/* $(1)/usr/lib
	$(MAKE) -C $(PKG_BUILD_DIR)/ $(TARGET) rootfs-copy-only INSTALL_ROOTFS_DIR=$(1)
endef

$(eval $(call HostBuild))
$(eval $(call BuildPackage,opensync))
