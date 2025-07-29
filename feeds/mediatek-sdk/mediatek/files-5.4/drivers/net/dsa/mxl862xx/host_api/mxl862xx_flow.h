/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/net/dsa/host_api/mxl862xx_flow.h - Header file for DSA Driver for MaxLinear Mxl862xx switch chips family
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

#ifndef _MXL862XX_FLOW_H_
#define _MXL862XX_FLOW_H_

#include "mxl862xx_types.h"

#pragma pack(push, 1)
#pragma scalar_storage_order little-endian

#define MPCE_RULES_INDEX 0
#define MPCE_RULES_INDEX_LAST (MPCE_RULES_INDEX + 7)
#define EAPOL_PCE_RULE_INDEX 8
#define BPDU_PCE_RULE_INDEX 9
#define PFC_PCE_RULE_INDEX 10

/** \brief Register access parameter to directly modify internal registers.
    Used by \ref GSW_REGISTER_MOD. */
typedef struct {
	/** Register Address Offset for modifiation. */
	u16 reg_addr;
	/** Value to write to 'reg_addr'. */
	u16 data;
	/** Mask of bits to be modified. 1 to modify, 0 to ignore. */
	u16 mask;
} mxl862xx_register_mod_t;

#pragma scalar_storage_order default
#pragma pack(pop)

#endif /* _MXL862XX_FLOW_H_ */
