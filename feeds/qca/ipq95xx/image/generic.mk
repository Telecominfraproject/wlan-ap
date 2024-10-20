define Device/FitImage
	KERNEL_SUFFIX := -uImage.itb
	KERNEL = kernel-bin | libdeflate-gzip | fit gzip $$(KDIR)/image-$$(DEVICE_DTS).dtb
	KERNEL_NAME := Image
endef

define Device/EmmcImage
	IMAGES += factory.bin sysupgrade.bin
	IMAGE/factory.bin := append-rootfs | pad-rootfs | pad-to 64k
	IMAGE/sysupgrade.bin/squashfs := append-rootfs | pad-to 64k | sysupgrade-tar rootfs=$$$$@ | append-metadata
endef

define Device/qcom_rdp433
	$(call Device/FitImage)
	$(call Device/EmmcImage)
	DEVICE_VENDOR := Qualcomm
	DEVICE_MODEL := IPQ9574-RDP433
	DEVICE_DTS_CONFIG := config-rdp433
	SOC := ipq9574
	DEVICE_PACKAGES += ath12k-firmware-qcn92xx ath12k-wifi-qcom-qcn92xx kmod-ath12k \
		mkf2fs f2fsck kmod-fs-f2fs
endef
TARGET_DEVICES += qcom_rdp433

define Device/prpl_freedom
	$(call Device/FitImage)
	$(call Device/EmmcImage)
	DEVICE_VENDOR := Prpl
	DEVICE_MODEL := Freedom
	DEVICE_DTS := ipq9574-freedom
	DEVICE_DTS_CONFIG := config@al02-c4
	SOC := ipq9574
	DEVICE_PACKAGES += ath12k-firmware-qcn92xx ath12k-wifi-qcom-qcn92xx kmod-ath12k \
		mkf2fs f2fsck kmod-fs-f2fs
endef
TARGET_DEVICES += prpl_freedom


