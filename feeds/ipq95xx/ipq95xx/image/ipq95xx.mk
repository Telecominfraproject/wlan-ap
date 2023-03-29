KERNEL_LOADADDR := 0x42080000

define Device/qcom_al02-c4
  DEVICE_TITLE := Qualcomm AP-AL02-C4
  DEVICE_DTS := ipq9574-al02-c4
  DEVICE_DTS_CONFIG := config@al02-c4
  SUPPORTED_DEVICES := qcom,ipq9574-ap-al02-c4
# DEVICE_PACKAGES := ath11k-wifi-tplink-ex227
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
endef
TARGET_DEVICES += qcom_al02-c4
