// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/resource.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <glob.h>
#include <unistd.h>

#include "qosify.h"

static int qosify_bpf_pr(enum libbpf_print_level level, const char *format,
		     va_list args)
{
	return vfprintf(stderr, format, args);
}

static void qosify_init_env(void)
{
	struct rlimit limit = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY,
	};

	setrlimit(RLIMIT_MEMLOCK, &limit);
}

static void qosify_fill_rodata(struct bpf_object *obj, uint32_t flags)
{
	struct bpf_map *map = NULL;

	while ((map = bpf_map__next(map, obj)) != NULL) {
		if (!strstr(bpf_map__name(map), ".rodata"))
			continue;

		bpf_map__set_initial_value(map, &flags, sizeof(flags));
	}
}

static int
qosify_create_program(const char *suffix, uint32_t flags)
{
	DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts,
		.pin_root_path = CLASSIFY_DATA_PATH,
	);
	struct bpf_program *prog;
	struct bpf_object *obj;
	char path[256];
	int err;

	snprintf(path, sizeof(path), CLASSIFY_PIN_PATH "_" "%s", suffix);

	obj = bpf_object__open_file(CLASSIFY_PROG_PATH, &opts);
	err = libbpf_get_error(obj);
	if (err) {
		perror("bpf_object__open_file");
		return -1;
	}

	prog = bpf_object__find_program_by_title(obj, "classifier");
	if (!prog) {
		fprintf(stderr, "Can't find classifier prog\n");
		return -1;
	}

	bpf_program__set_type(prog, BPF_PROG_TYPE_SCHED_CLS);

	qosify_fill_rodata(obj, flags);

	err = bpf_object__load(obj);
	if (err) {
		perror("bpf_object__load");
		return -1;
	}

	libbpf_set_print(NULL);

	unlink(path);
	err = bpf_program__pin(prog, path);
	if (err) {
		fprintf(stderr, "Failed to pin program to %s: %s\n",
			path, strerror(-err));
	}

	bpf_object__close(obj);

	return 0;
}

int qosify_loader_init(void)
{
	static const struct {
		const char *suffix;
		uint32_t flags;
	} progs[] = {
		{ "egress_eth", 0 },
		{ "egress_ip", QOSIFY_IP_ONLY },
		{ "ingress_eth", QOSIFY_INGRESS },
		{ "ingress_ip", QOSIFY_INGRESS | QOSIFY_IP_ONLY },
	};
	glob_t g;
	int i;

	if (glob(CLASSIFY_DATA_PATH "/*", 0, NULL, &g) == 0) {
		for (i = 0; i < g.gl_pathc; i++)
			unlink(g.gl_pathv[i]);
	}


	libbpf_set_print(qosify_bpf_pr);

	qosify_init_env();

	for (i = 0; i < ARRAY_SIZE(progs); i++) {
		if (qosify_create_program(progs[i].suffix, progs[i].flags))
			return -1;
	}

	return 0;
}
