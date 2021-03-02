/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_APC_H_
#define _APC_APC_H_

#include <stdio.h>
#include <string.h>

#include "nest/apcn.h"

#include "lib/ip.h"
#include "lib/lists.h"
#include "lib/timer.h"
#include "lib/resource.h"
#include "protocol.h"
#include "nest/iface.h"

#ifdef LOCAL_DEBUG
#define APC_FORCE_DEBUG 1
#else
#define APC_FORCE_DEBUG 0
#endif


#define APC_IS_V2 1

#define APC_PROTO 89

#define LSREFRESHTIME 1800	/* 30 minutes */
#define MINLSINTERVAL 5
#define MINLSARRIVAL 1
#define LSINFINITY 0xffffff

#define APC_DEFAULT_TICK 1
#define APC_DEFAULT_STUB_COST 1000
#define APC_DEFAULT_ECMP_LIMIT 16
#define APC_DEFAULT_TRANSINT 40

#define APC_MIN_PKT_SIZE 256
#define APC_MAX_PKT_SIZE 65535

#define APC_VLINK_ID_OFFSET 0x80000000


struct apc_config
{
	struct proto_config c;
	uint tick;
	u8 stub_router;
	u8 instance_id;
	u8 abr;
	list vlink_list;		/* list of configured vlinks (struct apc_iface_patt) */
};

struct apc_area_config
{
	node n;
	u32 areaid;
	u32 default_cost;		/* Cost of default route for stub areas
	      			   (With possible LSA_EXT3_EBIT for NSSA areas) */
	u8 type;		/* Area type (standard, stub, NSSA), represented
	      			   by option flags (OPT_E, OPT_N) */
	u8 summary;			/* Import summaries to this stub/NSSA area, valid for ABR */
	u8 default_nssa;		/* Generate default NSSA route for NSSA+summary area */
	u8 translator;		/* Translator role, for NSSA ABR */
	u32 transint;			/* Translator stability interval */
	list patt_list;		/* List of iface configs (struct apc_iface_patt) */
	list net_list;		/* List of aggregate networks for that area */
	list enet_list;		/* List of aggregate external (NSSA) networks */
	list stubnet_list;	/* List of stub networks added to Router LSA */
};

struct area_net_config
{
	node n;
	struct prefix px;
	u32 tag;
	u8 hidden;
};

struct area_net
{
	u32 metric;	/* With possible LSA_EXT3_EBIT for NSSA area nets */
	u32 tag;
	u8 hidden;
	u8 active;
};

struct apc_stubnet_config
{
	node n;
	struct prefix px;
	u32 cost;
	u8 hidden;
	u8 summary;
};

struct nbma_node
{
	node n;
	ip_addr ip;
	byte eligible;
	byte found;
};

struct apc_iface_patt
{
	struct iface_patt i;
	u32 type;
	u32 stub;
	u32 cost;
	u32 helloint;
	u32 rxmtint;
	u32 pollint;
	u32 waitint;
	u32 deadc;
	u32 deadint;
	u32 priority;
	u32 voa;
	u32 vid;
	int tx_tos;
	int tx_priority;
	u16 tx_length;
	u16 rx_buffer;

#define APC_RXBUF_MINSIZE 256	/* Minimal allowed size */
	u8 instance_id;
	u8 strictnbma;
	u8 check_link;
	u8 ecmp_weight;
	u8 real_bcast;		/* Not really used in APCv3 */
	u8 ttl_security;		/* bool + 2 for TX only */
	u8 bfd;
	u8 bsd_secondary;
};

/* Default values for interface parameters */
#define COST_D 10
#define RXMTINT_D 5
#define INFTRANSDELAY_D 1
#define PRIORITY_D 1
#define HELLOINT_D 10
#define POLLINT_D 20
#define DEADC_D 4
#define WAIT_DMH 4
  /* Value of Wait timer - not found it in RFC * - using 4*HELLO */


struct apc_proto
{
	struct proto p;
	timer * disp_timer; /* APC proto dispatcher */
	uint tick;
	list iface_list;    /* List of APC interfaces (struct apc_iface) */
	int padj;           /* Number of neighbors in Exchange or Loading state */
	byte stub_router;   /* Do not forward transit traffic */
	u32 router_id;
};


struct apc_iface
{
	node n;
	struct iface *iface;		/* Nest's iface (NULL for vlinks) */
	struct ifa *addr;		/* IP prefix associated with that APC iface */
	struct apc_iface_patt *cf;
	char *ifname;			/* Interface name (iface->name), new one for vlinks */
	
