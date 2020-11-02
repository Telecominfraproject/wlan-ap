/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdlib.h>
#include "apc.h"

static void apc_disp(struct _timer * tmr);

static int apc_start(struct proto * P)
{
	struct apc_proto *p = (struct apc_proto *) P;
	struct apc_config *c = (struct apc_config *) (P->cf);
	
	p->router_id = proto_get_router_id(P->cf);
	p->stub_router = c->stub_router;
	p->tick = c->tick;
	p->disp_timer = tm_new_set( apc_disp, p, 0, p->tick );
	tm_start(p->disp_timer, 1);
	init_list(&(p->iface_list));
	apc_iface_new( );
	return PS_UP;
}

static void apc_dump( struct proto * P )
{
}

static struct proto * apc_init(struct proto_config * c)
{
	struct proto * P = mb_allocz(sizeof(struct apc_proto));
	
	P->cf = c;
	P->debug = c->debug;
	P->mrtdump = c->mrtdump;
	P->name = c->name;
	P->preference = c->preference;
	P->disabled = c->disabled;
	P->proto = c->protocol;
	//+P->table = c->table->table;
	P->hash_key = 12345;//+random_u32();
	c->proto = P;
	
	P->accept_ra_types = 1; //RA_OPTIMAL;
	
	return P;
}


/**
 * apc_disp - invokes routing table calculation, aging and also area_disp()
 * @timer: timer usually called every @apc_proto->tick second, @timer->data
 * point to @apc_proto
 */
static void apc_disp( struct _timer * tmr )
{
	struct apc_proto *p = tmr->data;
	
	/* Originate or flush local topology LSAs */
	//+apc_update_topology(p);
}


/**
 * apc_shutdown - Finish of APC instance
 * @P: APC protocol instance
 *
 * RFC does not define any action that should be taken before router
 * shutdown. To make my neighbors react as fast as possible, I send
 * them hello packet with empty neighbor list. They should start
 * their neighbor state machine with event %NEIGHBOR_1WAY.
 */
static int apc_shutdown( struct proto *P )
{
	return PS_DOWN;
}

static void
apc_get_status(struct proto *P, byte * buf)
{
	struct apc_proto *p = (struct apc_proto *) P;
	
	if (p->p.proto_state == PS_DOWN)
		buf[0] = 0;
	else
	{
		struct apc_iface *ifa;
		struct apc_neighbor *n;
		int adj = 0;
	
		WALK_LIST(ifa, p->iface_list)
		WALK_LIST(n, ifa->neigh_list) if (n->state == NEIGHBOR_FULL)
		adj = 1;
	
		if (adj == 0)
			strcpy(buf, "Alone");
		else
			strcpy(buf, "Running");
	}
}


/**
 * apc_reconfigure - reconfiguration hook
 * @P: current instance of protocol (with old configuration)
 * @c: new configuration requested by user
 *
 * This hook tries to be a little bit intelligent. Instance of APC
 * will survive change of many constants like hello interval,
 * password change, addition or deletion of some neighbor on
 * nonbroadcast network, cost of interface, etc.
 */
static int apc_reconfigure(struct proto *P, struct proto_config *c)
{
	return 1;
}



struct protocol proto_apc = {
	.name =		"APC",
	.template =	"apc%d",
	.attr_class =	3, //EAP_APC,
	.preference =	150, //DEF_PREF_APC,
	.config_size =	sizeof(struct apc_config),
	.init =		apc_init,
	.dump =		apc_dump,
	.start =	apc_start,
	.shutdown =	apc_shutdown,
	.reconfigure =	apc_reconfigure,
	.get_status =	apc_get_status,
};

