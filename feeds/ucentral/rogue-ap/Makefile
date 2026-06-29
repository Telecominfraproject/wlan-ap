include $(TOPDIR)/rules.mk

PKG_NAME:=rogue-ap
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:= Niko Lei <niko.lei@cybertan.com.tw>

include $(INCLUDE_DIR)/package.mk

define Package/rogue-ap
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Rogue AP Detection Service
  DEPENDS:=+ucode \
	+ucode-mod-nl80211 \
	+ucode-mod-rtnl \
	+ucode-mod-ubus \
	+ucode-mod-uci \
	+ucode-mod-fs \
	+ucode-mod-uloop \
	+iw
endef

define Package/rogue-ap/description
	Periodically scan nearby Wi-Fi access points, classify them using
	configured rules, and report the results through uCentral.
endef

define Build/Compile
endef

define Package/rogue-ap/conffiles
/etc/config/rogueap
endef

define Package/rogue-ap/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/usr/share/rogueap
	$(CP) ./files/etc/config/rogueap $(1)/etc/config
	$(CP) ./files/etc/init.d/rogueap.init $(1)/etc/init.d/rogueap
	$(CP) ./files/usr/share/rogueap/rogueap-common.uc $(1)/usr/share/rogueap/rogueap-common.uc
	$(CP) ./files/usr/share/rogueap/rogueap-wifi6.uc $(1)/usr/share/rogueap/rogueap-wifi6.uc
	$(CP) ./files/usr/share/rogueap/rogueap-wifi7.uc $(1)/usr/share/rogueap/rogueap-wifi7.uc
endef

$(eval $(call BuildPackage,rogue-ap))
