/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _LIBBRIDGE_PRIVATE_H
#define _LIBBRIDGE_PRIVATE_H

#include <linux/sockios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/if_bridge.h>

#define MAX_BRIDGES	1024
#define MAX_PORTS	1024

#define SYSFS_CLASS_NET "/sys/class/net/"
#define SYSFS_PATH_MAX	256

#define dprintf(fmt,arg...)

extern int br_socket_fd;

static inline unsigned long __tv_to_jiffies(const struct timeval *tv)
{
	unsigned long long jif;

	jif = 1000000ULL * tv->tv_sec + tv->tv_usec;

	return jif/10000;
}

static inline void __jiffies_to_tv(struct timeval *tv, unsigned long jiffies)
{
	unsigned long long tvusec;

	tvusec = 10000ULL*jiffies;
	tv->tv_sec = tvusec/1000000;
	tv->tv_usec = tvusec - 1000000 * tv->tv_sec;
}
#endif
