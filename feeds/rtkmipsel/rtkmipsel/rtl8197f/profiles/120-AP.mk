#
# Copyright (C) 2006-2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/AP
  NAME:=AP package
  PACKAGES:=-wpad-mini
endef

define Profile/AP/Description
	Realtek SOC,Package AP mode support
endef

$(eval $(call Profile,AP))
