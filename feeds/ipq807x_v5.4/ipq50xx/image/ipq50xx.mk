KERNEL_LOADADDR := 0x41080000

define Device/cig_wf186h
  DEVICE_TITLE := Cigtech WF-186h
  DEVICE_DTS := qcom-ipq5018-cig-wf186h
  SUPPORTED_DEVICES := cig,wf186h
  DEVICE_PACKAGES := ath11k-wifi-cig-wf186h ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += cig_wf186h

define Device/cig_wf186w
  DEVICE_TITLE := Cigtech WF-186w
  DEVICE_DTS := qcom-ipq5018-cig-wf186w
  SUPPORTED_DEVICES := cig,wf186w
  DEVICE_PACKAGES := ath11k-wifi-cig-wf186w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += cig_wf186w

define Device/cybertan_eww631_a1
  DEVICE_TITLE := CyberTan EWW631-A1
  DEVICE_DTS := qcom-ipq5018-eww631-a1
  SUPPORTED_DEVICES := cybertan,eww631-a1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww631-a1 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += cybertan_eww631_a1

define Device/cybertan_eww631_b1
  DEVICE_TITLE := CyberTan EWW631-B1
  DEVICE_DTS := qcom-ipq5018-eww631-b1
  SUPPORTED_DEVICES := cybertan,eww631-b1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww631-b1 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += cybertan_eww631_b1

define Device/sonicfi_rap630w_312g
  DEVICE_TITLE := Sonicfi RAP630W-312G
  DEVICE_DTS := qcom-ipq5018-rap630w-312g
  SUPPORTED_DEVICES := sonicfi,rap630w-312g
  DEVICE_PACKAGES := ath11k-wifi-sonicfi-rap630w-312g ath11k-firmware-ipq50xx-map-spruce \
                     -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3 \
                     kmod-usb-uas kmod-fs-msdos kmod-fs-ntfs
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += sonicfi_rap630w_312g

define Device/sonicfi_rap630c_311g
  DEVICE_TITLE := Sonicfi RAP630C-311G
  DEVICE_DTS := qcom-ipq5018-rap630c-311g
  SUPPORTED_DEVICES := sonicfi,rap630c-311g
  DEVICE_PACKAGES := ath11k-wifi-sonicfi-rap630c-311g ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += sonicfi_rap630c_311g

define Device/sonicfi_rap630w_311g
  DEVICE_TITLE := Sonicfi RAP630W-311G
  DEVICE_DTS := qcom-ipq5018-rap630w-311g
  SUPPORTED_DEVICES := sonicfi,rap630w-311g
  DEVICE_PACKAGES := ath11k-wifi-sonicfi-rap630w-311g ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += sonicfi_rap630w_311g

define Device/sonicfi_rap630e
  DEVICE_TITLE := Sonicfi RAP630E
  DEVICE_DTS := qcom-ipq5018-rap630e
  SUPPORTED_DEVICES := sonicfi,rap630e
  DEVICE_PACKAGES := ath11k-wifi-sonicfi-rap630e ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += sonicfi_rap630e

define Device/edgecore_eap104
  DEVICE_TITLE := EdgeCore EAP104
  DEVICE_DTS := qcom-ipq5018-eap104
  SUPPORTED_DEVICES := edgecore,eap104
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap104 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_eap104

define Device/indio_um-325ax-v2
  DEVICE_TITLE := Indio UM-325ax-V2
  DEVICE_DTS := qcom-ipq5018-indio-um-325ax-v2
  SUPPORTED_DEVICES := indio,um-325ax-v2
  DEVICE_PACKAGES := ath11k-wifi-indio-um-325ax-v2 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += indio_um-325ax-v2

define Device/indio_um-335ax
  DEVICE_TITLE := Indio UM-335ax
  DEVICE_DTS := qcom-ipq5018-indio-um-335ax
  SUPPORTED_DEVICES := indio,um-335ax
  DEVICE_PACKAGES := ath11k-wifi-indio-um-335ax ath11k-firmware-qcn9000 ath11k-firmware-ipq50xx-spruce
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += indio_um-335ax

define Device/indio_um-525axp
  DEVICE_TITLE := Indio UM-525axp
  DEVICE_DTS := qcom-ipq5018-indio-um-525axp
  SUPPORTED_DEVICES := indio,um-525axp
  DEVICE_PACKAGES := ath11k-wifi-indio-um-525axp ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += indio_um-525axp

define Device/indio_um-525axm
  DEVICE_TITLE := Indio UM-525axm
  DEVICE_DTS := qcom-ipq5018-indio-um-525axm
  SUPPORTED_DEVICES := indio,um-525axm
  DEVICE_PACKAGES := ath11k-wifi-indio-um-525axm ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += indio_um-525axm

define Device/udaya_a6_id2
  DEVICE_TITLE := Udaya A6 - ID2
  DEVICE_DTS := qcom-ipq5018-udaya-a6-id2
  SUPPORTED_DEVICES := udaya,a6-id2
  DEVICE_PACKAGES := ath11k-wifi-udaya-a6-id2 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += udaya_a6_id2

