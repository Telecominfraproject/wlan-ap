KERNEL_LOADADDR := 0x41208000

define Device/FitImage
        KERNEL_SUFFIX := -uImage.itb
        KERNEL = kernel-bin | libdeflate-gzip | fit gzip $$(KDIR)/image-$$(DEVICE_DTS).dtb
        KERNEL_NAME := Image
endef

define Device/FitImageLzma
        KERNEL_SUFFIX := -uImage.itb
        KERNEL = kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(DEVICE_DTS).dtb
        KERNEL_NAME := Image
endef

define Device/UbiFit
        KERNEL_IN_UBI := 1
        IMAGES := factory.ubi sysupgrade.bin
        IMAGE/factory.ubi := append-ubi
        IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef

define Device/cig_wf186w
  DEVICE_TITLE := Cigtech WF-186w
  DEVICE_DTS := qcom-ipq5018-cig-wf186w
  SUPPORTED_DEVICES := cig,wf186w
  DEVICE_PACKAGES := ath11k-wifi-cig-wf186w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += cig_wf186w

define Device/cybertan_eww622_a1
  DEVICE_TITLE := CyberTan EWW622-A1
  DEVICE_DTS := qcom-ipq5018-eww622-a1
  SUPPORTED_DEVICES := cybertan,eww622-a1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww622-a1 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += cybertan_eww622_a1

define Device/cybertan_eww631_a1
  DEVICE_TITLE := CyberTan EWW631-A1
  DEVICE_DTS := qcom-ipq5018-eww631-a1
  SUPPORTED_DEVICES := cybertan,eww631-a1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww631-a1 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += cybertan_eww631_a1

define Device/wallys_dr5018
  DEVICE_TITLE := Wallys DR5018
  DEVICE_DTS := qcom-ipq5018-wallys-dr5018
  SUPPORTED_DEVICES := wallys,dr5018
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr5018 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += wallys_dr5018

define Device/cybertan_eww631_b1
  DEVICE_TITLE := CyberTan EWW631-B1
  DEVICE_DTS := qcom-ipq5018-eww631-b1
  SUPPORTED_DEVICES := cybertan,eww631-b1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww631-b1 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp02.1
endef
TARGET_DEVICES += cybertan_eww631_b1

define Device/edgecore_eap104
  DEVICE_TITLE := EdgeCore EAP104
  DEVICE_DTS := qcom-ipq5018-eap104
  SUPPORTED_DEVICES := edgecore,eap104
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap104 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_eap104

define Device/liteon_wpx8324
  DEVICE_TITLE := Liteon WPX8324
  DEVICE_DTS := qcom-ipq5018-liteon-wpx8324
  SUPPORTED_DEVICES := liteon,wpx8324
  DEVICE_PACKAGES := ath11k-wifi-liteon-wpx8324 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += liteon_wpx8324

define Device/muxi_ap3220l
  DEVICE_TITLE := MUXI AP3220L
  DEVICE_DTS := qcom-ipq5018-muxi-ap3220l
  SUPPORTED_DEVICES := muxi,ap3220l
  DEVICE_PACKAGES := ath11k-wifi-muxi-ap3220l ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += muxi_ap3220l

define Device/motorola_q14
  DEVICE_TITLE := Motorola Q14
  DEVICE_DTS := qcom-ipq5018-q14
  SUPPORTED_DEVICES := motorola,q14
  DEVICE_PACKAGES := ath11k-wifi-motorola-q14 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar mmc-factory.bin
  IMAGE/mmc-factory.bin := append-ubi | qsdk-ipq-factory-mmc
endef
TARGET_DEVICES += motorola_q14

define Device/qcom_mp03_1
  DEVICE_TITLE := Qualcomm Maple 03.1
  DEVICE_DTS := qcom-ipq5018-mp03.1
  SUPPORTED_DEVICES := qcom,ipq5018-mp03.1
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq5018
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += qcom_mp03_1

define Device/qcom_mp03_3
  DEVICE_TITLE := Qualcomm Maple 03.3
  DEVICE_DTS := qcom-ipq5018-mp03.3
  SUPPORTED_DEVICES := qcom,ipq5018-mp03.3
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq5018
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += qcom_mp03_3

define Device/hfcl_ion4xi_w
  DEVICE_TITLE := HFCL ION4xi_w
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4xi_w
  SUPPORTED_DEVICES := hfcl,ion4xi_w
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4xi_w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4xi_w

define Device/hfcl_ion4x_w
  DEVICE_TITLE := HFCL ION4x_w
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4x_w
  SUPPORTED_DEVICES := hfcl,ion4x_w
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4x_w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4x_w

define Device/hfcl_ion4xi_HMR
  DEVICE_TITLE := HFCL ION4xi_HMR
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4xi_HMR
  SUPPORTED_DEVICES := hfcl,ion4xi_HMR
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4xi_HMR ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4xi_HMR

define Device/yuncore_fap655
  DEVICE_TITLE := Yuncore FAP650
  DEVICE_DTS := qcom-ipq5018-yuncore-fap655
  SUPPORTED_DEVICES := yuncore,fap655
  DEVICE_PACKAGES := ath11k-wifi-yuncore-fap655 ath11k-firmware-ipq50xx-map-spruce -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += yuncore_fap655

define Device/xunison_d50-5g
  DEVICE_TITLE := Xunison D50-5G
  DEVICE_DTS := qcom-ipq5018-xunison-d50-5g
  SUPPORTED_DEVICES := xunison,d50_5g
  DEVICE_PACKAGES := ath11k-wifi-xunison-d50 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000 
  DEVICE_DTS_CONFIG := config@mp03.1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += xunison_d50-5g

define Device/xunison_d50
  DEVICE_TITLE := Xunison D50
  DEVICE_DTS := qcom-ipq5018-xunison-d50
  SUPPORTED_DEVICES := xunison,d50
  DEVICE_PACKAGES := ath11k-wifi-xunison-d50 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000 ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/factory.ubi := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi | qsdk-ipq-factory-nand
endef
TARGET_DEVICES += xunison_d50
