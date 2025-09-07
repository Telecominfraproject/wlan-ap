#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <time.h>
#include <ifaddrs.h>

#include <net/if.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <linux/if_ether.h>
#include <linux/rtnetlink.h>
#include <linux/nl80211.h>
#include <linux/limits.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <libubox/avl.h>
#include <libubox/avl-cmp.h>
#include <libubox/uloop.h>
#include <libubox/utils.h>
#include <libubox/ulog.h>
#include <libubox/blobmsg_json.h>
#include <libubox/vlist.h>

#include <libubus.h>

#define MAC_FMT	"%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_VAR(x) x[0], x[1], x[2], x[3], x[4], x[5]

#define IP_FMT	"%d.%d.%d.%d"
#define IP_VAR(x) x[0], x[1], x[2], x[3]

struct nl_socket {
	struct uloop_fd uloop;
	struct nl_sock *sock;
	int bufsize;
};

struct mac {
	uint8_t *addr;
	struct avl_node avl;
	char interface[64];
	char *ethers;

	struct timespec ts;
	struct list_head neigh4;
	struct list_head neigh6;
	struct list_head dhcpv4;
	struct list_head bridge_mac;
};

struct neigh {
	struct avl_node avl;
	struct list_head list;

	uint8_t *ip;
	int ip_ver;
	int iface;
	char ifname[IF_NAMESIZE];

	struct uloop_timeout ageing;
};

struct dhcpv4 {
	struct avl_node avl;
	struct list_head mac;

	uint8_t addr[ETH_ALEN];
	uint8_t ip[4];
	char iface[IF_NAMESIZE];
	char name[];
};

struct interface {
	struct avl_node avl;

	char *iface;
	char *device;
};

struct bridge_mac {
	struct vlist_node vlist;
	struct list_head mac;

	char bridge[IF_NAMESIZE];
	char ifname[IF_NAMESIZE];
	uint8_t addr[ETH_ALEN];
	__u8 port_no;
};

int avl_mac_cmp(const void *k1, const void *k2, void *ptr);

extern struct avl_tree mac_tree;
int mac_dump_all(void);
void mac_dump(struct mac *mac, int interface);
struct mac* mac_find(uint8_t *addr);
void mac_update(struct mac *mac, char *iface);

int neigh_init(void);
void neigh_enum(void);
void neigh_flush(void);
void neigh_done(void);

bool nl_status_socket(struct nl_socket *ev, int protocol,
		     int (*cb)(struct nl_msg *msg, void *arg), void *priv);
int genl_send_and_recv(struct nl_socket *ev, struct nl_msg * msg);

extern struct blob_buf b;
void blobmsg_add_iface(struct blob_buf *bbuf, char *name, int index);
void blobmsg_add_iftype(struct blob_buf *bbuf, const char *name, const uint32_t iftype);
void blobmsg_add_ipv4(struct blob_buf *bbuf, const char *name, const uint8_t* addr);
void blobmsg_add_ipv6(struct blob_buf *bbuf, const char *name, const uint8_t* addr);
void blobmsg_add_mac(struct blob_buf *bbuf, const char *name, const uint8_t* addr);

void ubus_init(void);
void ubus_uninit(void);

void bridge_init(void);
void bridge_dump_if(const char *bridge);
void bridge_flush(void);
void bridge_mac_del(struct bridge_mac *b);

void dhcpv4_ack(struct blob_attr *msg);
void dhcpv4_release(struct blob_attr *msg);
void dhcp_init(void);
void dhcp_done(void);
void dhcpv4_del(struct dhcpv4 *dhcpv4);

int interface_dump(void);
void interface_update(struct blob_attr *msg, int raw);
void interface_down(struct blob_attr *msg);
char *interface_resolve(char *device);
void interface_done(void);

void ethers_init(void);

void iface_done(void);
void iface_dump(int delta);
