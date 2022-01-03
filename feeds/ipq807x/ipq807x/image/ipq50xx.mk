KERNEL_LOADADDR := 0x41208000

define Device/cybertan_eww622_a1
  DEVICE_TITLE := CyberTan EWW622-A1
  DEVICE_DTS := qcom-ipq5018-eww622-a1
  SUPPORTED_DEVICES := cybertan,eww622-a1
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq5018 ath11k-wifi-qcom-qcn9000 ath11k-firmware-ipq50xx ath11k-firmware-qcn9000
  DEVICE_DTS_CONFIG := config@mp03.1
endef
TARGET_DEVICES += cybertan_eww622_a1

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
