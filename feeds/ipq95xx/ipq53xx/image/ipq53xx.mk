KERNEL_LOADADDR := 0x40080000

define Device/cig_wf189
  DEVICE_TITLE := CIG WF189
  DEVICE_DTS := ipq5332-cig-wf189
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf189 ath12k-firmware-qcn92xx-split-phy ath12k-firmware-ipq53xx
endef
TARGET_DEVICES += cig_wf189

define Device/sercomm_ap72tip
  DEVICE_TITLE := Sercomm AP72 TIP
  DEVICE_DTS := ipq5332-sercomm-ap72tip
  DEVICE_DTS_CONFIG := config@mi01.2-qcn9160-c1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sercomm-ap72tip ath12k-firmware-qcn92xx-split-phy ath12k-firmware-ipq53xx
endef
TARGET_DEVICES += sercomm_ap72tip

define Device/edgecore_eap105
  DEVICE_TITLE := Edgecore EAP105
  DEVICE_DTS := ipq5332-edgecore-eap105
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-edgecore-eap105 ath12k-firmware-qcn92xx-split-phy ath12k-firmware-ipq53xx
endef
TARGET_DEVICES += edgecore_eap105

define Device/sonicfi_rap7110c_341x
  DEVICE_TITLE := SONICFI RAP7110C-341X
  DEVICE_DTS := ipq5332-sonicfi-rap7110c-341x
  DEVICE_DTS_CONFIG := config@mi01.6
  SUPPORTED_DEVICES := sonicfi,rap7110c-341x
  IMAGES := sysupgrade.tar mmc-factory.bin
  IMAGE/mmc-factory.bin := append-ubi | qsdk-ipq-factory-mmc
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := ath12k-wifi-sonicfi-rap7110c-341x ath12k-firmware-qcn92xx-split-phy ath12k-firmware-ipq53xx
endef
TARGET_DEVICES += sonicfi_rap7110c_341x
