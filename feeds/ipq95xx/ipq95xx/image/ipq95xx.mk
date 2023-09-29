KERNEL_LOADADDR := 0x42080000

define Device/qcom_al02-c4
  DEVICE_TITLE := Qualcomm AP-AL02-C4
  DEVICE_DTS := ipq9574-al02-c4
  DEVICE_DTS_CONFIG := config@al02-c4
  SUPPORTED_DEVICES := qcom,ipq9574-ap-al02-c4
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-qcom-qcn9274
endef
TARGET_DEVICES += qcom_al02-c4

define Device/qcom_al02-c15
  DEVICE_TITLE := Qualcomm AP-AL02-C15
  DEVICE_DTS := ipq9574-al02-c15
  DEVICE_DTS_CONFIG := config@al02-c15
  SUPPORTED_DEVICES := qcom,ipq9574-ap-al02-c15
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq95xx
endef
TARGET_DEVICES += qcom_al02-c15
