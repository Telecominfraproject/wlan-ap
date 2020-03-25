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
#include "nfm_rule.h"
#include "nfm_chain.h"
#include "nfm_ovsdb.h"
#include "nfm_osfw.h"
#include "ovsdb_sync.h"
#include <string.h>
#include <stdio.h>

#define MODULE_ID LOG_MODULE_ID_MAIN

static struct ds_tree nfm_rule_tree = DS_TREE_INIT((ds_key_cmp_t *) strcmp, struct nfm_rule, elt);

/*
* Ignore change on these columns:
* - _uuid
* - status
*/
static bool nfm_rule_is_conf_modified(const struct schema_Netfilter *conf)
{
	if (!conf) {
		return false;
	}

	if (conf->name_changed || conf->enable_changed || conf->protocol_changed || conf->table_changed ||
			conf->chain_changed || conf->priority_changed || conf->rule_changed ||
			conf->target_changed) {
		return true;
	}
	return false;
}

static void nfm_rule_set_status(struct nfm_rule *self, const char *status)
{
	struct schema_Netfilter set;
	json_t *where = NULL;
	int rc = 0;

	if (!self || !status ||
			(strncmp(status, "disabled", sizeof("disabled")) &&
			strncmp(status, "enabled", sizeof("enabled")) &&
			strncmp(status, "error", sizeof("error")))) {
		LOGE("Set Nefilter status: invalid parameter (%p - %s)", self, status);
		return;
	} else if (!strncmp(self->conf.status, status, sizeof(self->conf.status))) {
		return;
	}

	memset(&set, 0, sizeof(set));
	set._partial_update = true;
	SCHEMA_SET_STR(set.status, status);

	where = ovsdb_where_simple(SCHEMA_COLUMN(Netfilter, name), self->conf.name);
	if (!where) {
		LOGW("[%s] Set Nefilter status: rule doesn't exist", self->conf.name);
		return;
	}

	rc = ovsdb_table_update_where(&table_Netfilter, where, &set);
	if (rc != 1) {
		LOGE("[%s] Set Nefilter status: unexpected result count %d", self->conf.name, rc);
		return;
	}

	strncpy(self->conf.status, status, sizeof(self->conf.status) - 1);
	self->conf.status[sizeof(self->conf.status) - 1] = '\0';
	LOGD("[%s] Netfilter rule status is %s", self->conf.name, status);
}

