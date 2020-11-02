/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_PROTOCOL_H_
#define _APC_PROTOCOL_H_

#include "lib/lists.h"
#include "lib/resource.h"
#include "lib/timer.h"

struct iface;
struct ifa;
struct neighbor;
struct network;
struct proto_config;
struct proto;
struct event;

/*
 *	Routing Protocol
 */

struct protocol {
  node n;
  char *name;
  char *template;			/* Template for automatic generation of names */
  int name_counter;			/* Counter for automatic name generation */
  int attr_class;			/* Attribute class known to this protocol */
  int multitable;			/* Protocol handles all announce hooks itself */
  uint preference;			/* Default protocol preference */
  uint config_size;			/* Size of protocol config */

  void (*postconfig)(struct proto_config *);			/* After configuring each instance */
  struct proto * (*init)(struct proto_config *);		/* Create new instance */
  int (*reconfigure)(struct proto *, struct proto_config *);	/* Try to reconfigure instance, returns success */
  void (*dump)(struct proto *);			/* Debugging dump */
  int (*start)(struct proto *);			/* Start the instance */
  int (*shutdown)(struct proto *);		/* Stop the instance */
  void (*cleanup)(struct proto *);		/* Called after shutdown when protocol became hungry/down */
  void (*get_status)(struct proto *, byte *buf); /* Get instance status (for `show protocols' command) */
  void (*show_proto_info)(struct proto *);	/* Show protocol info (for `show protocols all' command) */
  void (*copy_config)(struct proto_config *, struct proto_config *);	/* Copy config from given protocol instance */
};


/*
 *	Known protocols
 */

extern struct protocol proto_apc;

/*
 *	Routing Protocol Instance
 */

struct proto_config {
    node n;
  struct protocol *protocol;		/* Protocol */
  struct proto *proto;			/* Instance we've created */
  char *name;
  char *dsc;
  int class;				/* SYM_PROTO or SYM_TEMPLATE */
  u32 debug, mrtdump;			/* Debugging bitfields, both use D_* constants */
  unsigned preference, disabled;	/* Generic parameters */
  u32 router_id;			/* Protocol specific router ID */
  struct rtable_config *table;		/* Table we're attached to */
};


struct proto {
  node n;				/* Node in *_proto_list */
  node glob_node;			/* Node in global proto_list */
  struct protocol *proto;		/* Protocol */
  struct proto_config *cf;		/* Configuration data */
  struct proto_config *cf_new;		/* Configuration we want to switch to after shutdown (NULL=delete) */
  struct event *attn;			/* "Pay attention" event */

  char *name;				/* Name of this instance (== cf->name) */
  u32 debug;				/* Debugging flags */
  u32 mrtdump;				/* MRTDump flags */
  unsigned preference;			/* Default route preference */
  byte accept_ra_types;			/* Which types of route announcements are accepted (RA_OPTIMAL or RA_ANY) */
  byte disabled;			/* Manually disabled */
  byte proto_state;			/* Protocol state machine (PS_*, see below) */
  byte core_state;			/* Core state machine (FS_*, see below) */
  byte export_state;			/* Route export state (ES_*, see below) */
  byte reconfiguring;			/* We're shutting down due to reconfiguration */
  byte refeeding;			/* We are refeeding (valid only if export_state == ES_FEEDING) */
  byte flushing;			/* Protocol is flushed in current flush loop round */
  byte gr_recovery;			/* Protocol should participate in graceful restart recovery */
  byte gr_lock;				/* Graceful restart mechanism should wait for this proto */
  byte down_sched;			/* Shutdown is scheduled for later (PDS_*) */
  byte down_code;			/* Reason for shutdown (PDC_* codes) */
  u32 hash_key;				/* Random key used for hashing of neighbors */

  struct rtable *table;			/* Our primary routing table */
};

struct proto_spec {
  void *ptr;
  int patt;
};


