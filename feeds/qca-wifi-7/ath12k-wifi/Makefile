include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=ath12k-wifi
PKG_RELEASE:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

RSTRIP:=:
STRIP:=:

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

#
# This is intended to be used on an interim basis until device-specific
# board data for new devices is available through the upstream compilation
#
# Please send a mail with your device-specific board files upstream.
# You can find instructions and examples on the linux-wireless wiki:
# <https://wireless.wiki.kernel.org/en/users/drivers/ath10k/boardfiles>
#

define Package/ath12k-wifi-default
  SUBMENU:=ath12k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=Custom Board
endef

define Package/ath12k-wifi-cig-wf189
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for CIG WF-190
endef

define Package/ath12k-wifi-edgecore-eap105
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for EAP105
endef

define Package/ath12k-wifi-sonicfi-rap7110c-341x
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for RAP7710c_341x
endef

define Package/ath12k-wifi-sonicfi-rap750e-h
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for RAP750e_h
endef

define Package/ath12k-wifi-sonicfi-rap750w-311a
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for RAP750W_311a
endef

define Package/ath12k-wifi-cig-wf189h
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for WF189H
endef

define Package/ath12k-wifi-cig-wf189w
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for WF189W
endef

define Package/ath12k-wifi-sercomm-ap72tip
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for AP72TIP
endef

define Package/ath12k-wifi-sercomm-ap72tip-v4
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for AP72TIP-v4
endef


define Package/ath12k-wifi-cig-wf189/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.wf189.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.wf189.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
endef

define Package/ath12k-wifi-edgecore-eap105/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.eap105.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.eap105.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
endef

define Package/ath12k-wifi-sonicfi-rap7110c-341x/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.rap7110c_341x.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.rap7110c_341x.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
endef

define Package/ath12k-wifi-sonicfi-rap750e-h/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN6432/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.rap750e_h.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.rap750e_h.QCN6432 $(1)/lib/firmware/ath12k/QCN6432/hw1.0/board-2.bin
#	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/regdb.bin
	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/QCN6432/hw1.0/regdb.bin
endef

define Package/ath12k-wifi-sonicfi-rap750w-311a/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN6432/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.rap750w_311a.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.rap750w_311a.QCN6432 $(1)/lib/firmware/ath12k/QCN6432/hw1.0/board-2.bin
#	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/regdb.bin
	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/QCN6432/hw1.0/regdb.bin
endef

define Package/ath12k-wifi-cig-wf189w/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN6432/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.189w.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.189w.QCN6432 $(1)/lib/firmware/ath12k/QCN6432/hw1.0/board-2.bin
	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/QCN6432/hw1.0/regdb.bin
endef


define Package/ath12k-wifi-cig-wf189h/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN6432/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.189h.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.189h.QCN6432 $(1)/lib/firmware/ath12k/QCN6432/hw1.0/board-2.bin
	$(INSTALL_DATA) ./ipq5332_qcn6432.regdb $(1)/lib/firmware/ath12k/QCN6432/hw1.0/regdb.bin
endef

define Package/ath12k-wifi-sercomm-ap72tip/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.ap72tip.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.ap72tip.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
endef

define Package/ath12k-wifi-sercomm-ap72tip-v4/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.ap72tip-v4.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
	$(INSTALL_DATA) ./board-2.bin.ap72tip-v4.IPQ5332 $(1)/lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
endef

$(eval $(call BuildPackage,ath12k-wifi-cig-wf189))
$(eval $(call BuildPackage,ath12k-wifi-edgecore-eap105))
$(eval $(call BuildPackage,ath12k-wifi-sonicfi-rap7110c-341x))
$(eval $(call BuildPackage,ath12k-wifi-sonicfi-rap750e-h))
$(eval $(call BuildPackage,ath12k-wifi-sonicfi-rap750w-311a))
$(eval $(call BuildPackage,ath12k-wifi-cig-wf189w))
$(eval $(call BuildPackage,ath12k-wifi-cig-wf189h))
$(eval $(call BuildPackage,ath12k-wifi-sercomm-ap72tip))
$(eval $(call BuildPackage,ath12k-wifi-sercomm-ap72tip-v4))
