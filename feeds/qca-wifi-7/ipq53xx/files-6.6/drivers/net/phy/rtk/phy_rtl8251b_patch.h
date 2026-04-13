/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */
#include <linux/module.h>
#include <linux/phy.h>
#include "type.h"

#ifndef __PHY_RTL8251B_PATCH_H__
#define __PHY_RTL8251B_PATCH_H__

#ifndef SUCCESS
#define SUCCESS     1
#endif
#ifndef FAILURE
#define FAILURE     0
#endif

#ifndef BOOL
#define BOOL            uint32
#endif
#ifndef UINT32
#define UINT32          uint32
#endif
#ifndef UINT16
#define UINT16          uint16
#endif
#ifndef UINT8
#define UINT8           uint8
#endif

#define MMD_PMAPMD     1
#define MMD_PCS        3
#define MMD_AN         7
#define MMD_VEND1      30   /* Vendor specific 2 */
#define MMD_VEND2      31   /* Vendor specific 2 */


typedef struct
{
    UINT16 dev;
    UINT16 addr;
    UINT16 value;
} MMD_REG;

int32 MmdPhyRead(struct phy_device *phydev,UINT16 dev,UINT16 addr,UINT16 *data);
int32 MmdPhyWrite(struct phy_device *phydev,UINT16 dev,UINT16 addr,UINT16 data);

extern int32 rtl8251b_phy_init(struct phy_device *phydev);

#endif




