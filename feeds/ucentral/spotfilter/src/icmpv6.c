// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <netinet/icmp6.h>
#include "spotfilter.h"

struct icmpv6_opt {
	uint8_t type;
	uint8_t len;
	uint8_t data[6];
};

#define icmpv6_for_each_option(opt, start, end)					\
	for (opt = (const struct icmpv6_opt*)(start);				\
	     (const void *)(opt + 1) <= (const void *)(end) && opt->len > 0 &&	\
	     (const void *)(opt + opt->len) <= (const void *)(end); opt += opt->len)

void spotfilter_recv_icmpv6(const void *data, int len, const uint8_t *src, const uint8_t *dest)
{
	const struct nd_neighbor_advert *nd = data;
	const struct icmp6_hdr *hdr = data;
	const struct icmpv6_opt *opt;

	if (len < sizeof(*nd) || hdr->icmp6_code)
		return;

	if (hdr->icmp6_type != ND_NEIGHBOR_ADVERT)
		return;

	icmpv6_for_each_option(opt, &nd[1], data + len) {
		if (opt->type != ND_OPT_TARGET_LINKADDR || opt->len != 1)
			continue;

		if (memcmp(opt->data, src, ETH_ALEN))
			return;
	}

	if ((nd->nd_na_target.s6_addr[0] & 0xe0) != 0x20)
		return;

	if (opt != (const struct icmpv6_opt *)(data + len))
		return;

	client_set_ipaddr(src, &nd->nd_na_target, true);
}
