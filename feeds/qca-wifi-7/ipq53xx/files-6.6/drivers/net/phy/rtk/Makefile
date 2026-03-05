# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
#

ccflags-y    := -DRTK_PHYDRV_IN_LINUX
#ccflags-y    += -DDEBUG

obj-$(CONFIG_RTK_MSSDK_PHY)	+= rtk-ms-phy.o

rtk-ms-phy-objs     := rtk_phy.o
rtk-ms-phy-objs     += rtk_osal.o

# files from SDK
rtk-ms-phy-objs     += phy_patch.o
rtk-ms-phy-objs     += phy_rtl826xb_patch.o

# rtk phylib
rtk-ms-phy-objs     += rtk_phylib.o
rtk-ms-phy-objs     += rtk_phylib_rtl826xb.o

ifdef CONFIG_MACSEC
rtk-ms-phy-objs     += rtk_macsec.o
rtk-ms-phy-objs     += rtk_phylib_macsec.o
endif
