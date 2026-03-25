/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#define REALTEK_PHY_ID_RTL8261N         0x001CCAF3
#define REALTEK_PHY_ID_RTL8264B         0x001CC813
#define REALTEK_PHY_ID_RTL8251B         0x001CC86A
#define REALTEK_PHY_ID_RTL8251BE        0x001CC862
#define REALTEK_PHY_ID_RTL8251B_VB_CG   0x001CC868

#if IS_ENABLED(CONFIG_MACSEC)
int rtk_macsec_init(struct phy_device *phydev);
#else
static inline int rtk_macsec_init(struct phy_device *phydev)
{
	return 0;
}
#endif



