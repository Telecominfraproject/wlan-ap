KERNEL_LOADADDR := 0x40000000

define Device/cig_wf197
  DEVICE_TITLE := CIG WF197
  DEVICE_DTS := ipq5424-cig-wf197
  DEVICE_DTS_DIR := ../dts
  # TODO update the config and the proper images
  DEVICE_DTS_CONFIG := config@mr01.1
  IMAGES := sysupgrade.bin nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf197 ath12k-firmware-qcn92xx ath12k-firmware-ipq5424
endef
TARGET_DEVICES += cig_wf197