define Device/udaya_a6_od2
  DEVICE_TITLE := Udaya A6 - OD2
  DEVICE_DTS := qcom-ipq5018-udaya-a6-od2
  SUPPORTED_DEVICES := udaya,a6-od2
  DEVICE_PACKAGES := ath11k-wifi-udaya-a6-od2 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += udaya_a6_od2

define Device/wallys_dr5018
  DEVICE_TITLE := Wallys DR5018
  DEVICE_DTS := qcom-ipq5018-wallys-dr5018
  SUPPORTED_DEVICES := wallys,dr5018
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr5018 uboot-envtools ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += wallys_dr5018

define Device/yuncore_fap655
  DEVICE_TITLE := Yuncore FAP650
  DEVICE_DTS := qcom-ipq5018-yuncore-fap655
  SUPPORTED_DEVICES := yuncore,fap655
  DEVICE_PACKAGES := ath11k-wifi-yuncore-fap655 ath11k-firmware-ipq50xx-map-spruce -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += yuncore_fap655

define Device/edgecore_oap101
  DEVICE_TITLE := EdgeCore OAP101
  DEVICE_DTS := qcom-ipq5018-oap101
  SUPPORTED_DEVICES := edgecore,oap101
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101

define Device/edgecore_oap101_6e
  DEVICE_TITLE := EdgeCore OAP101 6E
  DEVICE_DTS := qcom-ipq5018-oap101-6e
  SUPPORTED_DEVICES := edgecore,oap101-6e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101_6e

define Device/edgecore_oap101e
  DEVICE_TITLE := EdgeCore OAP101 E
  DEVICE_DTS := qcom-ipq5018-oap101e
  SUPPORTED_DEVICES := edgecore,oap101e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101e ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101e

define Device/edgecore_oap101e_6e
  DEVICE_TITLE := EdgeCore OAP101 6E E
  DEVICE_DTS := qcom-ipq5018-oap101e-6e
  SUPPORTED_DEVICES := edgecore,oap101e-6e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101e ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101e_6e

define Device/emplus_wap385c
  DEVICE_TITLE := Emplus WAP385C
  DEVICE_DTS := qcom-ipq5018-emplus-wap385c
  SUPPORTED_DEVICES := emplus,wap385c
  DEVICE_PACKAGES := ath11k-wifi-emplus-wap385c ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += emplus_wap385c

define Device/hfcl_ion4x_w
  DEVICE_TITLE := HFCL ION4x_w
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4x_w
  SUPPORTED_DEVICES := hfcl,ion4x_w
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4x_w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4x_w

define Device/hfcl_ion4xi_w
  DEVICE_TITLE := HFCL ION4xi_w
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4xi_w
  SUPPORTED_DEVICES := hfcl,ion4xi_w
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4xi_w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4xi_w

define Device/optimcloud_d50-5g
  DEVICE_TITLE := OptimCloud D50-5G
  DEVICE_DTS := qcom-ipq5018-optimcloud-d50-5g
  SUPPORTED_DEVICES := optimcloud,d50-5g
  DEVICE_PACKAGES := ath11k-wifi-optimcloud-d50 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += optimcloud_d50-5g

define Device/optimcloud_d50
  DEVICE_TITLE := OptimCloud D50
  DEVICE_DTS := qcom-ipq5018-optimcloud-d50
  SUPPORTED_DEVICES := optimcloud,d50
  DEVICE_PACKAGES := ath11k-wifi-optimcloud-d50 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000 ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += optimcloud_d50

define Device/optimcloud_d60-5g
  DEVICE_TITLE := OptimCloud D60-5G
  DEVICE_DTS := qcom-ipq5018-optimcloud-d60-5g
  SUPPORTED_DEVICES := optimcloud,d60-5g
  DEVICE_PACKAGES := ath11k-wifi-optimcloud-d60 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += optimcloud_d60-5g

define Device/optimcloud_d60
  DEVICE_TITLE := OptimCloud D60
  DEVICE_DTS := qcom-ipq5018-optimcloud-d60
  SUPPORTED_DEVICES := optimcloud,d60
  DEVICE_PACKAGES := ath11k-wifi-optimcloud-d60 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000 ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += optimcloud_d60

define Device/glinet_b3000
  DEVICE_TITLE := GL.iNet B3000
  DEVICE_DTS := qcom-ipq5018-gl-b3000
  SUPPORTED_DEVICES := glinet,b3000
  DEVICE_PACKAGES := ath11k-wifi-gl-b3000 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
  IMAGES := sysupgrade.tar nand-factory.bin
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
endef
TARGET_DEVICES += glinet_b3000

define Device/emplus_wap581
  DEVICE_TITLE := Emplus WAP581
  DEVICE_DTS := qcom-ipq5018-emplus-wap581
  SUPPORTED_DEVICES := emplus,wap581
  DEVICE_PACKAGES := ath11k-wifi-emplus-wap581 ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
  IMAGES := sysupgrade.tar nand-factory.bin
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand  
endef
TARGET_DEVICES += emplus_wap581

