
define Device/qcom_mixx
        $(call Device/MultiDTBFitImage)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-MIXX
	DEVICE_VARIANT :=
	BOARD_NAME := ap-mixx
	SOC := ipq5332
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += qcom_mixx

define Device/qcom_rdp441
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP441
	DEVICE_VARIANT := AP-MI01.2
	BOARD_NAME := ap-mi01.2
	BUILD_DTS_ipq5332-rdp441 := 1
	SOC := ipq5332
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_6_1), qcom_rdp441)

define Device/qcom_rdp468
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP468
	DEVICE_VARIANT := AP-MI01.6
	BOARD_NAME := ap-mi01.6
	BUILD_DTS_ipq5332-rdp468 := 1
	SOC := ipq5332
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_6_1), qcom_rdp468)

define Device/qcom_rdp442
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP442
	DEVICE_VARIANT := AP-MI01.3
	BOARD_NAME := ap-mi01.3
	BUILD_DTS_ipq5332-rdp442 := 1
	SOC := ipq5332
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_6_1), qcom_rdp442)
