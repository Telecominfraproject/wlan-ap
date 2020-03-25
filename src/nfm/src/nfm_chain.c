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
#include "nfm_chain.h"
#include "nfm_osfw.h"
#include <string.h>
#include <stdio.h>

#define MODULE_ID LOG_MODULE_ID_MAIN

static struct ds_dlist nfm_chain_list = DS_DLIST_INIT(struct nfm_chain, elt);

static bool nfm_chain_set(struct nfm_chain *self, int family, const char *table, const char *chain)
{
	bool errcode = true;

	memset(self, 0, sizeof(*self));
	ds_dlist_insert_tail(&nfm_chain_list, self);

	if (((family != AF_INET) && (family != AF_INET6)) || !table || !table[0] || !chain || !chain[0]) {
		LOGE("Set Netfilter chain: invalid parameters(%d %s %s)", family, table, chain);
		return false;
	}
	self->family = family;
	strncpy(self->table, table, sizeof(self->table) - 1);
	self->table[sizeof(self->table) - 1] = '\0';
	strncpy(self->chain, chain, sizeof(self->chain) - 1);
	self->chain[sizeof(self->chain) - 1] = '\0';

	errcode = nfm_osfw_add_chain(self->family, self->table, self->chain);
	if (!errcode) {
		LOGE("Set Netfilter chain: SDK layer failed");
		return false;
	}
	self->isadded = true;
	return true;
}

static bool nfm_chain_unset(struct nfm_chain *self)
{
	bool errcode = true;

	if (self->isadded) {
		errcode = nfm_osfw_del_chain(self->family, self->table, self->chain);
		if (!errcode) {
			LOGE("Unset Netfilter chain: SDK layer failed");
			return false;
		}
		self->isadded = false;
	}

	ds_dlist_remove(&nfm_chain_list, self);
	memset(self, 0, sizeof(*self));
	return true;
}

static bool nfm_chain_destroy(struct nfm_chain *self)
{
	bool errcode = true;

	errcode = nfm_chain_unset(self);
	if (!errcode) {
		LOGE("Destroy Netfilter chain: unset chain failed");
		return false;
	}

	free(self);
	return true;
}

static struct nfm_chain *nfm_chain_create(int family, const char *table, const char *chain)
{
	struct nfm_chain *self = NULL;
	bool errcode = true;

	self = malloc(sizeof(*self));
	if (!self) {
		LOGE("Create Netfilter chain: memory allocation failed");
		return NULL;
	}

	errcode = nfm_chain_set(self, family, table, chain);
	if (!errcode) {
		LOGE("Create Netfilter chain: set chain failed");
		nfm_chain_destroy(self);
		return NULL;
	}
	return self;
}

static struct nfm_chain *nfm_chain_get(int family, const char *table, const char *chain)
{
	struct nfm_chain *self = NULL;

	if (!table || !chain) {
		LOGE("Get Netfilter chain: invalid parameters");
		return NULL;
	}

	ds_dlist_foreach(&nfm_chain_list, self) {
		if ((family == self->family) &&
				!strncmp(table, self->table, sizeof(self->table)) &&
				!strncmp(chain, self->chain, sizeof(self->chain))) {
			return self;
		}
	}
	return NULL;
}

bool nfm_chain_init(void)
{
	return true;
}

bool nfm_chain_get_ref(int family, const char *table, const char *chain)
{
	struct nfm_chain *self = NULL;

	self = nfm_chain_get(family, table, chain);
	if (!self) {
		self = nfm_chain_create(family, table, chain);
		if (!self) {
			LOGE("Get reference on chain: create %s chain %s for table %s failed",
					(family == AF_INET) ? "IPv4" : "IPv6", chain, table);
			return false;
		}
		LOGD("Netfilter %s chain %s is created for table %s",
				(family == AF_INET) ? "IPv4" : "IPv6", chain, table);
	}
	self->nbref++;
	return true;
}

bool nfm_chain_put_ref(int family, const char *table, const char *chain)
{
	struct nfm_chain *self = NULL;
	bool errcode = true;

	self = nfm_chain_get(family, table, chain);
	if (!self) {
		LOGE("Put reference on chain: cannot get %s chain %s for table %s",
				(family == AF_INET) ? "IPv4" : "IPv6", chain, table);
		return false;
	}
	self->nbref--;

	if (!self->nbref) {
		errcode = nfm_chain_destroy(self);
		if (!errcode) {
			LOGE("Put reference on chain: destroy %s chain %s for table %s failed",
					(family == AF_INET) ? "IPv4" : "IPv6", chain, table);
			return false;
		}
		LOGD("Netfilter %s chain %s is destroyed for table %s",
				(family == AF_INET) ? "IPv4" : "IPv6", chain, table);
	}
	return true;
}

