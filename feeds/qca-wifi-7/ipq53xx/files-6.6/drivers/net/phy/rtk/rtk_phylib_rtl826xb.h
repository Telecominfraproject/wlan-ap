/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#ifndef __RTK_PHYLIB_RTL826XB_H
#define __RTK_PHYLIB_RTL826XB_H

#include "rtk_phylib.h"

/* Register Access*/
int rtk_phylib_826xb_sds_read(rtk_phydev *phydev, uint32 page, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData);
int rtk_phylib_826xb_sds_write(rtk_phydev *phydev, uint32 page, uint32 reg, uint8 msb, uint8 lsb, uint32 data);

/* Interrupt */
int rtk_phylib_826xb_intr_enable(rtk_phydev *phydev, uint32 en);
int rtk_phylib_826xb_intr_read_clear(rtk_phydev *phydev, uint32 *status);
int rtk_phylib_826xb_intr_init(rtk_phydev *phydev);

/* Cable Test */
int rtk_phylib_826xb_cable_test_start(rtk_phydev *phydev);;
int rtk_phylib_826xb_cable_test_finished_get(rtk_phydev *phydev, uint32 *finished);
int rtk_phylib_826xb_cable_test_result_get(rtk_phydev *phydev, uint32 pair, rtk_rtct_channel_result_t *result);

/* MACsec */
int rtk_phylib_826xb_macsec_init(rtk_phydev *phydev);
int rtk_phylib_826xb_macsec_read(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData);
int rtk_phylib_826xb_macsec_write(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 data);
int rtk_phylib_826xb_macsec_bypass_set(rtk_phydev *phydev, uint32 bypass);
int rtk_phylib_826xb_macsec_bypass_get(rtk_phydev *phydev, uint32 *pBypass);

/* Link-down-power-saving/EDPD */
int rtk_phylib_826xb_link_down_power_saving_set(rtk_phydev *phydev, uint32 ena);
int rtk_phylib_826xb_link_down_power_saving_get(rtk_phydev *phydev, uint32 *pEna);

/* Wake on Lan */
int rtk_phylib_826xb_wol_reset(rtk_phydev *phydev);
int rtk_phylib_826xb_wol_set(rtk_phydev *phydev, uint32 wol_opts);
int rtk_phylib_826xb_wol_get(rtk_phydev *phydev, uint32 *pWol_opts);
int rtk_phylib_826xb_wol_unicast_addr_set(rtk_phydev *phydev, uint8 *mac_addr);
int rtk_phylib_826xb_wol_multicast_mask_add(rtk_phydev *phydev, uint32 offset);
int rtk_phylib_826xb_wol_multicast_mask_reset(rtk_phydev *phydev);
uint32 rtk_phylib_826xb_wol_multicast_mac2offset(uint8 *mac_addr);

#endif /* __RTK_PHYLIB_RTL826XB_H */
