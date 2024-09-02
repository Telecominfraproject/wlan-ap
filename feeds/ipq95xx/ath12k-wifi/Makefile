include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=ath12k-wifi
PKG_RELEASE:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile

endef

define Package/ath12k-wifi-default
  SUBMENU:=ath12k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS:=@(TARGET_qcn9274||TARGET_ipq53xx)
  TITLE:=Custom Board
endef

define Package/ath12k-wifi-qcom-qcn9274
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for QCOM QCN9274 eval kits
endef

define Package/ath12k-wifi-qcom-qcn9274/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN9274/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.QCN9274 $(1)/lib/firmware/ath12k/QCN9274/hw1.0/board-2.bin
endef

define Package/ath12k-wifi-cig-wf189
$(call Package/ath12k-wifi-default)
    TITLE:=board.bin for CIG WF189
endef

define Package/ath12k-wifi-edgecore-eap105
$(call Package/ath12k-wifi-default)
    TITLE:=board.bin for Edgecore EAP105
endef

define Package/ath12k-wifi-sercomm-ap72tip
$(call Package/ath12k-wifi-default)
    TITLE:=board.bin for Sercomm WIFI-7
endef

define Package/ath12k-wifi-sonicfi-rap7110c-341x
$(call Package/ath12k-wifi-default)
    TITLE:=board.bin for SONICFI RAP7110C-341X
endef

define Package/ath12k-wifi-cig-wf189/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./board-cig-wf189.bin.qcn9224 $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board.bin
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-cig-wf189.bin.ipq53xx $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board.bin
endef

define Package/ath12k-wifi-edgecore-eap105/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./board-edgecore-eap105.bin.qcn9224 $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board.bin
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-edgecore-eap105.bin.ipq53xx $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board.bin
endef

define Package/ath12k-wifi-sercomm-ap72tip/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./board-sercomm-ap72tip.bin.qcn9224 $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board.bin
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-sercomm-ap72tip.bin.ipq53xx $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board.bin
endef

define Package/ath12k-wifi-sonicfi-rap7110c-341x/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./board-sonicfi-rap7110c-341x.bin.qcn9224 $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board.bin
	$(INSTALL_DATA) ./regdb.bin $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-sonicfi-rap7110c-341x.bin.ipq53xx $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board.bin
endef

$(eval $(call BuildPackage,ath12k-wifi-qcom-qcn9274))
$(eval $(call BuildPackage,ath12k-wifi-cig-wf189))
$(eval $(call BuildPackage,ath12k-wifi-edgecore-eap105))
$(eval $(call BuildPackage,ath12k-wifi-sercomm-ap72tip))
$(eval $(call BuildPackage,ath12k-wifi-sonicfi-rap7110c-341x))

