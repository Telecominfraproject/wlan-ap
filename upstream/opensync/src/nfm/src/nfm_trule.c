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
#include "nfm_trule.h"
#include "nfm_chain.h"
#include "nfm_ovsdb.h"
#include "nfm_osfw.h"
#include "ovsdb_sync.h"
#include "policy_tags.h"
#include <string.h>
#include <stdio.h>

#define MODULE_ID LOG_MODULE_ID_MAIN

static struct ds_tree nfm_trule_tree = DS_TREE_INIT((ds_key_cmp_t *) strcmp, struct nfm_trule, elt);

/*
* Ignore change on these columns:
* - _uuid
* - status
*/
static bool nfm_trule_is_conf_modified(const struct schema_Netfilter *conf)
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

static void nfm_trule_set_status(struct nfm_trule *self, const char *status)
{
	struct schema_Netfilter set;
	json_t *where = NULL;
	int rc = 0;

	if (!self || !status ||
			(strncmp(status, "disabled", sizeof("disabled")) &&
			strncmp(status, "enabled", sizeof("enabled")) &&
			strncmp(status, "error", sizeof("error")))) {
		LOGE("Set Nefilter template status: invalid parameter(%p - %s)", self, status);
		return;
	} else if (!strncmp(self->conf.status, status, sizeof(self->conf.status))) {
		return;
	}

	memset(&set, 0, sizeof(set));
	set._partial_update = true;
	SCHEMA_SET_STR(set.status, status);

	where = ovsdb_where_simple(SCHEMA_COLUMN(Netfilter, name), self->conf.name);
	if (!where) {
		LOGW("[%s] Set Nefilter template status: rule doesn't exist", self->conf.name);
		return;
	}

	rc = ovsdb_table_update_where(&table_Netfilter, where, &set);
	if (rc != 1) {
		LOGE("[%s] Set Nefilter template status: unexpected result count %d", self->conf.name, rc);
		return;
	}

	strncpy(self->conf.status, status, sizeof(self->conf.status) - 1);
	self->conf.status[sizeof(self->conf.status) - 1] = '\0';
	LOGD("Netfilter template rule %s status is %s", self->conf.name, status);
}

static bool nfm_trule_detect_vars(struct nfm_trule *self, char *what, char var_chr, char begin,
		char end, uint8_t base_flags)
{
	uint8_t flag = 0;
	char *mrule = NULL;
	char *p = NULL;
	char *s = NULL;
	bool errcode = true;

	mrule = strdup(self->conf.rule);
	if (!mrule) {
		LOGE("Memory allocation failed");
		return false;
	}

	p = mrule;
	s = p;
	while ((s = strchr(s, var_chr))) {
		s++;
		if (*s != begin) {
			continue;
		}
		s++;

		flag = base_flags;
		if (*s == TEMPLATE_DEVICE_CHAR) {
			s++;
			flag |= OM_TLE_FLAG_DEVICE;
		} else if (*s == TEMPLATE_CLOUD_CHAR) {
			s++;
			flag |= OM_TLE_FLAG_CLOUD;
		}
		if (!(p = strchr(s, end))) {
			LOGW("[%s] Netfilter template rule has malformed %s (no ending '%c')",
					self->conf.name, what, end);
			continue;
		}
		*p++ = '\0';

		LOGT("[%s] Netfilter template rule detected %s %s'%s'",
				self->conf.name, what, (flag == OM_TLE_FLAG_DEVICE) ? "device " :
				(flag == OM_TLE_FLAG_CLOUD) ? "cloud " : "", s);
		if (!om_tag_list_entry_find_by_val_flags(&self->tags, s, base_flags)) {
			if (!om_tag_list_entry_add(&self->tags, s, flag)) {
				errcode = false;
				goto out;
			}
		}
		s = p;
	}

out:
	free(mrule);
	if (!errcode) {
		om_tag_list_free(&self->tags);
	}
	return errcode;
}

static bool nfm_trule_detect_tags(struct nfm_trule *self)
{
	return nfm_trule_detect_vars(self, "tag", TEMPLATE_VAR_CHAR, TEMPLATE_TAG_BEGIN,
			TEMPLATE_TAG_END, 0);
}

