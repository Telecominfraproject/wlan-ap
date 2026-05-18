KERNEL_LOADADDR := 0x80000000

define Device/cig_wf197
  DEVICE_TITLE := CIG WF197
  DEVICE_DTS := ipq5424-cig-wf197
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config-rdp466
  IMAGES := sysupgrade.bin mmc-factory.bin
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGE/mmc-factory.bin := append-ubi | qsdk-ipq-factory-mmc
  DEVICE_PACKAGES := ath12k-wifi-cig-wf197 ath12k-firmware-qcn92xx ath12k-firmware-ipq5424
endef
TARGET_DEVICES += cig_wf197
