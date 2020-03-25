/*
* Copyright (c) 2019, Sagemcom.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "log.h"
#include "osn_fw_pri.h"
#include "os.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MODULE_ID LOG_MODULE_ID_TARGET

static struct osfw_nfbase osfw_nfbase;

static const char *osfw_convert_family(int family)
{
	const char *str = OSFW_STR_UNKNOWN;

	switch (family) {
	case AF_INET:
		str = OSFW_STR_FAMILY_INET;
		break;

	case AF_INET6:
		str = OSFW_STR_FAMILY_INET6;
		break;

	default:
		LOGE("Invalid family: %d", family);
		break;
	}
	return str;
}

static const char *osfw_convert_cmd(int family)
{
	const char *cmd = OSFW_STR_UNKNOWN;

	switch (family) {
	case AF_INET:
		cmd = OSFW_STR_CMD_IPTABLES_RESTORE;
		break;

	case AF_INET6:
		cmd = OSFW_STR_CMD_IP6TABLES_RESTORE;
		break;

	default:
		LOGE("Invalid family: %d", family);
		break;
	}

	return cmd;
}

static const char *osfw_convert_table(enum osfw_table table)
{
	const char *str = OSFW_STR_UNKNOWN;

	switch (table) {
	case OSFW_TABLE_FILTER:
		str = OSFW_STR_TABLE_FILTER;
		break;

	case OSFW_TABLE_NAT:
		str = OSFW_STR_TABLE_NAT;
		break;

	case OSFW_TABLE_MANGLE:
		str = OSFW_STR_TABLE_MANGLE;
		break;

	case OSFW_TABLE_RAW:
		str = OSFW_STR_TABLE_RAW;
		break;

	case OSFW_TABLE_SECURITY:
		str = OSFW_STR_TABLE_SECURITY;
		break;

	default:
		LOGE("Convert firewall table: Invalid table: %d", table);
		break;
	}

	return str;
}

static bool osfw_is_builtin_chain(enum osfw_table table, const char *chain)
{
	if (!strncmp(chain, OSFW_STR_TARGET_ACCEPT, sizeof(OSFW_STR_TARGET_ACCEPT))) {
		return true;
	} else if (!strncmp(chain, OSFW_STR_TARGET_DROP, sizeof(OSFW_STR_TARGET_DROP))) {
		return true;
	} else if (!strncmp(chain, OSFW_STR_TARGET_REJECT, sizeof(OSFW_STR_TARGET_REJECT))) {
		return true;
	} else if (!strncmp(chain, OSFW_STR_TARGET_QUEUE, sizeof(OSFW_STR_TARGET_QUEUE))) {
		return true;
	} else if (!strncmp(chain, OSFW_STR_TARGET_RETURN, sizeof(OSFW_STR_TARGET_RETURN))) {
		return true;
	}

	switch (table) {
	case OSFW_TABLE_FILTER:
		if (!strncmp(chain, OSFW_STR_CHAIN_INPUT, sizeof(OSFW_STR_CHAIN_INPUT))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_FORWARD, sizeof(OSFW_STR_CHAIN_FORWARD))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_OUTPUT, sizeof(OSFW_STR_CHAIN_OUTPUT))) {
			return true;
		}
		break;

	case OSFW_TABLE_NAT:
		if (!strncmp(chain, OSFW_STR_CHAIN_PREROUTING, sizeof(OSFW_STR_CHAIN_PREROUTING))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_OUTPUT, sizeof(OSFW_STR_CHAIN_OUTPUT))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_POSTROUTING, sizeof(OSFW_STR_CHAIN_POSTROUTING))) {
			return true;
		}
		break;

	case OSFW_TABLE_MANGLE:
		if (!strncmp(chain, OSFW_STR_CHAIN_PREROUTING, sizeof(OSFW_STR_CHAIN_PREROUTING))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_INPUT, sizeof(OSFW_STR_CHAIN_INPUT))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_FORWARD, sizeof(OSFW_STR_CHAIN_FORWARD))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_OUTPUT, sizeof(OSFW_STR_CHAIN_OUTPUT))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_POSTROUTING, sizeof(OSFW_STR_CHAIN_POSTROUTING))) {
			return true;
		}
		break;

	case OSFW_TABLE_RAW:
		if (!strncmp(chain, OSFW_STR_CHAIN_PREROUTING, sizeof(OSFW_STR_CHAIN_PREROUTING))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_OUTPUT, sizeof(OSFW_STR_CHAIN_OUTPUT))) {
			return true;
		}
		break;

	case OSFW_TABLE_SECURITY:
		if (!strncmp(chain, OSFW_STR_CHAIN_INPUT, sizeof(OSFW_STR_CHAIN_INPUT))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_FORWARD, sizeof(OSFW_STR_CHAIN_FORWARD))) {
			return true;
		} else if (!strncmp(chain, OSFW_STR_CHAIN_OUTPUT, sizeof(OSFW_STR_CHAIN_OUTPUT))) {
			return true;
		}
		break;

	default:
		LOGE("Is built-in firewall chain: Invalid table: %d", table);
		break;
	}

	return false;
}

static bool osfw_is_valid_chain(const char *chain)
{
	if (strchr(chain, ' ')) {
		return false;
	}
	return true;
}

static bool osfw_nfchain_set(struct osfw_nfchain *self, struct ds_dlist *parent, const char *chain)
{
	memset(self, 0, sizeof(*self));
	self->parent = parent;
	ds_dlist_insert_tail(self->parent, self);
	strncpy(self->chain, chain, sizeof(self->chain) - 1);
	self->chain[sizeof(self->chain) - 1] = '\0';
	return true;
}

static bool osfw_nfchain_unset(struct osfw_nfchain *self)
{
	ds_dlist_remove(self->parent, self);
	memset(self, 0, sizeof(*self));
	return true;
}

static bool osfw_nfchain_del(struct osfw_nfchain *self)
{
	bool errcode = true;

	errcode = osfw_nfchain_unset(self);
	if (!errcode) {
		return false;
	}

	free(self);
	return false;
}

static struct osfw_nfchain *osfw_nfchain_add(struct ds_dlist *parent, const char *chain)
{
	struct osfw_nfchain *self = NULL;
	bool errcode = true;

	if (!parent || !chain) {
		LOGE("Add OSFW chain: invalid parameter");
		return NULL;
	}

	self = malloc(sizeof(*self));
	if (!self) {
		LOGE("Add OSFW chain: memory allocation failed");
		return NULL;
	}

	errcode = osfw_nfchain_set(self, parent, chain);
	if (!errcode) {
		osfw_nfchain_del(self);
		return NULL;
	}
	return self;
}

static bool osfw_nfchain_match(const struct osfw_nfchain *self, const char *chain)
{
	if (!self || !chain) {
		return false;
	} else if (strncmp(self->chain, chain, sizeof(self->chain))) {
		return false;
	}
	return true;
}

static void osfw_nfchain_print(const struct osfw_nfchain *self, FILE *stream)
{
	if (!self || !stream) {
		return;
	}
	fprintf(stream, "-N %s\n", self->chain);
}

static bool osfw_nfrule_set(struct osfw_nfrule *self, struct ds_dlist *parent, const char *chain,
		int prio, const char *match, const char *target)
{
	struct osfw_nfrule *nfrule = NULL;

	memset(self, 0, sizeof(*self));
	self->prio = prio;
	self->parent = parent;

	ds_dlist_foreach(self->parent, nfrule) {
		if (self->prio < nfrule->prio) {
			break;
		}
	}
	if (nfrule) {
		ds_dlist_insert_before(self->parent, nfrule, self);
	} else {
		ds_dlist_insert_tail(self->parent, self);
	}

	strncpy(self->chain, chain, sizeof(self->chain) - 1);
	self->chain[sizeof(self->chain) - 1] = '\0';
	strncpy(self->match, match, sizeof(self->match) - 1);
	self->match[sizeof(self->match) - 1] = '\0';
	strncpy(self->target, target, sizeof(self->target) - 1);
	self->target[sizeof(self->target) - 1] = '\0';
	return true;
}

static bool osfw_nfrule_unset(struct osfw_nfrule *self)
{
	ds_dlist_remove(self->parent, self);
	memset(self, 0, sizeof(*self));
	return true;
}

static bool osfw_nfrule_del(struct osfw_nfrule *self)
{
	bool errcode = true;

	errcode = osfw_nfrule_unset(self);
	if (!errcode) {
		return false;
	}

	free(self);
	return true;
}

static struct osfw_nfrule *osfw_nfrule_add(struct ds_dlist *parent, const char *chain,
		int prio, const char *match, const char *target)
{
	struct osfw_nfrule *self = NULL;
	bool errcode = true;

	if (!parent || !chain || !match || !target) {
		LOGE("Add OSFW rule: invalid parameter");
		return NULL;
	}

	self = malloc(sizeof(*self));
	if (!self) {
		LOGE("Add OSFW rule: memory allocation failed");
		return NULL;
	}

	errcode = osfw_nfrule_set(self, parent, chain, prio, match, target);
	if (!errcode) {
		osfw_nfrule_del(self);
		return NULL;
	}
	return self;
}

static bool osfw_nfrule_match(const struct osfw_nfrule *self, const char *chain, int prio,
		const char *match, const char *target)
{
	if (!self || !chain || !match || !target) {
		return false;
	} else if (self->prio != prio) {
		return false;
	} else if (strncmp(self->chain, chain, sizeof(self->chain))) {
		return false;
	} else if (strncmp(self->match, match, sizeof(self->match))) {
		return false;
	} else if (strncmp(self->target, target, sizeof(self->target))) {
		return false;
	}
	return true;
}


static void osfw_nfrule_print(const struct osfw_nfrule *self, FILE *stream)
{
	if (!self || !stream) {
		return;
	}
	fprintf(stream, "-A %s %s -j %s\n", self->chain, self->match, self->target);
}

static void osfw_nftable_print_header(const struct osfw_nftable *self, FILE *stream)
{
	if (!self || !stream) {
		return;
	}
	fprintf(stream, "*%s\n", osfw_convert_table(self->table));

	switch (self->table) {
	case OSFW_TABLE_FILTER:
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_INPUT, OSFW_STR_TARGET_DROP);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_FORWARD, OSFW_STR_TARGET_DROP);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_OUTPUT, OSFW_STR_TARGET_DROP);
		break;

	case OSFW_TABLE_NAT:
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_PREROUTING, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_OUTPUT, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_POSTROUTING, OSFW_STR_TARGET_ACCEPT);
		break;

	case OSFW_TABLE_MANGLE:
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_PREROUTING, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_INPUT, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_FORWARD, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_OUTPUT, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_POSTROUTING, OSFW_STR_TARGET_ACCEPT);
		break;

	case OSFW_TABLE_RAW:
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_PREROUTING, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_OUTPUT, OSFW_STR_TARGET_ACCEPT);
		break;

	case OSFW_TABLE_SECURITY:
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_INPUT, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_FORWARD, OSFW_STR_TARGET_ACCEPT);
		fprintf(stream, ":%s %s [0:0]\n", OSFW_STR_CHAIN_OUTPUT, OSFW_STR_TARGET_ACCEPT);
		break;

	default:
		LOGE("Invalid table: %d", self->table);
		break;
	}
}

static void osfw_nftable_print_footer(const struct osfw_nftable *self, FILE *stream)
{
	if (!self || !stream) {
		return;
	}
	fprintf(stream, "COMMIT\n");
}

static void osfw_nftable_print(struct osfw_nftable *self, bool nfrules, struct osfw_nfrule *nfrule, FILE *stream)
{
	struct osfw_nfchain *nfchain = NULL;

	if (!self) {
		return;
	} else if (self->isinitialized && !self->issupported) {
		return;
	}

	osfw_nftable_print_header(self, stream);
	ds_dlist_foreach(&self->chains, nfchain) {
		osfw_nfchain_print(nfchain, stream);
	}

	if (nfrules) {
		nfrule = NULL;
		ds_dlist_foreach(&self->rules, nfrule) {
			osfw_nfrule_print(nfrule, stream);
		}
	} else if (nfrule) {
		osfw_nfrule_print(nfrule, stream);
	}
	osfw_nftable_print_footer(self, stream);
}

static bool osfw_nftable_check(struct osfw_nftable *self, struct osfw_nfrule *nfrule)
{
	bool errcode = true;
	int err = 0;
	char cmd[OSFW_SIZE_CMD];
	char path[OSFW_SIZE_CMD];
	FILE *stream = NULL;

	snprintf(path, sizeof(path) - 1, "/tmp/osfw-%s-%s.%d", osfw_convert_family(self->family),
			osfw_convert_table(self->table), (int) getpid());
	path[sizeof(path) - 1] = '\0';
	stream = fopen(path, "w+");
	if (!stream) {
		LOGE("Open %s failed: %d - %s", path, errno, strerror(errno));
		return false;
	}
	osfw_nftable_print(self, false, nfrule, stream);
	fclose(stream);

	snprintf(cmd, sizeof(cmd) - 1, "cat %s | %s -t", path, osfw_convert_cmd(self->family));
	cmd[sizeof(cmd) - 1] = '\0';
	err = cmd_log(cmd);
	if (err) {
		if (self->isinitialized) {
			LOGE("Check OSFW %s %s configuration failed", osfw_convert_family(self->family),
					osfw_convert_table(self->table));
		}
		errcode = false;
	}

	unlink(path);
	return errcode;
}

static bool osfw_nftable_set(struct osfw_nftable *self, int family, enum osfw_table table)
{
	memset(self, 0, sizeof(*self));
	self->isinitialized = false;
	self->family = family;
	self->table = table;
	ds_dlist_init(&self->chains, struct osfw_nfchain, elt);
	ds_dlist_init(&self->rules, struct osfw_nfrule, elt);
	self->issupported = osfw_nftable_check(self, NULL);
	self->isinitialized = true;
	return true;
}

static bool osfw_nftable_unset(struct osfw_nftable *self)
{
	bool errcode = true;
	struct osfw_nfchain *nfchain = NULL;
	struct osfw_nfchain *nfchain_tmp = NULL;
	struct osfw_nfrule *nfrule = NULL;
	struct osfw_nfrule *nfrule_tmp = NULL;

	nfrule = ds_dlist_head(&self->rules);
	while (nfrule) {
		nfrule_tmp = ds_dlist_next(&self->rules, nfrule);
		errcode = osfw_nfrule_del(nfrule);
		if (!errcode) {
			return false;
		}
		nfrule = nfrule_tmp;
	}

	nfchain = ds_dlist_head(&self->chains);
	while (nfchain) {
		nfchain_tmp = ds_dlist_next(&self->chains, nfchain);
		errcode = osfw_nfchain_del(nfchain);
		if (!errcode) {
			return false;
		}
		nfchain = nfchain_tmp;
	}
	return true;
}

static struct osfw_nfchain *osfw_nftable_get_nfchain(struct osfw_nftable *self, const char *chain)
{
	struct osfw_nfchain *nfchain = NULL;

	ds_dlist_foreach(&self->chains, nfchain) {
		if (osfw_nfchain_match(nfchain, chain)) {
			return nfchain;
		}
	}
	return NULL;
}

static struct osfw_nfrule *osfw_nftable_get_nfrule(struct osfw_nftable *self, const char *chain,
		int prio, const char *match, const char *target)
{
	struct osfw_nfrule *nfrule = NULL;

	ds_dlist_foreach(&self->rules, nfrule) {
		if (osfw_nfrule_match(nfrule, chain, prio, match, target)) {
			return nfrule;
		}
	}
	return NULL;
}

static bool osfw_nftable_add_nfchain(struct osfw_nftable *self, const char *chain)
{
	bool errcode = true;
	struct osfw_nfchain *nfchain = NULL;

	if (!self->issupported) {
		LOGE("OSFW table add chain: table %s %s is not supported",
				osfw_convert_family(self->family), osfw_convert_table(self->table));
		return false;
	}

	nfchain = osfw_nfchain_add(&self->chains, chain);
	if (!nfchain) {
		return false;
	}

	errcode = osfw_nftable_check(self, NULL);
	if (!errcode) {
		osfw_nfchain_del(nfchain);
		return false;
	}
	return true;
}

static bool osfw_nftable_del_nfchain(struct osfw_nftable *self, const char *chain)
{
	bool errcode = true;
	struct osfw_nfchain *nfchain = NULL;

	nfchain = osfw_nftable_get_nfchain(self, chain);
	if (!nfchain) {
		LOGE("Chain not found");
		return false;
	}

	errcode = osfw_nfchain_del(nfchain);
	if (!errcode) {
		return false;
	}
	return true;
}

static bool osfw_nftable_add_nfrule(struct osfw_nftable *self, const char *chain, int prio,
		const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nfrule *nfrule = NULL;

	if (!self->issupported) {
		LOGE("OSFW table add rule: table %s %s is not supported",
				osfw_convert_family(self->family), osfw_convert_table(self->table));
		return false;
	}

	nfrule = osfw_nfrule_add(&self->rules, chain, prio, match, target);
	if (!nfrule) {
		return false;
	}

	errcode = osfw_nftable_check(self, nfrule);
	if (!errcode) {
		osfw_nfrule_del(nfrule);
		return false;
	}
	return true;
}

static bool osfw_nftable_del_nfrule(struct osfw_nftable *self, const char *chain, int prio,
		const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nfrule *nfrule = NULL;

	nfrule = osfw_nftable_get_nfrule(self, chain, prio, match, target);
	if (!nfrule) {
		LOGE("Rule not found");
		return false;
	}

	errcode = osfw_nfrule_del(nfrule);
	if (!errcode) {
		return false;
	}
	return true;
}

static bool osfw_nfinet_set(struct osfw_nfinet *self, int family)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	self->family = family;
	self->ismodified = true;

	errcode = osfw_nftable_set(&self->tables.filter, self->family, OSFW_TABLE_FILTER);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_set(&self->tables.nat, self->family, OSFW_TABLE_NAT);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_set(&self->tables.mangle, self->family, OSFW_TABLE_MANGLE);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_set(&self->tables.raw, self->family, OSFW_TABLE_RAW);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_set(&self->tables.security, self->family, OSFW_TABLE_SECURITY);
	if (!errcode) {
		return false;
	}
	return true;
}

static bool osfw_nfinet_unset(struct osfw_nfinet *self)
{
	bool errcode = true;

	self->ismodified = true;
	errcode = osfw_nftable_unset(&self->tables.filter);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_unset(&self->tables.nat);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_unset(&self->tables.mangle);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_unset(&self->tables.raw);
	if (!errcode) {
		return false;
	}

	errcode = osfw_nftable_unset(&self->tables.security);
	if (!errcode) {
		return false;
	}
	return true;
}

static struct osfw_nftable *osfw_nfinet_get_nftable(struct osfw_nfinet *self, enum osfw_table table)
{
	struct osfw_nftable *nftable = NULL;

	switch (table) {
	case OSFW_TABLE_FILTER:
		nftable = &self->tables.filter;
		break;

	case OSFW_TABLE_NAT:
		nftable = &self->tables.nat;
		break;

	case OSFW_TABLE_MANGLE:
		nftable = &self->tables.mangle;
		break;

	case OSFW_TABLE_RAW:
		nftable = &self->tables.raw;
		break;

	case OSFW_TABLE_SECURITY:
		nftable = &self->tables.security;
		break;

	default:
		LOGE("OSFW inet get table: invalid table %d", table);
		break;
	}
	return nftable;
}

static bool osfw_nfinet_add_nfchain(struct osfw_nfinet *self, enum osfw_table table, const char *chain)
{
	bool errcode = true;
	struct osfw_nftable *nftable = NULL;

	nftable = osfw_nfinet_get_nftable(self, table);
	if (!nftable) {
		LOGE("Add OSFW chain: %s %s table not found", osfw_convert_family(self->family),
				osfw_convert_table(table));
		return false;
	}

	errcode = osfw_nftable_add_nfchain(nftable, chain);
	if (!errcode) {
		return false;
	}

	self->ismodified = true;
	return true;
}

static bool osfw_nfinet_del_nfchain(struct osfw_nfinet *self, enum osfw_table table, const char *chain)
{
	bool errcode = true;
	struct osfw_nftable *nftable = NULL;

	nftable = osfw_nfinet_get_nftable(self, table);
	if (!nftable) {
		LOGE("Delete OSFW chain: %s %s table not found", osfw_convert_family(self->family),
				osfw_convert_table(table));
		return false;
	}

	errcode = osfw_nftable_del_nfchain(nftable, chain);
	if (!errcode) {
		return false;
	}

	self->ismodified = true;
	return true;
}

static bool osfw_nfinet_add_nfrule(struct osfw_nfinet *self, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nftable *nftable = NULL;

	nftable = osfw_nfinet_get_nftable(self, table);
	if (!nftable) {
		LOGE("Add OSFW rule: %s %s table not found", osfw_convert_family(self->family),
				osfw_convert_table(table));
		return false;
	}

	errcode = osfw_nftable_add_nfrule(nftable, chain, prio, match, target);
	if (!errcode) {
		return false;
	}

	self->ismodified = true;
	return true;
}

static bool osfw_nfinet_del_nfrule(struct osfw_nfinet *self, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nftable *nftable = NULL;

	nftable = osfw_nfinet_get_nftable(self, table);
	if (!nftable) {
		LOGE("Delete OSFW rule: %s %s table not found", osfw_convert_family(self->family),
				osfw_convert_table(table));
		return false;
	}

	errcode = osfw_nftable_del_nfrule(nftable, chain, prio, match, target);
	if (!errcode) {
		return false;
	}

	self->ismodified = true;
	return true;
}

static bool osfw_nfinet_apply(struct osfw_nfinet *self)
{
	bool errcode = true;
	int err = 0;
	char path[OSFW_SIZE_CMD];
	char cmd[OSFW_SIZE_CMD];
	FILE *stream = NULL;

	if (!self->ismodified) {
		return true;
	}

	snprintf(path, sizeof(path) - 1, "/tmp/osfw-%s.%d", osfw_convert_family(self->family), (int) getpid());
	path[sizeof(path) - 1] = '\0';
	stream = fopen(path, "w+");
	if (!stream) {
		LOGE("Open %s failed: %d - %s", path, errno, strerror(errno));
		return false;
	}
	osfw_nftable_print(&self->tables.filter, true, NULL, stream);
	osfw_nftable_print(&self->tables.nat, true, NULL, stream);
	osfw_nftable_print(&self->tables.mangle, true, NULL, stream);
	osfw_nftable_print(&self->tables.raw, true, NULL, stream);
	osfw_nftable_print(&self->tables.security, true, NULL, stream);
	fclose(stream);

	snprintf(cmd, sizeof(cmd) - 1, "cat %s | %s", path, osfw_convert_cmd(self->family));
	cmd[sizeof(cmd) - 1] = '\0';
	err = cmd_log(cmd);
	if (err) {
		LOGE("Apply OSFW configuration failed");
		errcode = false;
		snprintf(cmd, sizeof(cmd) - 1, "cp %s %s.error", path, path);
		cmd[sizeof(cmd) - 1] = '\0';
		cmd_log(cmd);
	}

	unlink(path);
	self->ismodified = false;
	return errcode;
}

static bool osfw_nfbase_set(struct osfw_nfbase *self)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	errcode = osfw_nfinet_set(&self->inet, AF_INET);
	if (!errcode) {
		LOGE("Set OSFW base: set OSFW inet failed");
		return false;
	}

	errcode = osfw_nfinet_set(&self->inet6, AF_INET6);
	if (!errcode) {
		LOGE("Set OSFW base: set OSFW inet6 failed");
		return false;
	}
	return true;
}

static bool osfw_nfbase_unset(struct osfw_nfbase *self)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	errcode = osfw_nfinet_unset(&self->inet);
	if (!errcode) {
		LOGE("Unset OSFW base: unset OSFW inet failed");
		return false;
	}

	errcode = osfw_nfinet_unset(&self->inet6);
	if (!errcode) {
		LOGE("Unset OSFW base: unset OSFW inet6 failed");
		return false;
	}
	return true;
}

static bool osfw_nfbase_apply(struct osfw_nfbase *self)
{
	bool errcode = true;

	errcode = osfw_nfinet_apply(&self->inet);
	if (!errcode) {
		LOGE("Apply OSFW base: apply OSFW inet failed");
		return false;
	}

	errcode = osfw_nfinet_apply(&self->inet6);
	if (!errcode) {
		LOGE("Apply OSFW base: apply OSFW inet6 failed");
		return false;
	}
	return true;
}

static struct osfw_nfinet *osfw_nfbase_get_inet(struct osfw_nfbase *self, int family)
{
	struct osfw_nfinet *nfinet = NULL;

	switch (family) {
	case AF_INET:
		nfinet = &self->inet;
		break;

	case AF_INET6:
		nfinet = &self->inet6;
		break;

	default:
		LOGE("OSFW base get table: invalid family %d", family);
		break;
	}
	return nfinet;
}

bool osfw_init(void)
{
	bool errcode = true;

	errcode = osfw_nfbase_set(&osfw_nfbase);
	if (!errcode) {
		LOGE("Initialize OSFW: set base failed");
		return false;
	}

	errcode = osfw_apply();
	if (!errcode) {
		LOGE("Initialize OSFW: apply failed");
		return false;
	}
	return true;
}

bool osfw_fini(void)
{
	bool errcode = true;

	errcode = osfw_nfbase_unset(&osfw_nfbase);
	if (!errcode) {
		LOGE("Finalize OSFW: unset base failed");
		return false;
	}

	errcode = osfw_apply();
	if (!errcode) {
		LOGE("Finalize OSFW: apply failed");
		return false;
	}
	return true;
}

bool osfw_chain_add(int family, enum osfw_table table, const char *chain)
{
	bool errcode = true;
	struct osfw_nfinet *nfinet = NULL;

	if (!chain || !chain[0]) {
		LOGE("Add OSFW chain: invalid parameters");
		return false;
	} else if (!osfw_is_valid_chain(chain)) {
		LOGE("Add OSFW chain: %s is not a valid chain", chain);
		return false;
	} else if (osfw_is_builtin_chain(table, chain)) {
		LOGD("Add OSFW chain: %s %s is built-in chain", osfw_convert_table(table), chain);
		return true;
	}

	nfinet = osfw_nfbase_get_inet(&osfw_nfbase, family);
	if (!nfinet) {
		LOGE("Add OSFW chain: %s not found", osfw_convert_family(family));
		return false;
	}

	errcode = osfw_nfinet_add_nfchain(nfinet, table, chain);
	if (!errcode) {
		LOGE("Add OSFW chain: add chain failed");
		return false;
	}
	return true;
}

bool osfw_chain_del(int family, enum osfw_table table, const char *chain)
{
	bool errcode = true;
	struct osfw_nfinet *nfinet = NULL;

	if (!chain || !chain[0]) {
		LOGE("Delete OSFW chain: invalid parameters");
		return false;
	} else if (!osfw_is_valid_chain(chain)) {
		LOGE("Delete OSFW chain: %s is not a valid chain", chain);
		return false;
	} else if (osfw_is_builtin_chain(table, chain)) {
		LOGD("Delete OSFW chain: %s %s is built-in chain", osfw_convert_table(table), chain);
		return true;
	}

	nfinet = osfw_nfbase_get_inet(&osfw_nfbase, family);
	if (!nfinet) {
		LOGE("Delete OSFW chain: %s not found", osfw_convert_family(family));
		return false;
	}

	errcode = osfw_nfinet_del_nfchain(nfinet, table, chain);
	if (!errcode) {
		LOGE("Delete OSFW chain: delete chain failed");
		return false;
	}
	return true;
}

bool osfw_rule_add(int family, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nfinet *nfinet = NULL;

	if (!chain || !chain[0] || !match || !target || !target[0]) {
		LOGE("Add OSFW rule: invalid parameters");
		return false;
	}

	nfinet = osfw_nfbase_get_inet(&osfw_nfbase, family);
	if (!nfinet) {
		LOGE("Add OSFW rule: %s not found", osfw_convert_family(family));
		return false;
	}

	errcode = osfw_nfinet_add_nfrule(nfinet, table, chain, prio, match, target);
	if (!errcode) {
		LOGE("Add OSFW rule: add rule failed");
		return false;
	}
	return true;
}

bool osfw_rule_del(int family, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target)
{
	bool errcode = true;
	struct osfw_nfinet *nfinet = NULL;

	if (!chain || !chain[0] || !match || !target || !target[0]) {
		LOGE("Delete OSFW rule: invalid parameters");
		return false;
	}

	nfinet = osfw_nfbase_get_inet(&osfw_nfbase, family);
	if (!nfinet) {
		LOGE("Delete OSFW rule: %s not found", osfw_convert_family(family));
		return false;
	}

	errcode = osfw_nfinet_del_nfrule(nfinet, table, chain, prio, match, target);
	if (!errcode) {
		LOGE("Delete OSFW rule: delete rule failed");
		return false;
	}
	return true;
}

bool osfw_apply(void)
{
	bool errcode = true;

	errcode = osfw_nfbase_apply(&osfw_nfbase);
	if (!errcode) {
		LOGE("Apply OSFW configuration failed");
		return false;
	}
	return true;
}

