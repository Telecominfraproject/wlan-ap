KERNEL_LOADADDR := 0x40000000

define Device/cig_wf189
  DEVICE_TITLE := CIG WF189
  DEVICE_DTS := ipq5332-cig-wf189
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf189 ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += cig_wf189

define Device/sercomm_ap72tip
  DEVICE_TITLE := Sercomm AP72TIP
  DEVICE_DTS := ipq5332-sercomm-ap72tip
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sercomm-ap72tip ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += sercomm_ap72tip

define Device/sercomm_ap72tip-v4
  DEVICE_TITLE := Sercomm AP72TIP-v4
  DEVICE_DTS := ipq5332-sercomm-ap72tip-v4
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sercomm-ap72tip-v4 ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += sercomm_ap72tip-v4

define Device/edgecore_eap105
  DEVICE_TITLE := Edgecore EAP105
  DEVICE_DTS := ipq5332-edgecore-eap105
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-edgecore-eap105 ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += edgecore_eap105

define Device/sonicfi_rap7110c_341x
  DEVICE_TITLE := SONICFI RAP7110C-341X
  DEVICE_DTS := ipq5332-sonicfi-rap7110c-341x
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  SUPPORTED_DEVICES := sonicfi,rap7110c-341x
  IMAGES := sysupgrade.tar mmc-factory.bin
  IMAGE/mmc-factory.bin := append-ubi | qsdk-ipq-factory-mmc
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := ath12k-wifi-sonicfi-rap7110c-341x ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += sonicfi_rap7110c_341x

define Device/sonicfi_rap750e_h
  DEVICE_TITLE := SONICFI RAP750E-H
  DEVICE_DTS := ipq5332-sonicfi-rap750e-h
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.3-c2
  SUPPORTED_DEVICES := sonicfi,rap750e-h
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sonicfi-rap750e-h ath12k-firmware-ipq5332-peb -ath12k-firmware-qcn92xx
endef
TARGET_DEVICES += sonicfi_rap750e_h

define Device/sonicfi_rap750e_s
  DEVICE_TITLE := SONICFI RAP750E-S
  DEVICE_DTS := ipq5332-sonicfi-rap750e-s
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.3-c2
  SUPPORTED_DEVICES := sonicfi,rap750e-s
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sonicfi-rap750e-s ath12k-firmware-ipq5332-peb -ath12k-firmware-qcn92xx
endef
TARGET_DEVICES += sonicfi_rap750e_s

define Device/sonicfi_rap750w_311a
  DEVICE_TITLE := SONICFI RAP750W-311A
  DEVICE_DTS := ipq5332-sonicfi-rap750w-311a
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.3-c2
  SUPPORTED_DEVICES := sonicfi,rap750w-311a
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-sonicfi-rap750w-311a ath12k-firmware-ipq5332-peb -ath12k-firmware-qcn92xx
endef
TARGET_DEVICES += sonicfi_rap750w_311a

define Device/cig_wf189w
  DEVICE_TITLE := CIG WF189W
  DEVICE_DTS := ipq5332-cig-wf189w
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi04.1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf189w ath12k-firmware-ipq5332-peb-peb
endef
TARGET_DEVICES += cig_wf189w


define Device/cig_wf189h
  DEVICE_TITLE := CIG WF189H
  DEVICE_DTS := ipq5332-cig-wf189h
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi04.1
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-cig-wf189h ath12k-firmware-ipq5332-peb-peb
endef
TARGET_DEVICES += cig_wf189h

define Device/zyxel_nwa130be
  DEVICE_TITLE := Zyxel NWA130BE
  DEVICE_DTS := ipq5332-zyxel-nwa130be
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_CONFIG := config@mi01.6
  IMAGES := sysupgrade.tar nand-factory.bin nand-factory.ubi
  BLOCKSIZE := 256k
  PAGESIZE := 4096
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  IMAGE/nand-factory.ubi := append-ubi
  DEVICE_PACKAGES := ath12k-wifi-zyxel-nwa130be ath12k-firmware-qcn92xx ath12k-firmware-ipq5332
endef
TARGET_DEVICES += zyxel_nwa130be
