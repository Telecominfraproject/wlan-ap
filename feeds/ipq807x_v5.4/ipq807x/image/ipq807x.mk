KERNEL_LOADADDR := 0x41080000

define Device/cig_wf194c4
  DEVICE_TITLE := CIG WF194C4
  DEVICE_DTS := qcom-ipq807x-wf194c4
  DEVICE_DTS_CONFIG=config@hk09
  SUPPORTED_DEVICES := cig,wf194c4
  DEVICE_PACKAGES := ath11k-wifi-cig-wf194c4 aq-fw-download uboot-envtools kmod-usb3 kmod-usb2
endef
TARGET_DEVICES += cig_wf194c4

define Device/cig_wf196
  DEVICE_TITLE := CIG WF196
  DEVICE_DTS := qcom-ipq807x-wf196
  DEVICE_DTS_CONFIG=config@hk14
  SUPPORTED_DEVICES := cig,wf196
  BLOCKSIZE := 256k
  PAGESIZE := 4096
  DEVICE_PACKAGES := ath11k-wifi-cig-wf196 aq-fw-download uboot-envtools kmod-usb3 kmod-usb2 \
  	ath11k-firmware-qcn9000
endef
TARGET_DEVICES += cig_wf196

define Device/edgecore_eap102
  DEVICE_TITLE := Edgecore EAP102
  DEVICE_DTS := qcom-ipq807x-eap102
  DEVICE_DTS_CONFIG=config@ac02
  SUPPORTED_DEVICES := edgecore,eap102
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap102 kmod-usb2 kmod-usb3 uboot-envtools
endef
TARGET_DEVICES += edgecore_eap102

define Device/edgecore_oap102
  DEVICE_TITLE := Edgecore OAP102
  DEVICE_DTS := qcom-ipq807x-oap102
  DEVICE_DTS_CONFIG=config@ac02
  SUPPORTED_DEVICES := edgecore,oap102
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap102 kmod-usb2 kmod-usb3 uboot-envtools
endef
TARGET_DEVICES += edgecore_oap102

define Device/edgecore_oap103
  DEVICE_TITLE := Edgecore OAP103
  DEVICE_DTS := qcom-ipq807x-oap103
  DEVICE_DTS_CONFIG=config@ac02
  SUPPORTED_DEVICES := edgecore,oap103
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap103 kmod-usb2 kmod-usb3 uboot-envtools
endef
TARGET_DEVICES += edgecore_oap103

define Device/edgecore_eap106
  DEVICE_TITLE := Edgecore EAP106
  DEVICE_DTS := qcom-ipq807x-eap106
  DEVICE_DTS_CONFIG=config@hk02
  SUPPORTED_DEVICES := edgecore,eap106
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap106 aq-fw-download kmod-usb2 kmod-usb3 uboot-envtools
endef
#TARGET_DEVICES += edgecore_eap106

define Device/sonicfi_rap650c
  DEVICE_TITLE := SonicFi RAP650C
  DEVICE_DTS := qcom-ipq807x-rap650c
  DEVICE_DTS_CONFIG=config@hk09
  SUPPORTED_DEVICES := sonicfi,rap650c
  DEVICE_PACKAGES := ath11k-wifi-sonicfi-rap650c uboot-envtools
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += sonicfi_rap650c

define Device/tplink_ex227
  DEVICE_TITLE := TP-Link EX227
  DEVICE_DTS := qcom-ipq807x-ex227
  DEVICE_DTS_CONFIG=config@hk07
  SUPPORTED_DEVICES := tplink,ex227
  DEVICE_PACKAGES := ath11k-wifi-tplink-ex227
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += tplink_ex227

define Device/tplink_ex447
  DEVICE_TITLE := TP-Link EX447
  DEVICE_DTS := qcom-ipq807x-ex447
  DEVICE_DTS_CONFIG=config@hk09
  SUPPORTED_DEVICES := tplink,ex447
  DEVICE_PACKAGES := ath11k-wifi-tplink-ex447
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += tplink_ex447
