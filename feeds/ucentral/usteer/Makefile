include $(TOPDIR)/rules.mk

PKG_NAME:=usteer
PKG_RELEASE:=1

PKG_SOURCE_URL=https://git.openwrt.org/project/usteer.git
PKG_MIRROR_HASH:=34a9f2867b3e9050d4707b986481681f97f6fad71bc3fb3276fa3c3b26e2ab67
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-19
PKG_SOURCE_VERSION:=f42bf2962edd0199e4c96ebf19933b2846bcce27
#PKG_MIRROR_HASH:=fe00f6ba4e188c1235338a6786c8b23a11a5d4b0e663ffb494ac3d2f4bdc1864

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/usteer
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+libubox +libubus +libblobmsg-json +libnl-tiny +libpcap
  TITLE:=OpenWrt AP roaming assist daemon
endef

define Package/usteer/conffiles
/etc/config/usteer
endef

define Package/usteer/install
	$(INSTALL_DIR) $(1)/sbin $(1)/etc/init.d $(1)/etc/config
	$(CP) ./files/* $(1)/
	$(CP) $(PKG_BUILD_DIR)/{usteerd,ap-monitor} $(1)/sbin/
endef

$(eval $(call BuildPackage,usteer))
