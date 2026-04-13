#ifndef __BACKPORT_LINUX_NETLINK_H
#define __BACKPORT_LINUX_NETLINK_H
#include_next <linux/netlink.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,0,0)
static inline void nl_set_extack_cookie_u64(struct netlink_ext_ack *extack,
					    u64 cookie)
{
	u64 __cookie = cookie;

	memcpy(extack->cookie, &__cookie, sizeof(__cookie));
	extack->cookie_len = sizeof(__cookie);
}
#endif

#endif /* __BACKPORT_LINUX_NETLINK_H */
