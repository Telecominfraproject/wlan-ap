KERNEL_LOADADDR := 0x41208000

define Device/qcom_mp03_3
  DEVICE_TITLE := Qualcomm Maple 03.3
  DEVICE_DTS := qcom-ipq5018-mp03.3
  SUPPORTED_DEVICES := qcom,ipq5018-mp03.3
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq5018
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += qcom_mp03_3
