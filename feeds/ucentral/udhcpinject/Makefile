include $(TOPDIR)/rules.mk

PKG_NAME:=udhcpinject
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/udhcpinject
	SECTION:=net
	CATEGORY:=Network
	TITLE:=An agent to inject DHCP option
	DEPENDS:=+libpcap +kmod-ifb +tc +libuci
	MAINTAINER:=kmk <alex18_huang@accton.com>
endef

define Package/udhcpinject/description
A utility to insert DHCP option 82 transparently into DHCP Discover packets.
The format is as follows:
	Option 82: (82) Agent Information
		Option: (1) Agent Circuit ID
			Agent Circuit ID: BSSID:ESSID
		Option: (2) Agent Remote ID
			Agent Remote ID: AP Hostname
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/udhcpinject/conffiles
/etc/config/dhcpinject
endef

define Package/udhcpinject/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/udhcpinject $(1)/usr/bin/
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,udhcpinject))