static bool nfm_trule_detect_groups(struct nfm_trule *self)
{
	return nfm_trule_detect_vars(self, "tag group", TEMPLATE_VAR_CHAR, TEMPLATE_GROUP_BEGIN,
			TEMPLATE_GROUP_END, OM_TLE_FLAG_GROUP);
}

static size_t nfm_trule_calculate_len(const struct nfm_trule *self, const struct nfm_tdata *tdata)
{
	size_t len = 0;
	size_t i = 0;

	len = strlen(self->conf.rule);
	for (i = 0; i < tdata->tv_cnt; i++) {
		len -= (strlen(tdata->tv[i].name) + 3 /* ${} */);
		len += strlen(tdata->tv[i].value);
	}

	/* Add room for NULL termination */
	return len + 1;
}

static char *nfm_trule_get_tag_value(struct nfm_tdata *tdata, char *tag_name, bool group)
{
	size_t i = 0;

	for (i = 0; i < tdata->tv_cnt; i++) {
		if (!strcmp(tdata->tv[i].name, tag_name) && tdata->tv[i].group == group) {
			return tdata->tv[i].value;
		}
	}
	return NULL;
}

static char *nfm_trule_expand(struct nfm_trule *self, struct nfm_tdata *tdata)
{
	char *mrule = NULL;
	char *erule = NULL;
	char *nval = NULL;
	char *p = NULL;
	char *s = NULL;
	char *e = NULL;
	char end = '\0';
	bool group = false;
	int nlen = 0;

	/* Duplicate rule we can modify */
	if (!(mrule = strdup(self->conf.rule))) {
		LOGE("[%s] Expand Netfilter template rule: memory allocation failed", self->conf.name);
		goto error;
	}

	/* Determine new length, and allocate memory for expanded rule */
	nlen = nfm_trule_calculate_len(self, tdata);
	if (!(erule = calloc(1, nlen))) {
		LOGE("[%s] Expand Netfilter template rule: memory allocation failed", self->conf.name);
		goto error;
	}

	/* Copy rule, replacing tags */
	p = mrule;
	s = p;
	while ((s = strchr(s, TEMPLATE_VAR_CHAR))) {
		if (*(s + 1) == TEMPLATE_TAG_BEGIN) {
			end = TEMPLATE_TAG_END;
			group = false;
		} else if (*(s + 1) == TEMPLATE_GROUP_BEGIN) {
			end = TEMPLATE_GROUP_END;
			group = true;
		} else {
			s++;
			continue;
		}

		*s = '\0';
		strcat(erule, p);

		s += 2;
		if (*s == TEMPLATE_DEVICE_CHAR || *s == TEMPLATE_CLOUD_CHAR) {
			s++;
		}
		if (!(e = strchr(s, end))) {
			LOGE("[%s] Expand template rule: expand tags failed", self->conf.name);
			goto error;
		}
		*e++ = '\0';
		p = e;

		if (!(nval = nfm_trule_get_tag_value(tdata, s, group))) {
			LOGE("[%s] Expand template rule: %stag '%s' not found", self->conf.name, group ? "group " : "", s);
			goto error;
		}
		strcat(erule, nval);

		s = p;
	}

	if (*p != '\0') {
		strcat(erule, p);
	}
	free(mrule);
	return erule;

error:
	free(mrule);
	free(erule);
	return NULL;
}

static bool nfm_trule_apply(struct nfm_trule *self, om_action_t type, struct nfm_tdata *tdata)
{
	struct schema_Netfilter conf;
	char *rule;
	bool errcode = false;

	rule = nfm_trule_expand(self, tdata);
	if (!rule) {
		LOGE("[%s] Apply Nefilter template rule: expand rule failed", self->conf.name);
		return false;
	}

	conf = self->conf;
	strncpy(conf.rule, rule, sizeof(conf.rule) - 1);
	conf.rule[sizeof(conf.rule) - 1] = '\0';

	switch (type) {
	case ADD:
		LOGD("[%s] Apply expanded template rule: add '%s'", self->conf.name, rule);
		errcode = nfm_osfw_add_rule(&conf);
		if (!errcode) {
			LOGE("[%s] Apply Nefilter template rule failed", self->conf.name);
		}
		break;

	case DELETE:
		LOGD("[%s] Apply expanded template rule: remove '%s'", self->conf.name, rule);
		errcode = nfm_osfw_del_rule(&conf);
		if (!errcode) {
			LOGE("[%s] Apply Nefilter template rule failed", self->conf.name);
		}
		break;

	default:
		LOGE("[%s] Apply Nefilter template rule: unknown type %d", self->conf.name, type);
		break;

	}

	free(rule);
	return errcode;
}

