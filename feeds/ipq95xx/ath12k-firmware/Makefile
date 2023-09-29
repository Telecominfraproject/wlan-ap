include $(TOPDIR)/rules.mk

PKG_NAME:=ath12k-firmware
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=main
PKG_SOURCE_URL:=https://github.com/quic/upstream-wifi-fw.git
PKG_MIRROR_HASH:=ade4287ff2935ad1d54e5dabb8e6de28f648d0974fa76238fcc1616235e6773e
PKG_SOURCE_VERSION:=3417bb86645c5ff4c58258db7cc33e43260b4222

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/ath12k-firmware-default
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  DEPENDS:=
endef

define Package/ath12k-firmware-qcn92xx
  $(Package/ath12k-firmware-default)
  TITLE:=ath12k firmware for qcn92xx devices
  DEPENDS:=@TARGET_ipq95xx
endef

define Build/Compile

endef

define Package/ath12k-firmware-qcn92xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw2.0
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ath12k-firmware/QCN9274/hw2.0/1.1.1/WLAN.WBE.1.1.1-00126-QCAHKSWPL_SILICONZ-1/* \
		$(1)/lib/firmware/ath12k/QCN92XX/hw2.0
endef

$(eval $(call BuildPackage,ath12k-firmware-qcn92xx))
