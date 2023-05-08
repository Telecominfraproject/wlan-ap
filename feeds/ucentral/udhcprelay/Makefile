include $(TOPDIR)/rules.mk

PKG_NAME:=udhcprelay
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/udhcprelay
  SECTION:=net
  CATEGORY:=Network
  TITLE:=DHCP Relay Daemon
  DEPENDS:=+libubox +libubus +kmod-ifb +tc +kmod-sched
endef

define Package/udhcprelay/conffiles
/etc/config/dhcprelay
endef

define Package/udhcprelay/install
	$(INSTALL_DIR) \
		$(1)/usr/sbin \
		$(1)/etc/init.d \
		$(1)/etc/config \
		$(1)/etc/hotplug.d/net
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/udhcprelay $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/dhcprelay.init $(1)/etc/init.d/dhcprelay
	$(INSTALL_DATA) ./files/dhcprelay.conf $(1)/etc/config/dhcprelay
	$(INSTALL_DATA) ./files/dhcprelay.hotplug $(1)/etc/hotplug.d/net/10-dhcprelay
endef

$(eval $(call BuildPackage,udhcprelay))