	list neigh_list;		/* List of neighbors (struct apc_neighbor) */
	u32 cost;			/* Cost of iface */
	u32 waitint;			/* number of sec before changing state from wait */
	u32 rxmtint;			/* number of seconds between LSA retransmissions */
	u32 pollint;			/* Poll interval */
	u32 deadint;			/* after "deadint" missing hellos is router dead */
	u32 iface_id;			/* Interface ID (iface->index or new value for vlinks) */
	u32 vid;			/* ID of peer of virtual link */
	ip_addr vip;			/* IP of peer of virtual link */
	struct apc_iface *vifa;	/* APC iface which the vlink goes through */
	u16 helloint;			/* number of seconds between hello sending */
	u32 csn;                      /* Last used crypt seq number */
	time_t csn_use;         /* Last time when packet with that CSN was sent */
	ip_addr all_routers;		/* Multicast (or broadcast) address for all routers */
	ip_addr des_routers;		/* Multicast (or NULL) address for designated routers */
	ip_addr drip;			/* Designated router IP */
	ip_addr bdrip;		/* Backup DR IP */
	u32 drid;			/* DR Router ID */
	u32 bdrid;			/* BDR Router ID */
	u32 dr_iface_id;		/* if drid is valid, this is iface_id of DR (for connecting network) */
	u8 instance_id;		/* Used to differentiate between more APC
	      			   instances on one interface */
	u8 type;			/* APC view of type (APC_IT_*) */
	u8 strictnbma;		/* Can I talk with unknown neighbors? */
	u8 stub;			/* Inactive interface */
	u8 state;			/* Interface state machine (APC_IS_*) */
	timer *wait_timer;		/* WAIT timer */
	timer *hello_timer;		/* HELLOINT timer */
	timer *poll_timer;		/* Poll Interval - for NBMA */
	
	int fadj;			/* Number of fully adjacent neighbors */
	u8 priority;			/* A router priority for DR election */
	u8 ioprob;
#define APC_I_OK 0		/* Everything OK */
#define APC_I_SK 1		/* Socket open failed */
#define APC_I_LL 2		/* Missing link-local address (APCv3) */
	u8 sk_dr;			/* Socket is a member of designated routers group */
	u8 marked;			/* Used in APC reconfigure, 2 for force restart */
	u16 rxbuf;			/* Buffer size */
	u16 tx_length;		/* Soft TX packet length limit, usually MTU */
	u8 check_link;		/* Whether iface link change is used */
	u8 ecmp_weight;		/* Weight used for ECMP */
	u8 check_ttl;			/* Check incoming packets for TTL 255 */
	u8 bfd;			/* Use BFD on iface */
};

struct apc_neighbor
{
	node n;
	struct apc_iface *ifa;
	u8 state;
	timer *inactim;		/* Inactivity timer */
	u8 imms;			/* I, M, Master/slave received */
	u8 myimms;			/* I, M Master/slave */
	u32 dds;			/* DD Sequence number being sent */
	u32 ddr;			/* last Dat Des packet received */
	
	u32 rid;			/* Router ID */
	ip_addr ip;			/* IP of it's interface */
	u8 priority;			/* Priority */
	u8 adj;			/* built adjacency? */
	u32 options;			/* Options received */

	/* Entries dr and bdr store IP addresses in APCv2 and router IDs in
	APCv3, we use the same type to simplify handling */
	u32 dr;			/* Neighbor's idea of DR */
	u32 bdr;		/* Neighbor's idea of BDR */
	u32 iface_id;		/* ID of Neighbour's iface connected to common network */

	list ackl[2];
#define ACKL_DIRECT 0
#define ACKL_DELAY 1
	timer *ackd_timer;		/* Delayed ack timer */
	struct bfd_request *bfd_req;	/* BFD request, if BFD is used */
	void *ldd_buffer;		/* Last database description packet */
	u32 ldd_bsize;		/* Buffer size for ldd_buffer */
	u32 csn;                /* Last received crypt seq number (for MD5) */
	u8 basic_mac[6];
};


/* APC interface types */
#define APC_IT_BCAST   0
#define APC_IT_NBMA    1
#define APC_IT_PTP     2
#define APC_IT_PTMP    3
#define APC_IT_VLINK   4
#define APC_IT_UNDEF   5

