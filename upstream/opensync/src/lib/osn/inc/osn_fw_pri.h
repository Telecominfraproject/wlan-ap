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

#ifndef NM3_INCLUDE_OSN_FW_PRI
#define NM3_INCLUDE_OSN_FW_PRI

#include "osn_fw.h"
#include "ds_dlist.h"

#define OSFW_SIZE_CHAIN 64
#define OSFW_SIZE_MATCH 512
#define OSFW_SIZE_TARGET 128
#define OSFW_SIZE_CMD 512

#define OSFW_STR_UNKNOWN "osfw-unknown"

#define OSFW_STR_FAMILY_INET "inet"
#define OSFW_STR_FAMILY_INET6 "inet6"

#define OSFW_STR_CMD_IPTABLES_RESTORE "iptables-restore"
#define OSFW_STR_CMD_IP6TABLES_RESTORE "ip6tables-restore"

#define OSFW_STR_TABLE_FILTER "filter"
#define OSFW_STR_TABLE_NAT "nat"
#define OSFW_STR_TABLE_MANGLE "mangle"
#define OSFW_STR_TABLE_RAW "raw"
#define OSFW_STR_TABLE_SECURITY "security"

#define OSFW_STR_CHAIN_PREROUTING "PREROUTING"
#define OSFW_STR_CHAIN_INPUT "INPUT"
#define OSFW_STR_CHAIN_FORWARD "FORWARD"
#define OSFW_STR_CHAIN_OUTPUT "OUTPUT"
#define OSFW_STR_CHAIN_POSTROUTING "POSTROUTING"

#define OSFW_STR_TARGET_ACCEPT "ACCEPT"
#define OSFW_STR_TARGET_DROP "DROP"
#define OSFW_STR_TARGET_RETURN "RETURN"
#define OSFW_STR_TARGET_REJECT "REJECT"
#define OSFW_STR_TARGET_QUEUE "QUEUE"

struct osfw_nfchain {
	struct ds_dlist_node elt;
	struct ds_dlist *parent;
	char chain[OSFW_SIZE_CHAIN];
};

struct osfw_nfrule {
	struct ds_dlist_node elt;
	struct ds_dlist *parent;
	char chain[OSFW_SIZE_CHAIN];
	int prio;
	char match[OSFW_SIZE_MATCH];
	char target[OSFW_SIZE_TARGET];
};

struct osfw_nftable {
	int family;
	enum osfw_table table;
	bool issupported;
	bool isinitialized;
	struct ds_dlist chains;
	struct ds_dlist rules;
};

struct osfw_nfinet {
	int family;
	bool ismodified;
	struct {
		struct osfw_nftable filter;
		struct osfw_nftable nat;
		struct osfw_nftable mangle;
		struct osfw_nftable raw;
		struct osfw_nftable security;
	} tables;
};

struct osfw_nfbase {
	struct osfw_nfinet inet;
	struct osfw_nfinet inet6;
};

#endif

