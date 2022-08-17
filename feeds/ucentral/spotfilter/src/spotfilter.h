// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __SPOTFILTER_H
#define __SPOTFILTER_H

#include <stdbool.h>
#include <stdint.h>
#include <regex.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include <libubox/utils.h>
#include <libubox/avl.h>
#include <libubox/vlist.h>
#include <libubox/blobmsg.h>
#include <libubox/ulog.h>

#include <netinet/in.h>

#include "spotfilter-bpf.h"
#include "interface.h"
#include "bpf.h"
#include "client.h"

#define SPOTFILTER_IFB_NAME	"spotfilter-ifb"

#define SPOTFILTER_PROG_PATH	"/lib/bpf/spotfilter-bpf.o"

#define SPOTFILTER_PRIO_BASE	0x120

extern int spotfilter_ifb_ifindex;
struct nl_msg;

int rtnl_init(void);
int rtnl_fd(void);
int rtnl_call(struct nl_msg *msg);

int spotfilter_run_cmd(char *cmd, bool ignore_error);

int spotfilter_ubus_init(void);
void spotfilter_ubus_stop(void);
void spotfilter_ubus_notify(struct interface *iface, struct client *cl, const char *type);

int spotfilter_dev_init(void);
void spotfilter_dev_done(void);

void spotfilter_dns_init(struct interface *iface);
void spotfilter_dns_free(struct interface *iface);

void spotfilter_recv_dhcpv4(const void *msg, int len, const void *eth_addr);
void spotfilter_recv_icmpv6(const void *data, int len, const uint8_t *src, const uint8_t *dest);

int spotfilter_nl80211_init(void);
void spotfilter_nl80211_done(void);

#endif
