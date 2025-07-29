/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/net/dsa/host_api/mxl862xx_mdio_relay.h - Header file for DSA Driver for MaxLinear Mxl862xx switch chips family
 *
 * Copyright (C) 2024 MaxLinear Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef _MXL862XX_MDIO_RELAY_H_
#define _MXL862XX_MDIO_RELAY_H_

#include <linux/types.h>
#include "mxl862xx_types.h"

#pragma pack(push, 1)
#pragma scalar_storage_order little-endian

struct mdio_relay_data {
	/* data to be read or written */
	uint16_t data;
	/* PHY index (0~7) for internal PHY
	 * PHY address (0~31) for external PHY access via MDIO bus
	 */
	uint8_t phy;
	/* MMD device (0~31) */
	uint8_t mmd;
	/* Register Index
	 * 0~31 if mmd is 0 (CL22)
	 * 0~65535 otherwise (CL45)
	 */
	uint16_t reg;
};

struct mdio_relay_mod_data {
	/* data to be written with mask */
	uint16_t data;
	/* PHY index (0~7) for internal PHY
	 * PHY address (0~31) for external PHY access via MDIO bus
	 */
	uint8_t phy;
	/* MMD device (0~31) */
	uint8_t mmd;
	/* Register Index
	 * 0~31 if mmd is 0 (CL22)
	 * 0~65535 otherwise (CL45)
	 */
	uint16_t reg;
	/* mask of bit fields to be updated
	 * 1 to write the bit
	 * 0 to ignore
	 */
	uint16_t mask;
};

#pragma scalar_storage_order default
#pragma pack(pop)

/* read internal GPHY MDIO/MMD registers */
int int_gphy_read(const mxl862xx_device_t *dev, struct mdio_relay_data *pdata);
/* write internal GPHY MDIO/MMD registers */
int int_gphy_write(const mxl862xx_device_t *dev, struct mdio_relay_data *pdata);
/* modify internal GPHY MDIO/MMD registers */
int int_gphy_mod(const mxl862xx_device_t *dev, struct mdio_relay_mod_data *pdata);

/* read external GPHY MDIO/MMD registers via MDIO bus */
int ext_mdio_read(const mxl862xx_device_t *dev, struct mdio_relay_data *pdata);
/* write external GPHY MDIO/MMD registers via MDIO bus */
int ext_mdio_write(const mxl862xx_device_t *dev, struct mdio_relay_data *pdata);
/* modify external GPHY MDIO/MMD registers via MDIO bus */
int ext_mdio_mod(const mxl862xx_device_t *dev, struct mdio_relay_mod_data *pdata);

#endif /*  _MXL862XX_MDIO_RELAY_H_ */
