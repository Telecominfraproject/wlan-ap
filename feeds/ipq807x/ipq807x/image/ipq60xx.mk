KERNEL_LOADADDR := 0x41008000

define Device/cig_wf188n
  DEVICE_TITLE := Cigtech WF-188n
  DEVICE_DTS := qcom-ipq6018-cig-wf188n
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := cig,wf188n
  DEVICE_PACKAGES := ath11k-wifi-cig-wf188n uboot-env
endef
TARGET_DEVICES += cig_wf188n

define Device/hfcl_ion4xe
  DEVICE_TITLE := HFCL ION4Xe
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4xe
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4xe
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018 uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4xe

define Device/hfcl_ion4xi
  DEVICE_TITLE := HFCL ION4Xi
  DEVICE_DTS := qcom-ipq6018-hfcl-ion4xi
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := hfcl,ion4xi
  DEVICE_PACKAGES := ath11k-wifi-qcom-ipq6018 uboot-envtools
endef
TARGET_DEVICES += hfcl_ion4xi

define Device/edgecore_eap101
  DEVICE_TITLE := EdgeCore EAP101
  DEVICE_DTS := qcom-ipq6018-edgecore-eap101
  DEVICE_DTS_CONFIG := config@cp01-c1
  SUPPORTED_DEVICES := edgecore,eap101
  DEVICE_PACKAGES := ath11k-wifi-edgecore-eap101 uboot-envtools -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3 kmod-usb2
endef
TARGET_DEVICES += edgecore_eap101

define Device/wallys_dr6018
  DEVICE_TITLE := Wallys DR6018
  DEVICE_DTS := qcom-ipq6018-wallys-dr6018
  DEVICE_DTS_CONFIG := config@cp01-c4
  SUPPORTED_DEVICES := wallys,dr6018
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr6018 uboot-envtools
endef
TARGET_DEVICES += wallys_dr6018

define Device/wallys_dr6018_v4
  DEVICE_TITLE := Wallys DR6018 V4
  DEVICE_DTS := qcom-ipq6018-wallys-dr6018-v4
  DEVICE_DTS_CONFIG := config@cp01-c4
  SUPPORTED_DEVICES := wallys,dr6018-v4
  DEVICE_PACKAGES := ath11k-wifi-wallys-dr6018-v4 uboot-envtools
endef
TARGET_DEVICES += wallys_dr6018_v4

define Device/glinet_ax1800
  DEVICE_TITLE := GL-iNet AX1800
  DEVICE_DTS := qcom-ipq6018-gl-ax1800
  SUPPORTED_DEVICES := glinet,ax1800
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-gl-ax1800 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += glinet_ax1800

define Device/glinet_axt1800
  DEVICE_TITLE := GL-iNet AXT1800
  DEVICE_DTS := qcom-ipq6018-gl-axt1800
  SUPPORTED_DEVICES := glinet,axt1800
  DEVICE_DTS_CONFIG := config@cp03-c1
  DEVICE_PACKAGES := ath11k-wifi-gl-axt1800 -kmod-usb-dwc3-of-simple kmod-usb-dwc3-qcom kmod-usb3
endef
TARGET_DEVICES += glinet_axt1800

define Device/yuncore_ax840
  DEVICE_TITLE := YunCore AX840
  DEVICE_DTS := qcom-ipq6018-yuncore-ax840
  DEVICE_DTS_CONFIG := config@cp03-c1
  SUPPORTED_DEVICES := yuncore,ax840
  DEVICE_PACKAGES := ath11k-wifi-yuncore-ax840 uboot-env
endef
TARGET_DEVICES += yuncore_ax840
