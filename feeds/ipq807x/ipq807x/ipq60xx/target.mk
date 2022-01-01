SUBTARGET:=ipq60xx
BOARDNAME:=IPQ60xx based boards

DEFAULT_PACKAGES += ath11k-firmware-ipq60xx qca-nss-fw-ipq60xx

define Target/Description
        Build images for IPQ60xx systems.
endef
