#
# MT7621 Profiles
#

DEFAULT_SOC := mt7621

DEVICE_VARS += BUFFALO_TRX_MAGIC

define Image/Prepare
	# For UBI we want only one extra block
	rm -f $(KDIR)/ubi_mark
	echo -ne '\xde\xad\xc0\xde' > $(KDIR)/ubi_mark
endef

define Device/sonicfi_rap63xc-211g
  $(Device/dsa-migration)
  IMAGE_SIZE := 15808k
  DEVICE_VENDOR := sonicfi
  DEVICE_MODEL := RAP63XC-211G
  DEVICE_PACKAGES := kmod-mt7915-firmware uboot-envtools
endef
TARGET_DEVICES += sonicfi_rap63xc-211g
