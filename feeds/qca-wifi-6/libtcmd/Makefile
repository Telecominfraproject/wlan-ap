include $(TOPDIR)/rules.mk

PKG:=libtcmd
PKG_NAME:=$(PKG)
PKG_RELEASE:=1

PKG_VERSION:=11.5

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA libtcmd utils
  DEPENDS:= @TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx +libpthread +libnl
endef

define Package/$(PKG_NAME)/description/Default
  LIBTCMD Package Support for QCA WIFI 11 drivers
endef


TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include \
		 -I$(STAGING_DIR)/usr/include/libnl3 \
		 -I$(STAGING_DIR)/include \
		 -fPIC -DWLAN_API_NL80211 -DLIBNL_2 -DWIN_AP_HOST

TARGET_LDFLAGS += -lnl-3 -lnl-genl-3 -shared

ifneq ($(CONFIG_PACKAGE_kmod-ath11k),)
	TARGET_CFLAGS+=-DWIN_AP_HOST_OPEN=1
endif

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/ $(1)/usr/lib/
	$(CP)   $(PKG_BUILD_DIR)/*.h $(1)/usr/include/
	$(CP)   $(PKG_BUILD_DIR)/libtcmd.so  $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libtcmd.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libtcmd))
