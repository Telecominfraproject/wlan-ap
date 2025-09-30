include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=ath11k-wifi
PKG_RELEASE:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

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

ALLWIFIBOARDS:= \
	cig-wf660a \
	cig-wf194c \
	cig-wf194c4 \
	cybertan-eww622-a1 \
	cybertan-eww631-a1 \
	cybertan-eww631-b1 \
	edgecore-eap101 \
	gl-ax1800 \
	gl-axt1800 \
	gl-b3000 \
	sercomm-wallaby \
	edgecore-eap102 \
	edgecore-oap102 \
	edgecore-oap103 \
	edgecore-eap104 \
	emplus-wap380c \
	emplus-wap385c \
	emplus-wap386v2 \
	emplus-wap581 \
	liteon-wpx8324 \
	indio-um-310ax-v1 \
	indio-um-510axp-v1 \
	indio-um-510axm-v1 \
	indio-um-325ax-v2 \
	indio-um-335ax \
	indio-um-525axp \
	indio-um-525axm \
	muxi-ap3220l \
	plasmacloud-pax1800 \
	wallys-dr5018 \
	wallys-dr6018 \
	wallys-dr6018-v4 \
	sonicfi-rap630c-311g \
	sonicfi-rap630w-311g \
	sonicfi-rap630w-312g \
	sonicfi-rap650c \
	sonicfi-rap630e \
	tplink-ex227 \
	tplink-ex447 \
	yuncore-ax840 \
	yuncore-fap650 \
	yuncore-fap655 \
	udaya-a6-id2 \
	udaya-a6-od2 \
	meshpp-s618

ALLWIFIPACKAGES:=$(foreach BOARD,$(ALLWIFIBOARDS),ath11k-wifi-$(BOARD))

define Package/ath11k-wifi-default
  SUBMENU:=ath11k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq50xx
  TITLE:=Custom Board
endef

define Package/ath11k-wifi-qcom-ipq8074
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ807x eval kits
endef

define Package/ath11k-wifi-qcom-ipq6018
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ60xx eval kits
endef

define Package/ath11k-wifi-qcom-ipq5018
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ50xx eval kits
endef

define Package/ath11k-wifi-qcom-qcn9000
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM QCN9000 eval kits
endef

define Package/ath11k-wifi-qcom-ipq6122
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ6122 eval kits
endef

define Package/ath11k-wifi-cig-wf196
$(call Package/ath11k-wifi-default)
    TITLE:=cig-wf196 bdf
endef

define Package/ath11k-wifi-cig-wf186w
$(call Package/ath11k-wifi-default)
    TITLE:=cig-wf186w bdf
endef

define Package/ath11k-wifi-cig-wf186h
$(call Package/ath11k-wifi-default)
    TITLE:=cig-wf186h bdf
endef

define Package/ath11k-wifi-gl-ax1800
$(call Package/ath11k-wifi-default)
    TITLE:=gl-ax1800 bdf
endef

define Package/ath11k-wifi-gl-axt1800
$(call Package/ath11k-wifi-default)
    TITLE:=gl-axt1800 bdf
endef

define Package/ath11k-wifi-gl-b3000
$(call Package/ath11k-wifi-default)
    TITLE:=gl-b3000 bdf
endef

define Package/ath11k-wifi-motorola-q14
$(call Package/ath11k-wifi-default)
    TITLE:=motorola q14 bdf
endef

define Package/ath11k-wifi-optimcloud-d50
$(call Package/ath11k-wifi-default)
    TITLE:=optimcloud d50 bdf
endef

define Package/ath11k-wifi-optimcloud-d60
$(call Package/ath11k-wifi-default)
    TITLE:=optimcloud d60 bdf
endef

define Package/ath11k-wifi-plasmacloud-pax1800
$(call Package/ath11k-wifi-default)
    TITLE:=plasmacloud-pax1800(-v2) bdf
endef

define Package/ath11k-wifi-hfcl-ion4xi
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4xi bdf
endef

define Package/ath11k-wifi-hfcl-ion4xe
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4xe bdf
endef

define Package/ath11k-wifi-hfcl-ion4x_3
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4x_3 bdf
endef

define Package/ath11k-wifi-hfcl-ion4xi_w
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4xi_w bdf
endef

define Package/ath11k-wifi-hfcl-ion4x_w
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4x_w bdf
endef