static bool nfm_trule_apply_tag(struct nfm_trule *self, om_action_t type,
		om_tag_list_entry_t *ttle, ds_tree_iter_t *iter, struct nfm_tdata *tdata, size_t tdn)
{
	om_tag_list_entry_t *tle = NULL;
	om_tag_list_entry_t *ntle = NULL;
	om_tag_list_entry_t *ftle = NULL;
	struct ds_tree_iter *niter = NULL;
	enum nfm_tag_filter filter = NFM_TAG_FILTER_NORMAL;
	struct ds_tree *tlist = NULL;
	om_tag_t *tag = NULL;
	uint8_t filter_flags = 0;
	bool errcode = true;

	if (!ttle) {
		LOGE("[%s] Tag list entry is invalid", self->conf.name);
		return false;
	}

	if (tdn == 0) {
		LOGI("[%s] Apply tag for Netfilter template rule: %s system rules from template rule %s",
				self->conf.name, (type == ADD) ? "Adding" : "Removing",
				ttle->value);
	}

	if (tdata->tag_override_name && !strcmp(ttle->value, tdata->tag_override_name)) {
		tlist  = tdata->tag_override_values;
		filter = tdata->filter;
	} else {
		tag = om_tag_find_by_name(ttle->value, (ttle->flags & OM_TLE_FLAG_GROUP) ? true : false);
		if (!tag) {
			LOGW("[%s] Apply tag for Netfilter template rule: %stag '%s' not found",
					self->conf.name,
					(ttle->flags & OM_TLE_FLAG_GROUP) ? "group " : "",
					ttle->value);
			return false;
		}
		tlist = &tag->values;
	}

	if (!(ftle = om_tag_list_entry_find_by_val_flags(&self->tags, ttle->value, ttle->flags))) {
		LOGW("[%s] Apply tag for Netfilter template rule: does not contain %stag '%s'",
				self->conf.name,
				(ttle->flags & OM_TLE_FLAG_GROUP) ? "group " : "",
				ttle->value);
		return false;
	}
	filter_flags = OM_TLE_VAR_FLAGS(ftle->flags);

	ntle = ds_tree_inext(iter);

	tdata->tv[tdn].name  = ttle->value;
	tdata->tv[tdn].group = (ttle->flags & OM_TLE_FLAG_GROUP) ? true : false;
	ds_tree_foreach(tlist, tle) {
		switch (filter) {
		default:
		case NFM_TAG_FILTER_NORMAL:
			if (filter_flags != 0 && (tle->flags & filter_flags) == 0) {
				continue;
			}
			break;

		case NFM_TAG_FILTER_MATCH:
			if (filter_flags == 0 || (tle->flags & filter_flags) == 0) {
				continue;
			}
			break;

		case NFM_TAG_FILTER_MISMATCH:
			if (filter_flags == 0 || (tle->flags & filter_flags) != 0) {
				continue;
			}
			break;
		}

		tdata->tv[tdn].value = tle->value;
		if (ntle) {
			if ((tdn+1) >= (sizeof(tdata->tv)/sizeof(struct nfm_tdata_tv))) {
				LOGE("[%s] Apply tag for Netfilter template rule: too many tags", self->conf.name);
				return false;
			}
			if (!(niter = malloc(sizeof(*niter)))) {
				LOGE("[%s] Apply tag for Netfilter template rule: memory allocation failed",
						self->conf.name);
				return false;
			}
			memcpy(niter, iter, sizeof(*niter));

			errcode = nfm_trule_apply_tag(self, type, ntle, niter, tdata, tdn + 1);
			free(niter);
			if (!errcode) {
				break;
			}
		} else {
			tdata->tv_cnt = tdn+1;
			errcode = nfm_trule_apply(self, type, tdata);
			if (!errcode) {
				if (!tdata->ignore_err) {
					break;
				}
				errcode = true;
			}
		}
	}
	return errcode;
}

