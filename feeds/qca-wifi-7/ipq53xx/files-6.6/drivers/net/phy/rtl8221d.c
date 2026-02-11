// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Rtl PHY
 *
 * Author: Huang Yunxiang <huangyunxiang@cigtech.com>
 *
 * Copyright 2024 cig, Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/phy.h>

#define PHY_ID_RTL8221D	0x001CC841
#define BIT_0       0x0001
#define BIT_1       0x0002
#define BIT_2       0x0004
#define BIT_3       0x0008
#define BIT_4       0x0010
#define BIT_5       0x0020
#define BIT_6       0x0040
#define BIT_7       0x0080
#define BIT_8       0x0100
#define BIT_9       0x0200
#define BIT_10      0x0400
#define BIT_11      0x0800
#define BIT_12      0x1000
#define BIT_13      0x2000
#define BIT_14      0x4000
#define BIT_15      0x8000


static int rtl8221d_config_aneg(struct phy_device *phydev)
{
	return 0;
}


static u32 Rtl8226b_is_link(struct phy_device *phydev)
{
    int phydata = 0;

    int i = 0;

    // must read twice
    for(i=0;i<2;i++)
    {
        phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xA402);    
    }

    phydev->link = (phydata & BIT_2) ? (1) : (0);
	return 0;

}

static int rtl8221d_read_status(struct phy_device *phydev)
{
	int phydata, speed_grp, speed;
	
	Rtl8226b_is_link(phydev);
	if (phydev->link)
   	{
		phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xA434);
		speed_grp = (phydata & (BIT_9 | BIT_10)) >> 9;
	        speed = (phydata & (BIT_4 | BIT_5)) >> 4;
		switch(speed_grp)
		{
			case 0:
			{
				switch(speed)
				{
				case 1:
					phydev->speed = SPEED_100;
					break;
				case 2:
					phydev->speed = SPEED_1000;
					break;
				default:
					phydev->speed = SPEED_10;
					break;
				}
				break;
			}
			case 1:
			{
				switch(speed)
				{
				case 1:
					phydev->speed = SPEED_2500;
					break;
				case 3:
					phydev->speed = SPEED_1000;        // 2.5G lite
					break;
				default:
					phydev->speed = SPEED_10;
					break;
				}
				break;
			}
			default:
				phydev->speed = SPEED_10;
				break;
        }
    }
    else
    {
		phydev->speed = SPEED_10;
    }
	
	phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xA434);
	phydev->duplex = (phydata & BIT_3) ? (DUPLEX_FULL) : (DUPLEX_HALF);
	
	return 0;
}

static int rtl8221d_config_init(struct phy_device *phydev)
{
	int err;
	int phydata;
	u16 timeoutms = 100;

	err = phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x7588, 0x2);
	if (err < 0)
		return err;
	err = phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x7589, 0x71d0);
	if (err < 0)
		return err;
	err = phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x7587, 0x3);
	if (err < 0)
		return err;
	while(--timeoutms)
	{
		phydata = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x7587);
		if((phydata & 0x01) == 0)
			break;
		mdelay(10);
	}

	phydata = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x75F3);
        phydata &= ~BIT_0;
        err = phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x75F3, phydata);
        if (err < 0)
                return err;
	phydata = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x697A);
	phydata &= (~(BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4 | BIT_5));
	phydata |= 2;
	phydata |=0x8000;
        err = phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x697A, phydata);
        if (err < 0)
                return err;

	phydata = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x697A);

	Rtl8226b_is_link(phydev);
	if (phydev->link)
	{
		phydata = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, 0x0);
		phydata |= BIT_15;
		err = phy_write_mmd(phydev, MDIO_MMD_PMAPMD, 0x0, phydata);
		if (err < 0)
			return err;

		while(--timeoutms)
		{
			phydata = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, 0x0);
			if (!(phydata & BIT_15))
           			 break;
			mdelay(10);
		}
		err = phy_write_mmd(phydev, MDIO_MMD_PMAPMD, 0x0, phydata);
		if (err < 0)
			return err;
	}
	else
	{
		phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xa400);
		phydata |= BIT_14;
		err = phy_write_mmd(phydev, MDIO_MMD_VEND2, 0xa400, phydata);
		if (err < 0)
			return err;
		while(--timeoutms)
                {
                        phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xA434);
                        if (phydata & BIT_2)
                                 break;
                        mdelay(10);
                }
		phydata = phy_read_mmd(phydev, MDIO_MMD_VEND2, 0xa400);
		phydata &= ~BIT_14;
                err = phy_write_mmd(phydev, MDIO_MMD_VEND2, 0xa400, phydata);
		if (err < 0)
			return err;
	}
	return 0;
}

static int rtl8221d_probe(struct phy_device *phydev)
{
	printk("rtl8221d probe");
	return 0;
}

static struct phy_driver aqr_driver[] = {
{
	PHY_ID_MATCH_MODEL(PHY_ID_RTL8221D),
	.name		= "Rtl 8221D",
	.features	= PHY_GBIT_FEATURES,
	.probe		= rtl8221d_probe,
	.config_init	= rtl8221d_config_init,
	.config_aneg    = rtl8221d_config_aneg,
	.read_status	= rtl8221d_read_status,
},
};

module_phy_driver(aqr_driver);

static struct mdio_device_id __maybe_unused rtl_tbl[] = {
	{ PHY_ID_MATCH_MODEL(PHY_ID_RTL8221D) },
	{ }
};

MODULE_DEVICE_TABLE(mdio, rtl_tbl);

MODULE_DESCRIPTION("rtl8221d PHY driver");
MODULE_AUTHOR("haungyunxiang huangyunxiang@cigtech.com>");
MODULE_LICENSE("GPL v2");
