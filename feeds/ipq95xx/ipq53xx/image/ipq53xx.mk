KERNEL_LOADADDR := 0x40080000

define Device/cig_wf198
  DEVICE_TITLE := CIG WF198
  DEVICE_DTS := ipq5332-cig-wf198
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf198
endef
TARGET_DEVICES += cig_wf198