void proto_copy_config(struct proto_config *dest, struct proto_config *src);
void proto_request_feeding(struct proto *p);

static inline void
proto_copy_rest(struct proto_config *dest, struct proto_config *src, unsigned size)
{ memcpy(dest + 1, src + 1, size - sizeof(struct proto_config)); }

void proto_show_basic_info(struct proto *p);

static inline u32
proto_get_router_id(struct proto_config *pc)
{
  return pc->router_id;
}

extern list active_proto_list;

/*
 *  Each protocol instance runs two different state machines:
 *
 *  [P] The protocol machine: (implemented inside protocol)
 *
 *		DOWN    ---->    START
 *		  ^		   |
 *		  |		   V
 *		STOP    <----     UP
 *
 *	States:	DOWN	Protocol is down and it's waiting for the core
 *			requesting protocol start.
 *		START	Protocol is waiting for connection with the rest
 *			of the network and it's not willing to accept
 *			packets. When it connects, it goes to UP state.
 *		UP	Protocol is up and running. When the network
 *			connection breaks down or the core requests
 *			protocol to be terminated, it goes to STOP state.
 *		STOP	Protocol is disconnecting from the network.
 *			After it disconnects, it returns to DOWN state.
 *
 *	In:	start()	Called in DOWN state to request protocol startup.
 *			Returns new state: either UP or START (in this
 *			case, the protocol will notify the core when it
 *			finally comes UP).
 *		stop()	Called in START, UP or STOP state to request
 *			protocol shutdown. Returns new state: either
 *			DOWN or STOP (in this case, the protocol will
 *			notify the core when it finally comes DOWN).
 *
 *	Out:	proto_notify_state() -- called by protocol instance when
 *			it does any state transition not covered by
 *			return values of start() and stop(). This includes
 *			START->UP (delayed protocol startup), UP->STOP
 *			(spontaneous shutdown) and STOP->DOWN (delayed
 *			shutdown).
 */

#define PS_DOWN 0
#define PS_START 1
#define PS_UP 2
#define PS_STOP 3

void proto_notify_state(struct proto *p, unsigned state);

/*
 *  [F] The feeder machine: (implemented in core routines)
 *
 *		HUNGRY    ---->   FEEDING
 *		 ^		     |
 *		 | 		     V
 *		FLUSHING  <----   HAPPY
 *
 *	States:	HUNGRY	Protocol either administratively down (i.e.,
 *			disabled by the user) or temporarily down
 *			(i.e., [P] is not UP)
 *		FEEDING	The protocol came up and we're feeding it
 *			initial routes. [P] is UP.
 *		HAPPY	The protocol is up and it's receiving normal
 *			routing updates. [P] is UP.
 *		FLUSHING The protocol is down and we're removing its
 *			routes from the table. [P] is STOP or DOWN.
 *
 *	Normal lifecycle of a protocol looks like:
 *
 *		HUNGRY/DOWN --> HUNGRY/START --> HUNGRY/UP -->
 *		FEEDING/UP --> HAPPY/UP --> FLUSHING/STOP|DOWN -->
 *		HUNGRY/STOP|DOWN --> HUNGRY/DOWN
 *
 *	Sometimes, protocol might switch from HAPPY/UP to FEEDING/UP
 *	if it wants to refeed the routes (for example BGP does so
 *	as a result of received ROUTE-REFRESH request).
 */

/*
 *	Debugging flags
 */

#define D_STATES 1		/* [core] State transitions */
#define D_ROUTES 2		/* [core] Routes passed by the filters */
#define D_FILTERS 4		/* [core] Routes rejected by the filters */
#define D_IFACES 8		/* [core] Interface events */
#define D_EVENTS 16		/* Protocol events */
#define D_PACKETS 32		/* Packets sent/received */

#ifndef PARSER
#define TRACE(flags, msg, args...) \
  do { if (p->p.debug & flags) log(L_TRACE "%s: " msg, p->p.name , ## args ); } while(0)
#endif

#endif