define Package/ath11k-wifi-hfcl-ion4xi_HMR
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4xi_HMR bdf
endef

define Package/ath11k-wifi-hfcl-ion4xi_wp
$(call Package/ath11k-wifi-default)
    TITLE:=hfcl-ion4xi_wp bdf
endef


define Package/ath11k-wifi-meshpp-s618
$(call Package/ath11k-wifi-default)
    TITLE:=meshpp-s618 bdf
endef

define Package/ath11k-wifi-edgecore-oap101
$(call Package/ath11k-wifi-default)
    TITLE:=edgecore oap101 bdf
endef

define Package/ath11k-wifi-edgecore-oap101e
$(call Package/ath11k-wifi-default)
    TITLE:=edgecore-oap101e bdf
endef

define Package/ath11k-wifi-cig-wf188n
$(call Package/ath11k-wifi-default)
    TITLE:=cig-wf188n bdf
endef

define ath11k-wifi-install-one-to
  $(INSTALL_DIR)  $(2)/lib/firmware/$(3)/
  $(INSTALL_DATA) $(1) $(2)/lib/firmware/$(3)/board.bin
endef

define ath11k-wifi-install-one
  $(if $(filter $(suffix $(1)),.IPQ5018),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),ath11k/IPQ5018/hw1.0/),\
  )
  $(if $(filter $(suffix $(1)),.IPQ6018),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),ath11k/IPQ6018/hw1.0/),\
  )
  $(if $(filter $(suffix $(1)),.IPQ8074),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),ath11k/IPQ8074/hw2.0/),\
  )
  $(if $(filter $(suffix $(1)),.QCN9074),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),ath11k/QCN9074/hw1.0/),\
  )
  $(if $(filter $(suffix $(1)),.QCN6122),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),ath11k/qcn6122/hw1.0/),\
  )

endef
# Blank line required at end of above define due to foreach context

define generate-ath11k-wifi-package
  define Package/ath11k-wifi-$(1)
    $(call Package/ath11k-wifi-default)
    TITLE:=bdwlan.bin Overrides for $(2)
    CONFLICTS:=$(PREV_BOARD)
  endef

  define Package/ath11k-wifi-$(1)/description
The $(2) requires board-specific, reference ("cal") data
that is not yet present in the upstream wireless firmware distribution.

This package supplies bdwlan.bin file(s) that, in the interim,
overwrite those supplied by the ath11k-firmware-* packages.

This is package is only necessary for the $(2).

Do not install it for any other device!
  endef

  define Package/ath11k-wifi-$(1)/install-overlay
    $$$$(foreach ATH11K_WIFI_BOARD_FILE,$$$$(wildcard board-$(1).*),\
      $$$$(call ath11k-wifi-install-one,$$$$(ATH11K_WIFI_BOARD_FILE),$$(1)))
  endef

  PREV_BOARD+=ath11k-wifi-$(1)
endef

define Package/ath11k-wifi-qcom-ipq8074/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ8074/hw2.0/
	$(INSTALL_DATA) ./board-2.bin.IPQ8074 $(1)/lib/firmware/ath11k/IPQ8074/hw2.0/board-2.bin
endef

define Package/ath11k-wifi-qcom-ipq6018/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-qcom-ipq5018/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-qcom-ipq6122/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6122/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.IPQ6122 $(1)/lib/firmware/ath11k/IPQ6122/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-qcom-qcn9000/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/QCN9074/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.QCN9000 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-cig-wf196/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/QCN9074/hw1.0/ $(1)/lib/firmware/ath11k/IPQ8074/hw2.0/
	$(INSTALL_DATA) ./board-cig-wf196_6g-ca.bin.QCN9074 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board.bin.CA
	$(INSTALL_DATA) ./board-cig-wf196_6g-us.bin.QCN9074 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board.bin.US
	$(INSTALL_DATA) ./board-cig-wf196-ca.bin.IPQ8074 $(1)/lib/firmware/ath11k/IPQ8074/hw2.0/board.bin.CA
	$(INSTALL_DATA) ./board-cig-wf196-us.bin.IPQ8074 $(1)/lib/firmware/ath11k/IPQ8074/hw2.0/board.bin.US
endef

define Package/ath11k-wifi-cig-wf186w/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-cig-wf186w-us.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.US
	$(INSTALL_DATA) ./board-cig-wf186w-ca.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.CA
	$(INSTALL_DATA) ./board-cig-wf186w-us.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.US
	$(INSTALL_DATA) ./board-cig-wf186w-ca.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.CA
