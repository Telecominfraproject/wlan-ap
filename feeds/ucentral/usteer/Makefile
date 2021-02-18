include $(TOPDIR)/rules.mk

PKG_NAME:=usteer
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/usteer.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-11-19
PKG_SOURCE_VERSION:=d10731f8060d3937ef562858f5a52ca7f910da31

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
