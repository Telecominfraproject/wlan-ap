# Source configuration for wlan-hostapd
PKG_SOURCE_DATE:=2025-03-10
PKG_SOURCE_VERSION:=95ad71157e3095c52d5212855ca232832b1b5085
# PKG_MIRROR_HASH:=5d1b2299f52e30bc33cdcbbce5f55be41bb761ccb785e8dc0f47f7be9ede5053
PKG_MIRROR_HASH:=skip
# Apply Openwrt patches from here to resolve cross-component dependency.
PATCH_DIR:=$(TOPDIR)/openwrt-patches/package/network/services/hostapd/openwrt_patches
