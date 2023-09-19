KERNEL_LOADADDR := 0x41080000

define Device/cig_wf186w
  DEVICE_TITLE := Cigtech WF-186w
  DEVICE_DTS := qcom-ipq5018-cig-wf186w
  SUPPORTED_DEVICES := cig,wf186w
  DEVICE_PACKAGES := ath11k-wifi-cig-wf186w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += cig_wf186w

define Device/edgecore_eap104
  DEVICE_TITLE := EdgeCore EAP104
  DEVICE_DTS := qcom-ipq5018-eap104
  SUPPORTED_DEVICES := edgecore,eap104
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap104 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_eap104

define Device/yuncore_fap655
  DEVICE_TITLE := Yuncore FAP650
  DEVICE_DTS := qcom-ipq5018-yuncore-fap655
  SUPPORTED_DEVICES := yuncore,fap655
  DEVICE_PACKAGES := ath11k-wifi-yuncore-fap655 ath11k-firmware-ipq50xx-map-spruce -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += yuncore_fap655

define Device/edgecore_oap101
  DEVICE_TITLE := EdgeCore OAP101
  DEVICE_DTS := qcom-ipq5018-oap101
  SUPPORTED_DEVICES := edgecore,oap101
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101

define Device/edgecore_oap101_6e
  DEVICE_TITLE := EdgeCore OAP101 6E
  DEVICE_DTS := qcom-ipq5018-oap101-6e
  SUPPORTED_DEVICES := edgecore,oap101-6e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101 ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101_6e

define Device/edgecore_oap101e
  DEVICE_TITLE := EdgeCore OAP101 E
  DEVICE_DTS := qcom-ipq5018-oap101e
  SUPPORTED_DEVICES := edgecore,oap101e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101e ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101e

define Device/edgecore_oap101e_6e
  DEVICE_TITLE := EdgeCore OAP101 6E E
  DEVICE_DTS := qcom-ipq5018-oap101e-6e
  SUPPORTED_DEVICES := edgecore,oap101e-6e
  DEVICE_PACKAGES := ath11k-wifi-edgecore-oap101e ath11k-firmware-ipq50xx-spruce ath11k-firmware-qcn6122 kmod-hwmon-tmp102 kmod-gpio-pca953x ugps kmod-tpm-tis-i2c
  DEVICE_DTS_CONFIG := config@mp03.5-c1
endef
TARGET_DEVICES += edgecore_oap101e_6e

define Device/hfcl_ion4xi_w
  DEVICE_TITLE := HFCL ION4xi_w
  DEVICE_DTS := qcom-ipq5018-hfcl-ion4xi_w
  SUPPORTED_DEVICES := hfcl,ion4xi_w
  DEVICE_PACKAGES := ath11k-wifi-hfcl-ion4xi_w ath11k-firmware-ipq50xx-map-spruce
  DEVICE_DTS_CONFIG := config@mp03.3
endef
TARGET_DEVICES += hfcl_ion4xi_w
