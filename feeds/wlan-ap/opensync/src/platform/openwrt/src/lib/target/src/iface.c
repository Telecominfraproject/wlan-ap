/* SPDX-License-Identifier: BSD-3-Clause */

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <sys/ioctl.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int ioctl_socket;

int iface_is_up(const char *ifname)
{
	struct ifreq ifr = {};

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (ioctl(ioctl_socket, SIOCGIFFLAGS, &ifr))
		return 0;

	return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
}

static __attribute__((constructor)) void sm_init(void)
{
	ioctl_socket = socket(AF_INET, SOCK_DGRAM, 0);
	fcntl(ioctl_socket, F_SETFD, fcntl(ioctl_socket, F_GETFD) | FD_CLOEXEC);
}
