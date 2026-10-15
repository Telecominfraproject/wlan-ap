#
# System defaults for boards running the MediaTek SDK
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mtk-sdk-defaults
PKG_VERSION:=1
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/mtk-sdk-defaults
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=System defaults for boards running the MediaTek SDK
  DEPENDS:=@MTK_SDK_KERNEL
endef

define Package/mtk-sdk-defaults/description
  System settings the MediaTek SDK was validated with. They used to be
  OpenWrt patches on staging-mtk-v5.0; as a package they only apply to
  boards built with the SDK kernel:
  - the packet steering service is disabled
  - PPPQ (per-port per-queue QoS) is enabled on the PPE
  - the system log is copied to /dev/pmsg0 so it is kept in ramoops
endef

define Build/Compile
endef

define Package/mtk-sdk-defaults/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/pppq-ebl.init $(1)/etc/init.d/pppq-ebl
	$(INSTALL_BIN) ./files/pmsg-log.init $(1)/etc/init.d/pmsg-log
	$(INSTALL_DATA) ./files/90-mtk-sdk-packet-steering $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,mtk-sdk-defaults))
