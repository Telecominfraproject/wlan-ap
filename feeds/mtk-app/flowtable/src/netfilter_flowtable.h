#include <netinet/in.h>
#include <arpa/inet.h>
#include <libnfnetlink/libnfnetlink.h>

struct ftnl_handle {
	struct nfnl_handle		*nfnlh;
	struct nfnl_subsys_handle	*ftnlssh;
};

struct flow_tuple {
	struct in_addr sip4;
	struct in_addr dip4;
	unsigned char proto;
	unsigned short int sport;
	unsigned short int dport;
};

enum ft_msg_types {
	FT_MSG_DEL,
	FT_MSG_ADD,	//not support now
	FT_MSG_FLUSH,
	FT_MSG_MAX
};

enum ftattr_type {
	FTA_UNSPEC,
	FTA_TUPLE,
	__FTA_MAX
};
#define FTA_MAX (__FTA_MAX - 1)

enum ftattr_tuple {
	FTA_TUPLE_UNSPEC,
	FTA_TUPLE_IP,
	FTA_TUPLE_PROTO,
	FTA_TUPLE_ZONE,
	__FTA_TUPLE_MAX
};
#define FTA_TUPLE_MAX (__FTA_TUPLE_MAX - 1)

enum ftattr_ip {
	FTA_IP_UNSPEC,
	FTA_IP_V4_SRC,
	FTA_IP_V4_DST,
	FTA_IP_V6_SRC,
	FTA_IP_V6_DST,
	__FTA_IP_MAX
};
#define FTA_IP_MAX (__FTA_IP_MAX - 1)

enum ftattr_l4proto {
	FTA_PROTO_UNSPEC,
	FTA_PROTO_NUM,
	FTA_PROTO_SPORT,
	FTA_PROTO_DPORT,
	__FTA_PROTO_MAX
};
#define FTA_PROTO_MAX (__FTA_PROTO_MAX - 1)

struct ftnl_handle *ftnl_open(void);
void ftnl_close(struct ftnl_handle *h);
int ftnl_flush_table(struct ftnl_handle *h);
int ftnl_del_flow(struct ftnl_handle *h, struct flow_tuple *tuple);