endef

define Package/ath11k-wifi-cig-wf186h/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-cig-wf186h-us.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.US
	$(INSTALL_DATA) ./board-cig-wf186h-ca.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.CA
	$(INSTALL_DATA) ./board-cig-wf186h-us.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.US
	$(INSTALL_DATA) ./board-cig-wf186h-ca.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.CA
endef

define Package/ath11k-wifi-optimcloud-d50/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/QCN9074/hw1.0/
	$(INSTALL_DATA) ./board-optimcloud-d50.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-optimcloud-d50.bin.QCN9074 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board.bin
endef

define Package/ath11k-wifi-optimcloud-d60/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/QCN9074/hw1.0/
	$(INSTALL_DATA) ./board-optimcloud-d60.bin.QCN9074 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board.bin
	$(INSTALL_DATA) ./board-optimcloud-d60.bin.QCN9074 $(1)/lib/firmware/ath11k/QCN9074/hw1.0/board.bin
endef

define Package/ath11k-wifi-gl-ax1800/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-gl-ax1800.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-gl-axt1800/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-gl-axt1800.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-gl-b3000/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-gl-b3000.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-gl-b3000.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin
endef

define Package/ath11k-wifi-motorola-q14/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-motorol-q14.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-2-motorol-q14.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-plasmacloud-pax1800/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-plasmacloud-pax1800.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-meshpp-s618/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-2-meshpp-s618.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-hfcl-ion4xi/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xi.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-hfcl-ion4xe/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xe.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-hfcl-ion4x_3/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4x_3.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-hfcl-ion4xi_wp/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xi_wp.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board.bin
endef

define Package/ath11k-wifi-hfcl-ion4xi_w/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xi_w.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-hfcl-ion4xi_w.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin
endef

define Package/ath11k-wifi-hfcl-ion4x_w/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xi_w.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-hfcl-ion4xi_w.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin
endef

define Package/ath11k-wifi-hfcl-ion4xi_HMR/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-hfcl-ion4xi_HMR.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-hfcl-ion4xi_HMR.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board.bin
endef

define Package/ath11k-wifi-edgecore-oap101/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-edgecore-oap101.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-2-edgecore-oap101.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-edgecore-oap101e/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/qcn6122/hw1.0/
	$(INSTALL_DATA) ./board-edgecore-oap101e.bin.IPQ5018 $(1)/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
	$(INSTALL_DATA) ./board-2-edgecore-oap101e.bin.QCN6122 $(1)/lib/firmware/ath11k/qcn6122/hw1.0/board-2.bin
endef

define Package/ath11k-wifi-cig-wf188n/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/
	$(INSTALL_DATA) ./board-cig-wf188n-ca.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board.bin.CA
	$(INSTALL_DATA) ./board-cig-wf188n-us.bin.IPQ6018 $(1)/lib/firmware/ath11k/IPQ6018/hw1.0/board.bin.US
endef

