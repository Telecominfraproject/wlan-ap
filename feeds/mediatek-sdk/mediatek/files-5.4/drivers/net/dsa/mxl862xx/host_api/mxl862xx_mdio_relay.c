// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_mdio_relay.c - dsa driver for Maxlinear mxl862xx switch chips family
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

#include "mxl862xx_mdio_relay.h"
#include "mxl862xx_host_api_impl.h"
#include "mxl862xx_mmd_apis.h"

int int_gphy_read(const mxl862xx_device_t *dev, struct mdio_relay_data *parm)
{
	return mxl862xx_api_wrap(dev, INT_GPHY_READ, parm, sizeof(*parm), 0,
			    sizeof(parm->data));
}

int int_gphy_write(const mxl862xx_device_t *dev, struct mdio_relay_data *parm)
{
	return mxl862xx_api_wrap(dev, INT_GPHY_WRITE, parm, sizeof(*parm), 0, 0);
}

int int_gphy_mod(const mxl862xx_device_t *dev, struct mdio_relay_mod_data *parm)
{
	return mxl862xx_api_wrap(dev, INT_GPHY_MOD, parm, sizeof(*parm), 0, 0);
}

int ext_mdio_read(const mxl862xx_device_t *dev, struct mdio_relay_data *parm)
{
	return mxl862xx_api_wrap(dev, EXT_MDIO_READ, parm, sizeof(*parm), 0,
			    sizeof(parm->data));
}

int ext_mdio_write(const mxl862xx_device_t *dev, struct mdio_relay_data *parm)
{
	return mxl862xx_api_wrap(dev, EXT_MDIO_WRITE, parm, sizeof(*parm), 0, 0);
}

int ext_mdio_mod(const mxl862xx_device_t *dev, struct mdio_relay_mod_data *parm)
{
	return mxl862xx_api_wrap(dev, EXT_MDIO_MOD, parm, sizeof(*parm), 0, 0);
}

EXPORT_SYMBOL(int_gphy_write);
EXPORT_SYMBOL(int_gphy_read);
EXPORT_SYMBOL(int_gphy_mod);
