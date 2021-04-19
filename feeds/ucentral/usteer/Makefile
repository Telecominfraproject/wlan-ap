include $(TOPDIR)/rules.mk

PKG_NAME:=usteer
PKG_RELEASE:=1

PKG_SOURCE_URL=https://git.openwrt.org/project/usteer.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-19
PKG_SOURCE_VERSION:=2b1c740ce33509b314c248eb191d21ba504160ba

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
