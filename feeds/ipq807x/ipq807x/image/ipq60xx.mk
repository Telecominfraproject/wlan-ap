KERNEL_LOADADDR := 0x41008000

DEVICE_VARS += CE_TYPE

define Device/cig_wf188n
  DEVICE_TITLE := Cigtech WF-188n
  DEVICE_DTS := qcom-ipq6018-cig-wf188n
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := cig,wf188n
  DEVICE_PACKAGES := ath11k-wifi-cig-wf188n uboot-env
endef
TARGET_DEVICES += cig_wf188n

define Device/hfcl_ion4xe
  DEVICE_TITLE := HFCL ION4Xe
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4xe
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4xe
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018 uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4xe

define Device/hfcl_ion4x
  DEVICE_TITLE := HFCL ION4X
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4x
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4x
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018 uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4x

define Device/hfcl_ion4x_2
  DEVICE_TITLE := HFCL ION4X_2
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4x_2
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4x_2
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018 uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4x_2

define Device/hfcl_ion4xi
  DEVICE_TITLE := HFCL ION4Xi
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4xi
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4xi
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4xi uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4xi

define Device/edgecore_eap101
  DEVICE_TITLE := EdgeCore EAP101
  DEVICE_DTS := qcom-ipq6018-edgecore-eap101
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := edgecore,eap101
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap101 uboot-envtools -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3 kmod-usb2
endef
TARGET_DEVICES += edgecore_eap101

define Device/indio_um-310ax-v1
  DEVICE_TITLE := Indio UM-310AX V1
  DEVICE_DTS := qcom-ipq6018-indio-um-310ax-v1
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := indio,um-310ax-v1
  DEVICE_PACKAGES := ath11k-wifi-indio-um-310ax-v1 uboot-env
endef
TARGET_DEVICES += indio_um-310ax-v1

define Device/indio_um-510axp-v1
  DEVICE_TITLE := Indio UM-510AXP V1
  DEVICE_DTS := qcom-ipq6018-indio-um-510axp-v1
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := indio,um-510axp-v1
  DEVICE_PACKAGES := ath11k-wifi-indio-um-510axp-v1 uboot-env
endef
TARGET_DEVICES += indio_um-510axp-v1

define Device/indio_um-510axm-v1
  DEVICE_TITLE := Indio UM-510AXM V1
  DEVICE_DTS := qcom-ipq6018-indio-um-510axm-v1
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := indio,um-510axm-v1
  DEVICE_PACKAGES := ath11k-wifi-indio-um-510axm-v1 uboot-env
endef
TARGET_DEVICES += indio_um-510axm-v1

define Device/wallys_dr6018
  DEVICE_TITLE := Wallys DR6018
  DEVICE_DTS := qcom-ipq6018-wallys-dr6018
  DEVICE_DTS_CONFIG := config@cp01-c4
  SUPPORTED_DEVICES := wallys,dr6018
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr6018 uboot-envtools -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3 kmod-usb2
endef
TARGET_DEVICES += wallys_dr6018

define Device/wallys_dr6018_v4
  DEVICE_TITLE := Wallys DR6018 V4
  DEVICE_DTS := qcom-ipq6018-wallys-dr6018-v4
  DEVICE_DTS_CONFIG := config@cp01-c4
  SUPPORTED_DEVICES := wallys,dr6018-v4
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr6018-v4 uboot-envtools ath11k-firmware-qcn9000
endef
TARGET_DEVICES += wallys_dr6018_v4

define Device/qcom_cp01_c1
  DEVICE_TITLE := Qualcomm Cypress C1
  DEVICE_DTS := qcom-ipq6018-cp01-c1
  SUPPORTED_DEVICES := qcom,ipq6018-cp01
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018
endef
TARGET_DEVICES += qcom_cp01_c1

define Device/glinet_ax1800
  DEVICE_TITLE := GL-iNet AX1800
  DEVICE_DTS := qcom-ipq6018-gl-ax1800
  SUPPORTED_DEVICES := glinet,ax1800
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-gl-ax1800 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += glinet_ax1800

define Device/glinet_axt1800
  DEVICE_TITLE := GL-iNet AXT1800
  DEVICE_DTS := qcom-ipq6018-gl-axt1800
  SUPPORTED_DEVICES := glinet,axt1800
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-gl-axt1800 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += glinet_axt1800

define Device/yuncore_ax840
  DEVICE_TITLE := YunCore AX840
  DEVICE_DTS := qcom-ipq6018-yuncore-ax840
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := yuncore,ax840
  DEVICE_PACKAGES := ath11k-wifi-yuncore-ax840 uboot-env
endef
TARGET_DEVICES += yuncore_ax840

define Device/plasmacloud_common_64k
  DEVICE_PACKAGES := uboot-envtools
  CE_TYPE :=
  BLOCKSIZE := 64k
  IMAGES := sysupgrade.tar factory.bin
  IMAGE/factory.bin := append-rootfs | pad-rootfs | openmesh-image ce_type=$$$$(CE_TYPE)
  IMAGE/sysupgrade.tar := append-rootfs | pad-rootfs | sysupgrade-tar rootfs=$$$$@ | append-metadata
  KERNEL += | pad-to $$(BLOCKSIZE)
endef

define Device/plasmacloud_pax1800-v1
  $(Device/plasmacloud_common_64k)
  DEVICE_TITLE := Plasma Cloud PAX1800 v1
  DEVICE_DTS := qcom-ipq6018-pax1800-v1
  SUPPORTED_DEVICES := plasmacloud,pax1800-v1
  DEVICE_DTS_CONFIG := config@cp03-c1
  CE_TYPE := PAX1800
  DEVICE_PACKAGES += ath11k-wifi-plasmacloud-pax1800
endef
TARGET_DEVICES += plasmacloud_pax1800-v1

define Device/plasmacloud_pax1800-v2
  $(Device/plasmacloud_common_64k)
  DEVICE_TITLE := Plasma Cloud PAX1800 v2
  DEVICE_DTS := qcom-ipq6018-pax1800-v2
  SUPPORTED_DEVICES := plasmacloud,pax1800-v2
  DEVICE_DTS_CONFIG := config@plasmacloud.pax1800v2
  CE_TYPE := PAX1800v2
  DEVICE_PACKAGES += ath11k-wifi-plasmacloud-pax1800
endef
TARGET_DEVICES += plasmacloud_pax1800-v2

define Device/meshpp_s618_cp03
  DEVICE_TITLE := S618 cp03
  DEVICE_DTS := qcom-ipq6018-meshpp-s618-cp03
  SUPPORTED_DEVICES := meshpp,s618-cp03
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-meshpp-s618 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += meshpp_s618_cp03

define Device/meshpp_s618_cp01
  DEVICE_TITLE := S618 cp01
  DEVICE_DTS := qcom-ipq6018-meshpp-s618-cp01
  SUPPORTED_DEVICES := meshpp,s618-cp01
  DEVICE_DTS_CONFIG := config@cp01-c1
  DEVICE_PACKAGES := ath11k-wifi-meshpp-s618 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += meshpp_s618_cp01

define Device/yuncore_fap650
  DEVICE_TITLE := YunCore FAP 650
  DEVICE_DTS := qcom-ipq6018-yuncore-fap650
  SUPPORTED_DEVICES := yuncore,fap650
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-yuncore-fap650
endef
TARGET_DEVICES += yuncore_fap650

