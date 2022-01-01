
SUBTARGET:=ipq50xx
BOARDNAME:=IPQ50XX
CPU_TYPE:=cortex-a7

DEFAULT_PACKAGES += ath11k-firmware-ipq50xx qca-nss-fw-ipq50xx

define Target/Description
	Build firmware image for IPQ50xx SoC devices.
endef
