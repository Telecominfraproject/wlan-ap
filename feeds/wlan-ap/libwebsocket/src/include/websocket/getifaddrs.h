/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef LWS_HAVE_GETIFADDRS
#define LWS_HAVE_GETIFADDRS 0
#endif

#if LWS_HAVE_GETIFADDRS
#include <sys/types.h>
#include <ifaddrs.h>
#else
#ifdef __cplusplus
extern "C" {
#endif

#ifndef ifaddrs_h_7467027A95AD4B5C8DDD40FE7D973791
#define ifaddrs_h_7467027A95AD4B5C8DDD40FE7D973791

/*
 * the interface is defined in terms of the fields below, and this is
 * sometimes #define'd, so there seems to be no simple way of solving
 * this and this seemed the best. */

#undef ifa_dstaddr

struct ifaddrs {
	struct ifaddrs *ifa_next;
	char *ifa_name;
	unsigned int ifa_flags;
	struct sockaddr *ifa_addr;
	struct sockaddr *ifa_netmask;
	struct sockaddr *ifa_dstaddr;
	void *ifa_data;
};

#ifndef ifa_broadaddr
#define ifa_broadaddr ifa_dstaddr
#endif

int getifaddrs(struct ifaddrs **);

void freeifaddrs(struct ifaddrs *);

#endif /* __ifaddrs_h__ */

#ifdef __cplusplus
}
#endif
#endif
