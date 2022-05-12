SUBTARGET:=ipq60xx
BOARDNAME:=IPQ60xx based boards

KERNEL_PATCHVER:=5.4

DEFAULT_PACKAGES += ath11k-firmware-ipq60xx qca-nss-fw-ipq60xx

define Target/Description
        Build images for IPQ60xx systems.
endef
