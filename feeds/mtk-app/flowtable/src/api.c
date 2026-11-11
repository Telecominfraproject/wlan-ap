#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netfilter_flowtable.h"

#ifdef ATTR_DUMP
static void attr_dump(struct nfattr *attr)
{
	char *data = nla_data(attr);
	int i = 0;

	while (i < nal_len(attr)) {
		printf("%x ", *(data + i));
		i++;
		if (i % 16 == 0)
			printf("\n");
	}
	printf("\n");
}
#endif

struct ftnl_handle *ftnl_open(void)
{
	struct ftnl_handle *h = NULL;

	h = malloc(sizeof(struct ftnl_handle));
	if (!h)
		return NULL;

	h->nfnlh = nfnl_open();
	if (!h->nfnlh) {
		printf("nfnl open fail\n");
		free(h);
		return NULL;
	}

	h->ftnlssh = nfnl_subsys_open(h->nfnlh, NFNL_SUBSYS_FLOWTABLE, 1, 0);
	if (!h->ftnlssh) {
		nfnl_close(h->nfnlh);
		printf("subsys open fail\n");
		free(h);
		return NULL;
	}

	return h;
}

void ftnl_close(struct ftnl_handle *h)
{
	nfnl_subsys_close(h->ftnlssh);
	nfnl_close(h->nfnlh);
	free(h);
}

static void build_tuple(struct nlmsghdr *nlh, size_t size,
			struct flow_tuple *tuple)
{
	struct nfattr *nest_tuple, *nest_ip, *nest_proto;

	nest_tuple = nfnl_nest(nlh, size, FTA_TUPLE);

	nest_ip = nfnl_nest(nlh, size, FTA_TUPLE_IP);
	nfnl_addattr_l(nlh, size, FTA_IP_V4_SRC,
		       &tuple->sip4, sizeof(uint32_t));
	nfnl_addattr_l(nlh, size, FTA_IP_V4_DST,
		       &tuple->dip4, sizeof(uint32_t));
	nfnl_nest_end(nlh, nest_ip);

	nest_proto = nfnl_nest(nlh, size, FTA_TUPLE_PROTO);
	nfnl_addattr_l(nlh, size, FTA_PROTO_NUM,
		       &tuple->proto, sizeof(uint8_t));
	nfnl_addattr_l(nlh, size, FTA_PROTO_SPORT,
		       &tuple->sport, sizeof(uint16_t));
	nfnl_addattr_l(nlh, size, FTA_PROTO_DPORT,
		       &tuple->dport, sizeof(uint16_t));
	nfnl_nest_end(nlh, nest_proto);

	nfnl_nest_end(nlh, nest_tuple);
#ifdef ATTR_DUMP
	attr_dump(nest_tuple);
#endif
}

int ftnl_flush_table(struct ftnl_handle *h)
{
	union {
		char buffer[NFNL_HEADER_LEN];
		struct nlmsghdr nlh;
	} u;
	int ret;

	/* construct msg */
	nfnl_fill_hdr(h->ftnlssh, &u.nlh, 0, AF_INET, 0,
		      FT_MSG_FLUSH, NLM_F_REQUEST | NLM_F_ACK);

	/* send msg */
	ret = nfnl_send(h->nfnlh, &u.nlh);
	return ret;
}

int ftnl_del_flow(struct ftnl_handle *h, struct flow_tuple *tuple)
{
	const int size = 256;
	union {
		char buffer[size];
		struct nlmsghdr nlh;
	} u;
	int ret;

	/* construct msg */
	nfnl_fill_hdr(h->ftnlssh, &u.nlh, 0, AF_INET, 0,
		      FT_MSG_DEL, NLM_F_REQUEST|NLM_F_ACK);
	build_tuple(&u.nlh, size, tuple);

	/* send msg */
	ret = nfnl_send(h->nfnlh, &u.nlh);

	return ret;
}
