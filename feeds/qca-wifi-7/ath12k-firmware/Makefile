include $(TOPDIR)/rules.mk

PKG_NAME:=ath12k-firmware
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

RSTRIP:=:
STRIP:=:

define Package/ath12k-firmware-default
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  DEPENDS:=
endef

define Package/ath12k-firmware-qcn92xx
$(Package/ath12k-firmware-default)
  TITLE:=ath12k firmware for QCN92XX devices
endef

define Package/ath12k-firmware-qcn92xx/description
Standard ath12k firmware for QCN92XX from QCA
endef

define Package/ath12k-firmware-ipq5332
$(Package/ath12k-firmware-default)
  TITLE:=ath12k firmware for IPQ5332 devices
endef

define Package/ath12k-firmware-ipq5332/description
Standard ath12k firmware for IPQ5332 from QCA
endef

define Package/ath12k-firmware-ipq5332-peb
$(Package/ath12k-firmware-default)
  TITLE:=ath12k firmware for IPQ5332 + Pebble devices
endef

define Package/ath12k-firmware-ipq5332-peb/description
Standard ath12k firmware for IPQ5332 + Pebble from QCA
endef

define Build/Compile

endef

define Package/ath12k-firmware-qcn92xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./files/QCN92XX/* \
		$(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
endef

define Package/ath12k-firmware-ipq5332/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ5332/
	$(INSTALL_DATA) ./files/IPQ5332/* \
		$(1)/lib/firmware/IPQ5332/
endef

define Package/ath12k-firmware-ipq5332-peb/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ5332/
	$(INSTALL_DIR) $(1)/lib/firmware/qcn6432/
	$(INSTALL_DATA) ./files/IPQ5332_peb/*.* \
		$(1)/lib/firmware/IPQ5332/
	$(INSTALL_DATA) ./files/IPQ5332_peb/qcn6432/* \
		$(1)/lib/firmware/qcn6432/
endef

$(eval $(call BuildPackage,ath12k-firmware-qcn92xx))
$(eval $(call BuildPackage,ath12k-firmware-ipq5332))
$(eval $(call BuildPackage,ath12k-firmware-ipq5332-peb))
