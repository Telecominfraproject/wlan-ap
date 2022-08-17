// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/resource.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <glob.h>
#include <unistd.h>

#include "spotfilter.h"

static int spotfilter_bpf_pr(enum libbpf_print_level level, const char *format,
		     va_list args)
{
	return vfprintf(stderr, format, args);
}

static void
spotfilter_fill_rodata(struct bpf_object *obj, struct spotfilter_bpf_config *val)
{
	struct bpf_map *map = NULL;

	while ((map = bpf_object__next_map(obj, map)) != NULL) {
		if (!strstr(bpf_map__name(map), ".rodata"))
			continue;

		bpf_map__set_initial_value(map, val, sizeof(*val));
	}
}

static void spotfilter_init_env(void)
{
	struct rlimit limit = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY,
	};

	setrlimit(RLIMIT_MEMLOCK, &limit);
}

int spotfilter_bpf_load(struct interface *iface)
{
	DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
	struct spotfilter_bpf_config config = {
		.snoop_ifindex = spotfilter_ifb_ifindex
	};
	struct bpf_program *prog_i, *prog_e;
	struct bpf_object *obj;
	int err;

	libbpf_set_print(spotfilter_bpf_pr);

	spotfilter_init_env();

	obj = bpf_object__open_file(SPOTFILTER_PROG_PATH, &opts);
	err = libbpf_get_error(obj);
	if (err) {
		perror("bpf_object__open_file");
		return -1;
	}

	prog_i = bpf_object__find_program_by_name(obj, "spotfilter_in");
	if (!prog_i) {
		fprintf(stderr, "Can't find ingress classifier\n");
		goto error;
	}

	prog_e = bpf_object__find_program_by_name(obj, "spotfilter_out");
	if (!prog_e) {
		fprintf(stderr, "Can't find egress classifier\n");
		goto error;
	}

	bpf_program__set_type(prog_i, BPF_PROG_TYPE_SCHED_CLS);
	bpf_program__set_type(prog_e, BPF_PROG_TYPE_SCHED_CLS);

	spotfilter_fill_rodata(obj, &config);

	err = bpf_object__load(obj);
	if (err) {
		perror("bpf_object__load");
		goto error;
	}

	iface->bpf.prog_ingress = bpf_program__fd(prog_i);
	iface->bpf.prog_egress = bpf_program__fd(prog_e);
	if ((iface->bpf.map_class = bpf_object__find_map_fd_by_name(obj, "class")) < 0 ||
	    (iface->bpf.map_client = bpf_object__find_map_fd_by_name(obj, "client")) < 0 ||
	    (iface->bpf.map_whitelist_v4 = bpf_object__find_map_fd_by_name(obj, "whitelist_ipv4")) < 0 ||
	    (iface->bpf.map_whitelist_v6 = bpf_object__find_map_fd_by_name(obj, "whitelist_ipv6")) < 0) {
		perror("bpf_object__find_map_fd_by_name");
		goto error;
	}
	iface->bpf.obj = obj;

	return 0;

error:
	bpf_object__close(obj);
	return -1;
}

int spotfilter_bpf_get_client(struct interface *iface,
			      const struct spotfilter_client_key *key,
			      struct spotfilter_client_data *data)
{
	return bpf_map_lookup_elem(iface->bpf.map_client, key, data);
}

int spotfilter_bpf_set_client(struct interface *iface,
			      const struct spotfilter_client_key *key,
			      const struct spotfilter_client_data *data)
{
	if (!data)
		return bpf_map_delete_elem(iface->bpf.map_client, key);

	return bpf_map_update_elem(iface->bpf.map_client, key, data, BPF_ANY);
}

static void
__spotfilter_bpf_set_device(struct interface *iface, int ifindex, bool egress, bool enabled)
{
	DECLARE_LIBBPF_OPTS(bpf_tc_hook, hook,
			    .attach_point = egress ? BPF_TC_EGRESS : BPF_TC_INGRESS,
			    .ifindex = ifindex);
	DECLARE_LIBBPF_OPTS(bpf_tc_opts, attach_tc,
			    .handle = 1,
			    .priority = SPOTFILTER_PRIO_BASE);

	if (!enabled) {
		bpf_tc_detach(&hook, &attach_tc);
		return;
	}

	if (egress)
		attach_tc.prog_fd = iface->bpf.prog_egress;
	else
		attach_tc.prog_fd = iface->bpf.prog_ingress;

	bpf_tc_hook_create(&hook);
	bpf_tc_attach(&hook, &attach_tc);
}

void spotfilter_bpf_set_device(struct interface *iface, int ifindex, bool enabled)
{
	if (enabled)
		spotfilter_bpf_set_device(iface, ifindex, false);

	__spotfilter_bpf_set_device(iface, ifindex, true, enabled);
	__spotfilter_bpf_set_device(iface, ifindex, false, enabled);
}

void spotfilter_bpf_update_class(struct interface *iface, uint32_t index)
{
	bpf_map_update_elem(iface->bpf.map_class, &index, &iface->cdata[index], BPF_ANY);
}

bool spotfilter_bpf_whitelist_seen(struct interface *iface, const void *addr, bool ipv6)
{
	int fd = ipv6 ? iface->bpf.map_whitelist_v6 : iface->bpf.map_whitelist_v4;
	struct spotfilter_whitelist_entry e;

	bpf_map_lookup_elem(fd, addr, &e);
	if (!e.seen)
	    return false;

	e.seen = 0;
	bpf_map_update_elem(fd, addr, &e, BPF_ANY);

	return true;
}

void spotfilter_bpf_set_whitelist(struct interface *iface, const void *addr,
				  bool ipv6, const uint8_t *state)
{
	int fd = ipv6 ? iface->bpf.map_whitelist_v6 : iface->bpf.map_whitelist_v4;
	struct spotfilter_whitelist_entry e = {};

	if (!state) {
		bpf_map_delete_elem(fd, addr);
		return;
	}

	e.val = *state;
	bpf_map_update_elem(fd, addr, &e, BPF_ANY);
}

void spotfilter_bpf_free(struct interface *iface)
{
	if (!iface->bpf.obj)
		return;

	bpf_object__close(iface->bpf.obj);
	iface->bpf.obj = NULL;
}
