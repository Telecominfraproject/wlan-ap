
define Device/qcom_maxx
	$(call Device/MultiDTBFitImage)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-MAXX
	DEVICE_VARIANT :=
	BOARD_NAME := ap-maxx
	SOC := ipq5424
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),9216k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += qcom_maxx

define Device/qcom_rdp466
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP466
	DEVICE_VARIANT := AP-MR02.1
	BOARD_NAME := ap-rdp466
	BUILD_DTS_ipq5424-rdp466 := 1
	SOC := ipq5424
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,7000k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_6_1)$(CONFIG_LINUX_6_6), qcom_rdp466)

define Device/qcom_rdp485-c4
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP485-C4
	DEVICE_VARIANT := AP-MR02.2
	BOARD_NAME := ap-rdp485-c4
	BUILD_DTS_ipq5424-rdp485-c4 := 1
	SOC := ipq5424
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,7000k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += qcom_rdp485-c4

define Device/emulation
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL :=
	DEVICE_VARIANT :=
	BOARD_NAME :=
	BUILD_DTS_ipq5424-emulation := 1
	SOC := ipq5424
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
