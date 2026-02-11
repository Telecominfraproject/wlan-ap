/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BACKPORT_BCMA_DRIVER_CC_H_
#define BACKPORT_BCMA_DRIVER_CC_H_

#include_next <linux/bcma/bcma_driver_chipcommon.h>

#ifndef BCMA_CC_SROM_CONTROL_OTP_PRESENT
#define  BCMA_CC_SROM_CONTROL_OTP_PRESENT	0x00000020
#endif /* BCMA_CC_SROM_CONTROL_OTP_PRESENT */

#endif /* BACKPORT_BCMA_DRIVER_CC_H_ */