$(eval $(call generate-ath11k-wifi-package,cig-wf660a,Cigtech WF660a))
$(eval $(call generate-ath11k-wifi-package,cig-wf194c,Cigtech WF194c))
$(eval $(call generate-ath11k-wifi-package,cig-wf194c4,Cigtech WF194c4))
$(eval $(call generate-ath11k-wifi-package,cybertan-eww622-a1,CyberTan EWW622 A1))
$(eval $(call generate-ath11k-wifi-package,cybertan-eww631-a1,CyberTan EWW631 A1))
$(eval $(call generate-ath11k-wifi-package,cybertan-eww631-b1,CyberTan EWW631 B1))
$(eval $(call generate-ath11k-wifi-package,sercomm-wallaby,Sercomm Kiwi))
$(eval $(call generate-ath11k-wifi-package,wallys-dr6018,Wallys DR6018))
$(eval $(call generate-ath11k-wifi-package,wallys-dr6018-v4,Wallys DR6018 V4))
$(eval $(call generate-ath11k-wifi-package,edgecore-eap101,EdgeCore EAP101))
$(eval $(call generate-ath11k-wifi-package,edgecore-eap102,Edgecore EAP102))
$(eval $(call generate-ath11k-wifi-package,edgecore-oap102,Edgecore OAP102))
$(eval $(call generate-ath11k-wifi-package,edgecore-oap103,Edgecore OAP103))
$(eval $(call generate-ath11k-wifi-package,edgecore-eap104,Edgecore EAP104))
$(eval $(call generate-ath11k-wifi-package,emplus-wap380c,Emplus WAP380C))
$(eval $(call generate-ath11k-wifi-package,emplus-wap385c,Emplus WAP385C))
$(eval $(call generate-ath11k-wifi-package,emplus-wap386v2,Emplus WAP386 V2))
$(eval $(call generate-ath11k-wifi-package,emplus-wap581,Emplus WAP581))
$(eval $(call generate-ath11k-wifi-package,liteon-wpx8324,Liteon WPX8324))
$(eval $(call generate-ath11k-wifi-package,indio-um-310ax-v1,Indio UM-310AX V1))
$(eval $(call generate-ath11k-wifi-package,indio-um-510axp-v1,Indio UM-510AXP V1))
$(eval $(call generate-ath11k-wifi-package,indio-um-510axm-v1,Indio UM-510AXM V1))
$(eval $(call generate-ath11k-wifi-package,indio-um-325ax-v2,Indio UM-325AX V2))
$(eval $(call generate-ath11k-wifi-package,indio-um-335ax,Indio UM-335AX))
$(eval $(call generate-ath11k-wifi-package,indio-um-525axp,Indio UM-525AXP))
$(eval $(call generate-ath11k-wifi-package,indio-um-525axm,Indio UM-525AXM))
$(eval $(call generate-ath11k-wifi-package,sonicfi-rap630c-311g,Sonicfi RAP630C 311G))
$(eval $(call generate-ath11k-wifi-package,sonicfi-rap630w-311g,Sonicfi RAP630W 311G))
$(eval $(call generate-ath11k-wifi-package,sonicfi-rap630w-312g,Sonicfi RAP630W 312G))
$(eval $(call generate-ath11k-wifi-package,sonicfi-rap650c,SonicFi RAP650C))
$(eval $(call generate-ath11k-wifi-package,sonicfi-rap630e,SonicFi RAP630E))
$(eval $(call generate-ath11k-wifi-package,tplink-ex227,TP-Link EX227))
$(eval $(call generate-ath11k-wifi-package,tplink-ex447,TP-Link EX447))
$(eval $(call generate-ath11k-wifi-package,yuncore-ax840,YunCore AX840))
$(eval $(call generate-ath11k-wifi-package,meshpp-s618,Mesh++ S618))
$(eval $(call generate-ath11k-wifi-package,muxi-ap3220l,MUXI AP3220L))
$(eval $(call generate-ath11k-wifi-package,yuncore-fap650,YunCore FAP650))
$(eval $(call generate-ath11k-wifi-package,yuncore-fap655,YunCore FAP655))
$(eval $(call generate-ath11k-wifi-package,udaya-a6-id2,Udaya A6-ID2))
$(eval $(call generate-ath11k-wifi-package,udaya-a6-od2,Udaya A6-OD2))
$(eval $(call generate-ath11k-wifi-package,wallys-dr5018,Wallys DR5018))

$(foreach PACKAGE,$(ALLWIFIPACKAGES),$(eval $(call BuildPackage,$(PACKAGE))))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq5018))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq6122))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq8074))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq6018))
$(eval $(call BuildPackage,ath11k-wifi-qcom-qcn9000))
$(eval $(call BuildPackage,ath11k-wifi-cig-wf196))
$(eval $(call BuildPackage,ath11k-wifi-cig-wf186w))
$(eval $(call BuildPackage,ath11k-wifi-cig-wf186h))
$(eval $(call BuildPackage,ath11k-wifi-motorola-q14))
$(eval $(call BuildPackage,ath11k-wifi-optimcloud-d50))
$(eval $(call BuildPackage,ath11k-wifi-optimcloud-d60))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4xi))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4xe))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4x_3))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4xi_wp))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4xi_w))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4x_w))
$(eval $(call BuildPackage,ath11k-wifi-hfcl-ion4xi_HMR))
$(eval $(call BuildPackage,ath11k-wifi-edgecore-oap101))
$(eval $(call BuildPackage,ath11k-wifi-edgecore-oap101e))
$(eval $(call BuildPackage,ath11k-wifi-cig-wf188n))
