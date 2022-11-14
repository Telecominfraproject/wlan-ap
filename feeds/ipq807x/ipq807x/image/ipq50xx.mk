KERNEL_LOADADDR := 0x41208000

define Device/cybertan_eww622_a1
  DEVICE_TITLE := CyberTan EWW622-A1
  DEVICE_DTS := qcom-ipq5018-eww622-a1
  SUPPORTED_DEVICES := cybertan,eww622-a1
  DEVICE_PACKAGES := ath11k-wifi-cybertan-eww622-a1 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += cybertan_eww622_a1

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
