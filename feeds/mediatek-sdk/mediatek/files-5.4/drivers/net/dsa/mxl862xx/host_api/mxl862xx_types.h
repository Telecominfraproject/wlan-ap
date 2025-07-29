// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_types.h - DSA Driver for MaxLinear Mxl862xx switch chips family
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

#ifndef _MXL862XX_TYPES_H_
#define _MXL862XX_TYPES_H_

#include <linux/phy.h>

/** \brief This is the unsigned 64-bit datatype. */
typedef uint64_t u64;
/** \brief This is the unsigned 32-bit datatype. */
typedef uint32_t u32;
/** \brief This is the unsigned 16-bit datatype. */
typedef uint16_t u16;
/** \brief This is the unsigned 8-bit datatype. */
typedef uint8_t u8;
/** \brief This is the signed 64-bit datatype. */
typedef int64_t i64;
/** \brief This is the signed 32-bit datatype. */
typedef int32_t i32;
/** \brief This is the signed 16-bit datatype. */
typedef int16_t i16;
/** \brief This is the signed 8-bit datatype. */
typedef int8_t i8;
/** \brief This is the signed 8-bit datatype. */
typedef int32_t s32;
/** \brief This is the signed 8-bit datatype. */
typedef int8_t s8;


/** \brief This is a union to describe the IPv4 and IPv6 Address in numeric representation. Used by multiple Structures and APIs. The member selection would be based upon \ref GSW_IP_Select_t */
typedef union {
	/** Describe the IPv4 address.
	    Only used if the IPv4 address should be read or configured.
	    Cannot be used together with the IPv6 address fields. */
	u32 nIPv4;
	/** Describe the IPv6 address.
	    Only used if the IPv6 address should be read or configured.
	    Cannot be used together with the IPv4 address fields. */
	u16 nIPv6[8];
} mxl862xx_ip_t;

/** \brief Selection to use IPv4 or IPv6.
    Used  along with \ref GSW_IP_t to denote which union member to be accessed.
*/
typedef enum {
	/** IPv4 Type */
	MXL862XX_IP_SELECT_IPV4 = 0,
	/** IPv6 Type */
	MXL862XX_IP_SELECT_IPV6 = 1
} mxl862xx_ip_select_t;

typedef struct {
	int sw_addr;
	struct mii_bus *bus;
} mxl862xx_device_t;
#endif /* _MXL862XX_TYPES_H_ */
