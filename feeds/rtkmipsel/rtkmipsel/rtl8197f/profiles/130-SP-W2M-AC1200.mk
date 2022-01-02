#
# Copyright (C) 2006-2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/SP-W2M-AC1200
  NAME:=SparkWave2 mini
  PACKAGES:=-wpad-mini
endef

define Profile/SP-W2M-AC1200/Description
	SparkWave2 mini 8197F, both microusb and POE models
endef

define Profile/SP-W2M-AC1200/Config
config KERNEL_RTL_8197F_SP_W2M_AC1200
	bool
	default y
	depends on TARGET_rtkmipsel_rtl8197f_SP-W2M-AC1200
endef

$(eval $(call Profile,SP-W2M-AC1200))