static bool nfm_rule_get_ref_chain(struct nfm_rule *self)
{
	bool errcode = true;

	if (nfm_osfw_is_inet4(self->conf.protocol)) {
		errcode = nfm_chain_get_ref(AF_INET, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: get reference on %s inet chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags |= NFM_FLAG_RULE_CHAIN4_REFERENCED;

		errcode = nfm_chain_get_ref(AF_INET, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: get reference on %s inet target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags |= NFM_FLAG_RULE_TARGET4_REFERENCED;
	}

	if (nfm_osfw_is_inet6(self->conf.protocol)) {
		errcode = nfm_chain_get_ref(AF_INET6, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: get reference on %s inet6 chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags |= NFM_FLAG_RULE_CHAIN6_REFERENCED;

		errcode = nfm_chain_get_ref(AF_INET6, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: get reference on %s inet6 target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags |= NFM_FLAG_RULE_TARGET6_REFERENCED;
	}
	return true;
}
static bool nfm_rule_set(struct nfm_rule *self, const struct schema_Netfilter *conf)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	self->conf = *conf;
	ds_tree_insert(&nfm_rule_tree, self, self->conf.name);
	self->flags |= NFM_FLAG_RULE_IN_TREE;

	if (self->conf.enable) {
		errcode = nfm_rule_get_ref_chain(self);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: get reference on chain failed", self->conf.name);
			goto error;
		}

		errcode = nfm_osfw_add_rule(&self->conf);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: SDK layer failed", self->conf.name);
			goto error;
		}
		self->flags |= NFM_FLAG_RULE_APPLIED;

		nfm_rule_set_status(self, "enabled");
	} else {
		nfm_rule_set_status(self, "disabled");
	}
	return true;

error:
	/* Set the status to "error" and return success */
	nfm_rule_set_status(self, "error");
	return true;
}

static bool nfm_rule_unset(struct nfm_rule *self)
{
	bool errcode = true;

	if (self->flags & NFM_FLAG_RULE_APPLIED) {
		errcode = nfm_osfw_del_rule(&self->conf);
		if (!errcode) {
			return false;
		}
		self->flags &= ~NFM_FLAG_RULE_APPLIED;
	}

	if (self->flags & NFM_FLAG_RULE_TARGET4_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: put reference on %s inet target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags &= ~NFM_FLAG_RULE_TARGET4_REFERENCED;
	}

	if (self->flags & NFM_FLAG_RULE_CHAIN4_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: put reference on %s inet chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags &= ~NFM_FLAG_RULE_CHAIN4_REFERENCED;
	}

	if (self->flags & NFM_FLAG_RULE_TARGET6_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET6, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: put reference on %s inet6 target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags &= ~NFM_FLAG_RULE_TARGET6_REFERENCED;
	}

	if (self->flags & NFM_FLAG_RULE_CHAIN6_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET6, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter rule: put reference on %s inet6 chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags &= ~NFM_FLAG_RULE_CHAIN6_REFERENCED;
	}

	if (self->flags & NFM_FLAG_RULE_IN_TREE) {
		ds_tree_remove(&nfm_rule_tree, self);
		self->flags &= ~NFM_FLAG_RULE_IN_TREE;
	}
	memset(self, 0, sizeof(*self));
	return true;
}

static bool nfm_rule_destroy(struct nfm_rule *self)
{
	bool errcode = true;

	errcode = nfm_rule_unset(self);
	if (!errcode) {
		LOGE("[%s] Delete Netfilter rule: unset rule failed", self->conf.name);
		return false;
	}
	free(self);
	return true;
}

static struct nfm_rule *nfm_rule_create(const struct schema_Netfilter *conf)
{
	struct nfm_rule *self = NULL;
	bool errcode = true;

	self = malloc(sizeof(*self));
	if (!self) {
		LOGE("[%s] Create Netfilter rule: memory allocation failed", conf->name);
		return NULL;
	}

	errcode = nfm_rule_set(self, conf);
	if (!errcode) {
		LOGE("[%s] Create Netfilter rule: set rule failed", conf->name);
		nfm_rule_destroy(self);
		return NULL;
	}
	return self;
}

static struct nfm_rule *nfm_rule_get(const char *name)
{
	if (!name) {
		return NULL;
	}
	return (struct nfm_rule *) ds_tree_find(&nfm_rule_tree, (void *) name);
}

bool nfm_rule_init(void)
{
	return true;
}

bool nfm_rule_new(const struct schema_Netfilter *conf)
{
	struct nfm_rule *self = NULL;

	if (!conf) {
		LOGE("New Netfilter rule: invalid parameter");
		return false;
	}

	self = nfm_rule_get(conf->name);
	if (self) {
		LOGE("[%s] New Netfilter rule: already exists", conf->name);
		return false;
	}

	self = nfm_rule_create(conf);
	if (!self) {
		LOGE("[%s] New Netfilter rule: create rule failed", conf->name);
		return false;
	}
	LOGD("[%s] Netfilter rule is created", conf->name);
	return true;
}

bool nfm_rule_del(const struct schema_Netfilter *conf)
{
	struct nfm_rule *self = NULL;
	bool errcode = true;

	if (!conf) {
		LOGE("Delete Netfilter rule: invalid parameter");
		return false;
	}

	self = nfm_rule_get(conf->name);
	if (!self) {
		LOGE("[%s] Delete Netfilter rule: not found", conf->name);
		return false;
	}

	errcode = nfm_rule_destroy(self);
	if (!errcode) {
		LOGE("[%s] Delete Netfilter rule: destroy rule failed", conf->name);
		return false;
	}
	LOGD("[%s] Netfilter rule is destroyed", conf->name);
	return true;
}

bool nfm_rule_modify(const struct schema_Netfilter *conf)
{
	bool errcode = true;

	if (!nfm_rule_is_conf_modified(conf)) {
		return true;
	}
	errcode = nfm_rule_del(conf);
	if (!errcode) {
		LOGE("[%s] Modify Netfilter rule: delete rule failed", conf->name);
		return false;
	}
	errcode = nfm_rule_new(conf);
	if (!errcode) {
		LOGE("[%s] Modify Netfilter rule: new rule failed", conf->name);
		return false;
	}
	return true;
}