/* APC interface states */
#define APC_IS_DOWN    0   /* Not active */
#define APC_IS_LOOP    1   /* Iface with no link */
#define APC_IS_WAITING 2   /* Waiting for Wait timer */
#define APC_IS_PTP     3   /* PTP operational */
#define APC_IS_DROTHER 4   /* I'm on BCAST or NBMA and I'm not DR */
#define APC_IS_BACKUP  5   /* I'm BDR */
#define APC_IS_DR      6   /* I'm DR */
#define APC_MAX_MODE   7


/* Definitions for interface state machine */
#define ISM_UP      0   /* Interface Up */
#define ISM_WAITF   1   /* Wait timer fired */
#define ISM_BACKS   2   /* Backup seen */
#define ISM_NEICH   3   /* Neighbor change */
#define ISM_LOOP    4   /* Link down */
#define ISM_UNLOOP  5   /* Link up */
#define ISM_DOWN    6   /* Interface down */


/* APC neighbor states */
#define NEIGHBOR_DOWN       0
#define NEIGHBOR_ATTEMPT    1
#define NEIGHBOR_INIT       2
#define NEIGHBOR_2WAY       3
#define NEIGHBOR_EXSTART    4
#define NEIGHBOR_EXCHANGE   5
#define NEIGHBOR_LOADING    6
#define NEIGHBOR_FULL       7

/* Definitions for neighbor state machine */
#define INM_HELLOREC	0	/* Hello Received */
#define INM_START	1	/* Neighbor start - for NBMA */
#define INM_2WAYREC	2	/* 2-Way received */
#define INM_NEGDONE	3	/* Negotiation done */
#define INM_EXDONE	4	/* Exchange done */
#define INM_BADLSREQ	5	/* Bad LS Request */
#define INM_LOADDONE	6	/* Load done */
#define INM_ADJOK	7	/* AdjOK? */
#define INM_SEQMIS	8	/* Sequence number mismatch */
#define INM_1WAYREC	9	/* 1-Way */
#define INM_KILLNBR	10	/* Kill Neighbor */
#define INM_INACTTIM	11	/* Inactivity timer */
#define INM_LLDOWN	12	/* Line down */

#define TRANS_OFF	0
#define TRANS_ON	1
#define TRANS_WAIT	2	/* Waiting before the end of translation */

#define DBDES_I		4	/* Init bit */
#define DBDES_M		2	/* More bit */
#define DBDES_MS	1	/* Master/Slave bit */
#define DBDES_IMMS	(DBDES_I | DBDES_M | DBDES_MS)

/* apc.c */
#define apc_is_v2(X) APC_IS_V2

/* iface.c */
void apc_iface_chstate(struct apc_iface *ifa, u8 state);
void apc_iface_sm(struct apc_iface *ifa, int event);
void apc_iface_new(void);

/* neighbor.c */
struct apc_neighbor *apc_neighbor_new(struct apc_iface *ifa);
void apc_neigh_sm(struct apc_neighbor *n, int event);
void apc_dr_election(struct apc_iface *ifa);
struct apc_neighbor *find_neigh_by_ip(struct apc_iface *ifa, ip_addr ip);

#ifndef PARSER
#define DROP(DSC,VAL) do { err_dsc = DSC; err_val = VAL; goto drop; } while(0)
#endif

/* hello.c */
#define OHS_HELLO    0
#define OHS_POLL     1
#define OHS_SHUTDOWN 2

void apc_send_hello( struct apc_iface *ifa, int kind );
void apc_receive_hello( unsigned char *pkt, struct apc_iface *ifa, struct apc_neighbor *n, ip_addr faddr, unsigned int plen );

/* apc_main.c */
void remove_previous_radius( void );
unsigned int set_radius_proxy( int n_peer, u32 * PeerList );

/*build_dir/target-arm_cortex-a7+neon-vfpv4_musl_eabi/libwebsocket/include/websocke*/
struct apc_neigh
{
	unsigned int Ip;
	unsigned char BasicMac[6];
} __attribute__((packed));

#define IAC_APC_ELECTION_PORT 50010
#define APC_UNKNOWN     0
#define I_AM_APC        1
#define I_AM_BAPC       2
#define IAC_HEADER_SIZE             22
#define APC_MAX_NEIGHBORS       100


struct apc_spec
{
	unsigned int IsApc;
	unsigned int Changed;
	unsigned int FloatIp;
	struct apc_neigh Neighbors[APC_MAX_NEIGHBORS];
} __attribute__((packed));


#endif /* _APC_APC_H_ */
