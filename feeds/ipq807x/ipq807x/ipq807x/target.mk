SUBTARGET:=ipq807x
BOARDNAME:=IPQ807x based boards

DEFAULT_PACKAGES += ath11k-firmware-ipq807x qca-nss-fw-ipq807x
define Target/Description
        Build images for IPQ807x systems.
endef