bool nfm_trule_update_tags(struct nfm_trule *self, om_action_t type)
{
	bool errcode = true;
	om_tag_list_entry_t *tle;
	struct ds_tree_iter iter;
	struct nfm_tdata tdata;

	if (ds_tree_head(&self->tags)) {
		memset(&tdata, 0, sizeof(tdata));
		tdata.filter = NFM_TAG_FILTER_NORMAL;
		tdata.ignore_err = false;
		tle = ds_tree_ifirst(&iter, &self->tags);
		errcode = nfm_trule_apply_tag(self, type, tle, &iter, &tdata, 0);
		if (!errcode) {
			return false;
		}
	}
	return true;
}

static bool nfm_trule_set_tags(struct nfm_trule *self)
{
	bool errcode = true;

	errcode = nfm_trule_detect_tags(self);
	if (!errcode) {
		LOGE("[%s] Set Nefilter template rule: detect tags failed", self->conf.name);
		return false;
	}

	errcode = nfm_trule_detect_groups(self);
	if (!errcode) {
		LOGE("[%s] Set Nefilter template rule: detect groups failed", self->conf.name);
		return false;
	}

	errcode = nfm_trule_update_tags(self, ADD);
	if (!errcode) {
		LOGE("[%s] Set Nefilter template rule: update tags failed", self->conf.name);
		return false;
	}
	return true;
}

static bool nfm_trule_unset_tags(struct nfm_trule *self)
{
	bool errcode = true;

	errcode = nfm_trule_update_tags(self, DELETE);
	if (!errcode) {
		LOGE("[%s] Set Nefilter template rule: update tags failed", self->conf.name);
		return false;
	}

	if (self->flags & NFM_FLAG_TRULE_TAG_LIST_INIT) {
		om_tag_list_free(&self->tags);
		self->flags &= ~NFM_FLAG_TRULE_TAG_LIST_INIT;
	}
	return true;
}

