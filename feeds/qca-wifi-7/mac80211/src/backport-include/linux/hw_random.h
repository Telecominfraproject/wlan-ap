#ifndef __BACKPORT_HW_RANDOM_H
#define __BACKPORT_HW_RANDOM_H

#include_next <linux/hw_random.h>
#include <linux/version.h>
#include <linux/delay.h>

#if LINUX_VERSION_IS_LESS(6,1,0)

#define hwrng_msleep LINUX_BACKPORT(hwrng_msleep)
static inline long hwrng_msleep(struct hwrng *rng, unsigned int msecs)
{
	return msleep_interruptible(msecs);
}

#endif /* < 6.1.0 */

#endif
