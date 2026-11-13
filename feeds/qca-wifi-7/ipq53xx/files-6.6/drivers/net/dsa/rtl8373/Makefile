# SPDX-License-Identifier: GPL-2.0

MODULE_NAME = rtl8373_dsa

obj-$(CONFIG_NET_DSA_REALTEK_RTL8373_SMI) += $(MODULE_NAME).o
$(MODULE_NAME)-objs 				:= rtl8373_lut.o rtl8373_vlan.o rtl8373_mib.o rtl8373_switch.o rtl8373_interrupt.o rtl8373.o rtl8373_port.o rtl8373_eee.o realtek-smi-rtl8373.o
obj-$(CONFIG_NET_DSA_REALTEK_RTL8373_PHY) += rtl8373_core.o rtl8373_phy.o

MODULE_NAME = rtl8373_dsa_i2c
obj-$(CONFIG_NET_DSA_REALTEK_RTL8373_I2C) += $(MODULE_NAME).o
$(MODULE_NAME)-objs  				:=  rtl8373_lut.o rtl8373_vlan.o rtl8373_mib.o rtl8373_switch.o rtl8373_interrupt.o rtl8373.o rtl8373_phy.o rtl8373_port.o rtl8373_eee.o rtl8373_i2c.o
obj-$(CONFIG_NET_DSA_REALTEK_RTL8373_PHY) += rtl8373_core.o rtl8373_phy.o
