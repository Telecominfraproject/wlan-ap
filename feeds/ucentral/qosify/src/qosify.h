// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __QOS_CLASSIFY_H
#define __QOS_CLASSIFY_H

#include <stdbool.h>
#include <regex.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "qosify-bpf.h"

#include <libubox/utils.h>
#include <libubox/avl.h>
#include <libubox/blobmsg.h>
#include <libubox/ulog.h>

#include <netinet/in.h>

#define CLASSIFY_PROG_PATH	"/lib/bpf/qosify-bpf.o"
#define CLASSIFY_PIN_PATH	"/sys/fs/bpf/qosify"
#define CLASSIFY_DATA_PATH	"/sys/fs/bpf/qosify_data"

enum qosify_map_id {
	CL_MAP_TCP_PORTS,
	CL_MAP_UDP_PORTS,
	CL_MAP_IPV4_ADDR,
	CL_MAP_IPV6_ADDR,
	CL_MAP_CONFIG,
	CL_MAP_DNS,
	__CL_MAP_MAX,
};

struct qosify_map_data {
	enum qosify_map_id id;

	bool file : 1;
	bool user : 1;

	uint8_t dscp;
	uint8_t file_dscp;

	union {
		uint32_t port;
		struct in_addr ip;
		struct in6_addr ip6;
		struct {
			const char *pattern;
			regex_t regex;
		} dns;
	} addr;
};

struct qosify_map_entry {
	struct avl_node avl;

	uint32_t timeout;

	struct qosify_map_data data;
};


extern int qosify_map_timeout;
extern int qosify_active_timeout;
extern struct qosify_config config;

int qosify_loader_init(void);

int qosify_map_init(void);
int qosify_map_dscp_value(const char *val);
int qosify_map_load_file(const char *file);
int qosify_map_set_entry(enum qosify_map_id id, bool file, const char *str, uint8_t dscp);
void qosify_map_reload(void);
void qosify_map_clear_files(void);
void qosify_map_gc(void);
void qosify_map_dump(struct blob_buf *b);
void qosify_map_set_dscp_default(enum qosify_map_id id, uint8_t val);
void qosify_map_reset_config(void);
void qosify_map_update_config(void);
int qosify_map_add_dns_host(const char *host, const char *addr, const char *type, int ttl);

int qosify_iface_init(void);
void qosify_iface_config_update(struct blob_attr *ifaces, struct blob_attr *devs);
void qosify_iface_check(void);
void qosify_iface_status(struct blob_buf *b);
void qosify_iface_stop(void);

int qosify_ubus_init(void);
void qosify_ubus_stop(void);
int qosify_ubus_check_interface(const char *name, char *ifname, int ifname_len);

#endif
