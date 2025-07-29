// spdx-license-identifier: gpl-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_host_api_impl.h - dsa driver for maxlinear mxl862xx switch chips family
 *
 * copyright (c) 2024 maxlinear inc.
 *
 * this program is free software; you can redistribute it and/or
 * modify it under the terms of the gnu general public license
 * as published by the free software foundation; either version 2
 * of the license, or (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 51 franklin street, fifth floor, boston, ma  02110-1301, usa.
 *
 */

#ifndef _MXL862XX_HOST_API_IMPL_H_
#define _MXL862XX_HOST_API_IMPL_H_

#include "mxl862xx_types.h"

extern int mxl862xx_api_wrap(const mxl862xx_device_t *dev, uint16_t cmd, void *pdata,
			uint16_t size, uint16_t cmd_r, uint16_t r_size);

#endif /* _MXL862XX_HOST_API_IMPL_H_ */