static bool nfm_trule_get_ref_chain(struct nfm_trule *self)
{
	bool errcode = true;

	if (nfm_osfw_is_inet4(self->conf.protocol)) {
		errcode = nfm_chain_get_ref(AF_INET, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter template rule: get reference on %s inet chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags |= NFM_FLAG_TRULE_CHAIN4_REFERENCED;

		errcode = nfm_chain_get_ref(AF_INET, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter template rule: get reference on %s inet target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags |= NFM_FLAG_TRULE_TARGET4_REFERENCED;
	}

	if (nfm_osfw_is_inet6(self->conf.protocol)) {
		errcode = nfm_chain_get_ref(AF_INET6, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Start Nefilter template rule: get reference on %s inet6 chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags |= NFM_FLAG_TRULE_CHAIN6_REFERENCED;

		errcode = nfm_chain_get_ref(AF_INET6, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Start Nefilter template rule: get reference on %s inet6 target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags |= NFM_FLAG_TRULE_TARGET6_REFERENCED;
	}
	return true;
}

static bool nfm_trule_set(struct nfm_trule *self, const struct schema_Netfilter *conf)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	self->conf = *conf;
	ds_tree_insert(&nfm_trule_tree, self, self->conf.name);
	self->flags |= NFM_FLAG_TRULE_IN_TREE;
	om_tag_list_init(&self->tags);
	self->flags |= NFM_FLAG_TRULE_TAG_LIST_INIT;

	if (self->conf.enable) {
		errcode = nfm_trule_get_ref_chain(self);
		if (!errcode) {
			LOGE("[%s] Set Nefilter template rule: get reference on chain failed", self->conf.name);
			goto error;
		}

		errcode = nfm_trule_set_tags(self);
		if (!errcode) {
			LOGE("[%s] Set Nefilter template rule: set tags failed", self->conf.name);
			goto error;
		}

		nfm_trule_set_status(self, "enabled");
	} else {
		nfm_trule_set_status(self, "disabled");
	}
	return true;

error:
	/* Set the status to "error" and return success */
	nfm_trule_set_status(self, "error");
	return true;
}

static bool nfm_trule_unset(struct nfm_trule *self)
{
	bool errcode = true;

	errcode = nfm_trule_unset_tags(self);
	if (!errcode) {
		LOGE("[%s] Unset Nefilter template rule: unset tags failed", self->conf.name);
		return false;
	}

	if (self->flags & NFM_FLAG_TRULE_TARGET4_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Unset Nefilter template rule: put reference on %s inet target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags &= ~NFM_FLAG_TRULE_TARGET4_REFERENCED;
	}

	if (self->flags & NFM_FLAG_TRULE_CHAIN4_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Unset Nefilter template rule: put reference on %s inet chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags &= ~NFM_FLAG_TRULE_CHAIN4_REFERENCED;
	}

	if (self->flags & NFM_FLAG_TRULE_TARGET6_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET6, self->conf.table, self->conf.target);
		if (!errcode) {
			LOGE("[%s] Unset Nefilter template rule: put reference on %s inet6 target failed",
					self->conf.name, self->conf.target);
			return false;
		}
		self->flags &= ~NFM_FLAG_TRULE_TARGET6_REFERENCED;
	}

	if (self->flags & NFM_FLAG_TRULE_CHAIN6_REFERENCED) {
		errcode = nfm_chain_put_ref(AF_INET6, self->conf.table, self->conf.chain);
		if (!errcode) {
			LOGE("[%s] Unset Nefilter template rule: put reference on %s inet6 chain failed",
					self->conf.name, self->conf.chain);
			return false;
		}
		self->flags &= ~NFM_FLAG_TRULE_CHAIN6_REFERENCED;
	}

	if (self->flags & NFM_FLAG_TRULE_IN_TREE) {
		ds_tree_remove(&nfm_trule_tree, self);
		self->flags &= ~NFM_FLAG_TRULE_IN_TREE;
	}
	memset(self, 0, sizeof(*self));
	return true;
}

static bool nfm_trule_destroy(struct nfm_trule *self)
{
	bool errcode = true;

	errcode = nfm_trule_unset(self);
	if (!errcode) {
		LOGE("[%s] Destroy Netfilter template rule: unset rule failed", self->conf.name);
		return false;
	}
	free(self);
	return true;
}

static struct nfm_trule *nfm_trule_create(const struct schema_Netfilter *conf)
{
	struct nfm_trule *self = NULL;
	bool errcode = true;

	self = malloc(sizeof(*self));
	if (!self) {
		LOGE("[%s] Create Netfilter template rule: memory allocation failed", conf->name);
		return NULL;
	}

	errcode = nfm_trule_set(self, conf);
	if (!errcode) {
		LOGE("[%s] Create Netfilter template rule: set rule failed", conf->name);
		nfm_trule_destroy(self);
		return NULL;
	}
	return self;
}

static struct nfm_trule *nfm_trule_get(const char *name)
{
	if (!name) {
		return NULL;
	}
	return (struct nfm_trule *) ds_tree_find(&nfm_trule_tree, (void *) name);
}

bool nfm_trule_on_tag_update(struct nfm_trule *self, om_tag_t *tag, struct ds_tree *removed, struct ds_tree *added, struct ds_tree *updated)
{
	bool errcode = true;
	om_tag_list_entry_t *tle = NULL;
	struct ds_tree_iter iter;
	struct nfm_tdata tdata;

	if (!om_tag_list_entry_find_by_value(&self->tags, tag->name)) {
		return true;
	}

	/* Do removals first */
	if (removed && ds_tree_head(removed)) {
		memset(&tdata, 0, sizeof(tdata));
		tdata.tag_override_name = tag->name;
		tdata.tag_override_values = removed;
		tdata.filter = NFM_TAG_FILTER_NORMAL;
		tdata.ignore_err = false;
		tle = ds_tree_ifirst(&iter, &self->tags);
		if (!nfm_trule_apply_tag(self, DELETE, tle, &iter, &tdata, 0)) {
			errcode = false;
		}
	}

	/* Now do additions */
	if (added && ds_tree_head(added)) {
		memset(&tdata, 0, sizeof(tdata));
		tdata.tag_override_name = tag->name;
		tdata.tag_override_values = added;
		tdata.filter = NFM_TAG_FILTER_NORMAL;
		tdata.ignore_err = false;
		tle = ds_tree_ifirst(&iter, &self->tags);
		if (!nfm_trule_apply_tag(self, ADD, tle, &iter, &tdata, 0)) {
			errcode = false;
		}
	}

	/* Now do updates */
	if (updated && ds_tree_head(updated)) {
		/* Remove unmatching first */
		memset(&tdata, 0, sizeof(tdata));
		tdata.tag_override_name = tag->name;
		tdata.tag_override_values = updated;
		tdata.filter = NFM_TAG_FILTER_MISMATCH;
		/* Some rules may already not exist */
		tdata.ignore_err = true;
		tle = ds_tree_ifirst(&iter, &self->tags);
		if (!nfm_trule_apply_tag(self, DELETE, tle, &iter, &tdata, 0)) {
			errcode = false;
		}

		/* Add matching now */
		memset(&tdata, 0, sizeof(tdata));
		tdata.tag_override_name = tag->name;
		tdata.tag_override_values = updated;
		tdata.filter = NFM_TAG_FILTER_MATCH;
		tdata.ignore_err = false;
		tle = ds_tree_ifirst(&iter, &self->tags);
		if (!nfm_trule_apply_tag(self, ADD, tle, &iter, &tdata, 0)) {
			errcode = false;
		}
	}
	return errcode;
}

static bool nfm_trule_on_tag_update_all(om_tag_t *tag, struct ds_tree *removed, struct ds_tree *added, struct ds_tree *updated)
{
	struct nfm_trule *self = NULL;
	bool errcode = true;

	ds_tree_foreach(&nfm_trule_tree, self) {
		if (!nfm_trule_on_tag_update(self, tag, removed, added, updated)) {
			errcode = false;
		}
	}
	return errcode;
}

bool nfm_trule_init(void)
{
	struct tag_mgr tagmgr;

	memset(&tagmgr, 0, sizeof(tagmgr));
	tagmgr.service_tag_update = nfm_trule_on_tag_update_all;
	om_tag_init(&tagmgr);
	return true;
}

bool nfm_trule_new(const struct schema_Netfilter *conf)
{
	struct nfm_trule *self = NULL;

	if (!conf) {
		LOGE("New Netfilter template rule: invalid parameter");
		return false;
	}

	self = nfm_trule_get(conf->name);
	if (self) {
		LOGE("[%s] New Netfilter template rule: already exists", conf->name);
		return false;
	}

	self = nfm_trule_create(conf);
	if (!self) {
		LOGE("[%s] New Netfilter template rule: create rule failed", conf->name);
		return false;
	}
	LOGD("[%s] Netfilter template rule is created", conf->name);
	return true;
}

bool nfm_trule_del(const struct schema_Netfilter *conf)
{
	struct nfm_trule *self = NULL;
	bool errcode = true;

	if (!conf) {
		LOGE("Delete Netfilter template rule: invalid parameter");
		return false;
	}

	self = nfm_trule_get(conf->name);
	if (!self) {
		LOGE("[%s] Delete Netfilter template rule: not found", conf->name);
		return false;
	}

	errcode = nfm_trule_destroy(self);
	if (!errcode) {
		LOGE("[%s] Delete Netfilter template rule: destroy rule failed", conf->name);
		return false;
	}
	LOGD("[%s] Netfilter template rule is destroyed", conf->name);
	return true;
}

bool nfm_trule_is_template(const struct schema_Netfilter *conf)
{
	bool istemplate = false;
	const char *str = NULL;

	if (!conf) {
		LOGE("Is Netfilter template rule: invalid parameter");
		return false;
	}

	str = conf->rule;
	while ((str = strchr(str, TEMPLATE_VAR_CHAR))) {
		str++;
		if (*str == TEMPLATE_TAG_BEGIN) {
			if (strchr(str, TEMPLATE_TAG_END)) {
				istemplate = true;
				break;
			}
		} else if (*str == TEMPLATE_GROUP_BEGIN) {
			if (strchr(str, TEMPLATE_GROUP_END)) {
				istemplate = true;
				break;
			}
		}
	}

	LOGD("[%s] Netfilter rule is%s a template rule: %s", conf->name, istemplate ? "" : " not", conf->rule);
	return istemplate;
}

bool nfm_trule_modify(const struct schema_Netfilter *conf)
{
	bool errcode = true;

	if (!nfm_trule_is_conf_modified(conf)) {
		return true;
	}
	errcode = nfm_trule_del(conf);
	if (!errcode) {
		LOGE("[%s] Modify Netfilter template rule: delete template rule failed", conf->name);
		return false;
	}
	errcode = nfm_trule_new(conf);
	if (!errcode) {
		LOGE("[%s] Modify Netfilter template rule: new template rule failed", conf->name);
		return false;
	}
	return true;
}

