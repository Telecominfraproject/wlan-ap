/* SPDX-License-Identifier: BSD-3-Clause */
#include <apc.h>

const char *apc_ns_names[] = {
	"Down", "Attempt", "Init", "2-Way", "ExStart", "Exchange", "Loading",
	"Full"
};

const char *apc_inm_names[] = {
	"HelloReceived", "Start", "2-WayReceived", "NegotiationDone",
	"ExchangeDone",	"BadLSReq", "LoadingDone", "AdjOK?",
	"SeqNumberMismatch", "1-WayReceived",
	"KillNbr", "InactivityTimer", "LLDown"
};


static int can_do_adj(struct apc_neighbor *n);
static void inactivity_timer_hook(struct _timer * tmr);

extern struct apc_iface * apc_ifa;
extern struct apc_proto * ApcProto;
extern unsigned int MyIpAddr;
extern unsigned int WaitingToReelect;

unsigned int BackingUpRadius = 0;

/* Resets LSA request and retransmit lists.
 * We do not reset DB summary list iterator here,
 * it is reset during entering EXCHANGE state.
 */
static void
reset_lists(struct apc_proto *p, struct apc_neighbor *n)
{
}

struct apc_neighbor * apc_neighbor_new(struct apc_iface * ifa)
{
	struct apc_neighbor * n = mb_allocz(sizeof(struct apc_neighbor));
	printf("apc_new_neighbor\n");	
	n->ifa = ifa;
	add_tail(&ifa->neigh_list, NODE n);
	n->adj = 0;
	n->csn = 0;
	n->state = NEIGHBOR_DOWN;
	
	init_list(&n->ackl[ACKL_DIRECT]);
	init_list(&n->ackl[ACKL_DELAY]);
	
	n->inactim = tm_new_set(inactivity_timer_hook, n, 0, 0);
	
	return(n);
}

static void apc_neigh_down(struct apc_neighbor * n)
{
	struct apc_iface * ifa = n->ifa;
	
	rem_node(NODE n);
	
	printf("Neighbor %x on %s removed", n->rid, ifa->ifname );
	tm_free(n->inactim);
	mb_free(n);
}

/**
 * apc_neigh_chstate - handles changes related to new or lod state of neighbor
 * @n: APC neighbor
 * @state: new state
 *
 * Many actions have to be taken acording to a change of state of a neighbor. It
 * starts rxmt timers, call interface state machine etc.
 */
static void apc_neigh_chstate(struct apc_neighbor * n, u8 state)
{
	struct apc_iface * ifa = n->ifa;
	struct apc_proto * p = ApcProto;
	u8 old_state = n->state;
	
	if (state == old_state)
		return;
	
	printf("Neighbor %x on %s changed state from %s to %s\n",
	                n->rid, ifa->ifname, apc_ns_names[old_state],
			apc_ns_names[state]);
	
	n->state = state;

	/* Increase number of partial adjacencies */
	if ((state == NEIGHBOR_EXCHANGE) || (state == NEIGHBOR_LOADING))
		p->padj++;
	
	/* Decrease number of partial adjacencies */
	if ((old_state == NEIGHBOR_EXCHANGE) || (old_state == NEIGHBOR_LOADING))
		p->padj--;
	
	/* Increase number of full adjacencies */
	if (state == NEIGHBOR_FULL)
		ifa->fadj++;
	
	/* Decrease number of full adjacencies */
	if (old_state == NEIGHBOR_FULL)
		ifa->fadj--;

	if (state == NEIGHBOR_EXSTART)
	{
		/* First time adjacency */
		if (n->adj == 0)
			n->dds = 111;//+random_u32();
		
		n->dds++;
		n->myimms = DBDES_IMMS;
	}

	if (state > NEIGHBOR_EXSTART)
		n->myimms &= ~DBDES_I;
	
	/* Generate NeighborChange event if needed, see RFC 2328 9.2 */
	if ((state == NEIGHBOR_2WAY) && (old_state < NEIGHBOR_2WAY))
		apc_iface_sm( ifa, ISM_NEICH );
	if ((state < NEIGHBOR_2WAY) && (old_state >= NEIGHBOR_2WAY))
		apc_iface_sm(ifa, ISM_NEICH);
}

/**
 * apc_neigh_sm - apc neighbor state machine
 * @n: neighor
 * @event: actual event
 *
 * This part implements the neighbor state machine as described in 10.3 of
 * RFC 2328. The only difference is that state %NEIGHBOR_ATTEMPT is not
 * used. We discover neighbors on nonbroadcast networks in the
 * same way as on broadcast networks. The only difference is in
 * sending hello packets. These are sent to IPs listed in
 * @apc_iface->nbma_list .
 */
void apc_neigh_sm(struct apc_neighbor * n, int event)
{
	struct apc_proto * p = ApcProto;

	printf("Neighbor state machine for %x on %s, event %s\n",
		n->rid, n->ifa->ifname, apc_inm_names[event]);

	switch(event)
	{
	case INM_START:
		apc_neigh_chstate(n, NEIGHBOR_ATTEMPT);
		/* NBMA are used different way */
		break;

	case INM_HELLOREC:
		if(n->state < NEIGHBOR_INIT)
			apc_neigh_chstate(n, NEIGHBOR_INIT);
		/* Restart inactivity timer */
		tm_start(n->inactim, n->ifa->deadint);
		break;

	case INM_2WAYREC:
		if(n->state < NEIGHBOR_2WAY)
			apc_neigh_chstate(n, NEIGHBOR_2WAY);
		if((n->state == NEIGHBOR_2WAY) && can_do_adj(n))
			apc_neigh_chstate(n, NEIGHBOR_EXSTART);
		break;

	case INM_NEGDONE:
		if(n->state == NEIGHBOR_EXSTART)
			apc_neigh_chstate(n, NEIGHBOR_EXCHANGE);
		else
			printf("NEGDONE and I'm not in EXSTART?\n" );
		break;

	case INM_EXDONE:
		apc_neigh_chstate(n, NEIGHBOR_FULL);
		break;

	case INM_LOADDONE:
		apc_neigh_chstate(n, NEIGHBOR_FULL);
		break;

	case INM_ADJOK:
		/* Can In build adjacency? */
		if ((n->state == NEIGHBOR_2WAY) && can_do_adj(n))
		{
			apc_neigh_chstate(n, NEIGHBOR_EXSTART);
		}
		else if ((n->state >= NEIGHBOR_EXSTART) && !can_do_adj(n))
		{
			reset_lists(p, n);
			apc_neigh_chstate(n, NEIGHBOR_2WAY);
		}
		break;

	case INM_SEQMIS:
	case INM_BADLSREQ:
		if (n->state >= NEIGHBOR_EXCHANGE)
		{
			reset_lists(p, n);
			apc_neigh_chstate(n, NEIGHBOR_EXSTART);
		}
		break;
	
	case INM_KILLNBR:
	case INM_LLDOWN:
	case INM_INACTTIM:
		/* No need for reset_lists() */
		apc_neigh_chstate(n, NEIGHBOR_DOWN);
		apc_neigh_down(n);
		break;

	case INM_1WAYREC:
		reset_lists(p, n);
		apc_neigh_chstate(n, NEIGHBOR_INIT);
		break;
	
	default:
		printf("%s: INM - Unknown event?\n", p->p.name);
		break;
	}
}

static int can_do_adj(struct apc_neighbor * n)
{
	struct apc_iface * ifa = n->ifa;
	struct apc_proto * p = ApcProto;
	int i = 0;
	
	switch(ifa->state)
	{
	case APC_IS_DOWN:
	case APC_IS_LOOP:
		printf("%s: Iface %s in down state?", p->p.name, ifa->ifname);
		break;
	
	case APC_IS_WAITING:
		printf("%s: Neighbor? on iface %s\n", p->p.name, ifa->ifname);
		break;
	
	case APC_IS_DROTHER:
		if (((n->rid == ifa->drid) || (n->rid == ifa->bdrid))
			&& (n->state >= NEIGHBOR_2WAY))
			i = 1;
		break;
	
	case APC_IS_PTP:
	case APC_IS_BACKUP:
	case APC_IS_DR:
		if (n->state >= NEIGHBOR_2WAY)
			i = 1;
		break;
	
	default:
		printf("%s: Iface %s in unknown state?", p->p.name, ifa->ifname);
		break;
	}
	printf("%s: Iface %s can_do_adj=%d\n", p->p.name, ifa->ifname, i);
	return i;
}

static inline u32 neigh_get_id(struct apc_proto *p, struct apc_neighbor *n)
{
	return ipa_to_u32(n->ip);
}

static struct apc_neighbor * elect_bdr( struct apc_proto * p, list nl)
{
    struct apc_neighbor *neigh, *n1, *n2;
    u32 nid;

    n1 = NULL;
    n2 = NULL;
    WALK_LIST( neigh, nl )                /* First try those decl. themselves */
    {
        nid = neigh_get_id( p, neigh );

        if( neigh->state >= NEIGHBOR_2WAY )     /* Higher than 2WAY */
            if( neigh->priority > 0 )           /* Eligible */
                if( neigh->dr != nid )          /* And not decl. itself DR */
                {
                    if( neigh->bdr == nid )     /* Declaring BDR */
                    {
                        if( n1 != NULL )
                        {
                            if( neigh->priority > n1->priority )
                                n1 = neigh;
                            else
                            {
                                if( neigh->priority == n1->priority )
                                    if( neigh->rid > n1->rid )
                                        n1 = neigh;
                            }
                        }
                        else
                            n1 = neigh;
                    }
                    else                        /* And NOT declaring BDR */
                    {
                        if( n2 != NULL )
                        {
                            if( neigh->priority > n2->priority )
                                n2 = neigh;
                            else
                                if( neigh->priority == n2->priority )
                                    if( neigh->rid > n2->rid )
                                        n2 = neigh;
                        }
                        else
                            n2 = neigh;
                    }
                }
    }
    if( n1 == NULL )
        n1 = n2;

    return( n1 );
}

static struct apc_neighbor * elect_dr( struct apc_proto * p, list nl )
{
    struct apc_neighbor *neigh, *n;
    u32 nid;

    n = NULL;
    WALK_LIST( neigh, nl )                      /* And now DR */
    {
        nid = neigh_get_id( p, neigh );

        printf("neigh %p %x %x %x %d\n", n, neigh->dr, neigh->rid, neigh->ip, neigh->state );
        if( neigh->state >= NEIGHBOR_2WAY )     /* Higher than 2WAY */
            if( neigh->priority > 0 )           /* Eligible */
                if( neigh->dr == nid )          /* And declaring itself DR */
                {
                    printf("el_dr %p %x %x %x\n", n, neigh->dr, neigh->rid, neigh->ip );
                    if( n != NULL )
                    {
                        if( neigh->priority > n->priority )
                            n = neigh;
                        else
                            if( neigh->priority == n->priority )
                                if( neigh->rid > n->rid )
                                    n = neigh;
                    }
                    else
                        n = neigh;
                }
    }

    return( n );
}

/**
 * apc_dr_election - (Backup) Designed Router election
 * @ifa: actual interface
 *
 * When the wait timer fires, it is time to elect (Backup) Designated Router.
 * Structure describing me is added to this list so every electing router has
 * the same list. Backup Designated Router is elected before Designated
 * Router. This process is described in 9.4 of RFC 2328. The function is
 * supposed to be called only from apc_iface_sm() as a part of the interface
 * state machine.
 */
void apc_dr_election(struct apc_iface * ifa)
{
	struct apc_proto * p = ApcProto;
	struct apc_neighbor *neigh, *ndr, *nbdr, me;
	u32 myid = p->router_id;
	u32 old_drid;
	u32 old_bdrid;
	
	me.state = NEIGHBOR_2WAY;
	me.rid = myid;
	me.priority = ifa->priority;
	me.ip = MyIpAddr;
	
	me.dr  = ipa_to_u32( ifa->drip );
	me.bdr = ipa_to_u32( ifa->bdrip );
	me.iface_id = ifa->iface_id;
	
	add_tail( &ifa->neigh_list, NODE & me );
	
	nbdr = elect_bdr( p, ifa->neigh_list );
	ndr = elect_dr( p, ifa->neigh_list );
	
	printf("(B)DR election. %p %p\n", ndr, nbdr );

	if (ndr == NULL)
		ndr = nbdr;

	if (ndr)
		printf("elect %x %x %x\n", ndr->dr, ndr->rid, ndr->ip);
	else
		printf("elect no dr\n");

	if (ndr && nbdr)
		printf("(B)DR electionIP. %x %x\n", ndr->ip, nbdr->ip);

	/* 9.4. (4) */
	if (((ifa->drid == myid) && (ndr != &me))
		|| ((ifa->drid != myid) && (ndr == &me))
		|| ((ifa->bdrid == myid) && (nbdr != &me))
		|| ((ifa->bdrid != myid) && (nbdr == &me)))
	{
		me.dr = ndr ? neigh_get_id(p, ndr) : 0;
		me.bdr = nbdr ? neigh_get_id(p, nbdr) : 0;
		
		nbdr = elect_bdr(p, ifa->neigh_list);
		ndr = elect_dr(p, ifa->neigh_list);
	
		if (ndr == NULL)
			ndr = nbdr;
		if (ndr)
			printf("elect1 %x %x %x\n", ndr->dr, ndr->rid, ndr->ip);
		else
			printf("elect1 no dr\n");
	}

	rem_node( NODE & me );
	
	old_drid = ifa->drid;
	old_bdrid = ifa->bdrid;
	
	ifa->drid = ndr ? ndr->rid : 0;
	ifa->drip = ndr ? ndr->ip  : IPA_NONE;
	ifa->dr_iface_id = ndr ? ndr->iface_id : 0;
	
	ifa->bdrid = nbdr ? nbdr->rid : 0;
	ifa->bdrip = nbdr ? nbdr->ip  : IPA_NONE;
	
	printf("DR=%x, BDR=%x\n", ifa->drip, ifa->bdrip );

	/* We are part of the interface state machine */
	if (ifa->drid == myid)
		apc_iface_chstate(ifa, APC_IS_DR);
	else
		if (ifa->bdrid == myid)
			apc_iface_chstate(ifa, APC_IS_BACKUP);
		else
			apc_iface_chstate(ifa, APC_IS_DROTHER);

	/* Review neighbor adjacencies if DR or BDR changed */
	if ((ifa->drid != old_drid) || (ifa->bdrid != old_bdrid))
	{
		WALK_LIST(neigh, ifa->neigh_list)
		if (neigh->state >= NEIGHBOR_2WAY)
			apc_neigh_sm(neigh, INM_ADJOK);
	}
}

struct apc_neighbor * find_neigh_by_ip(struct apc_iface * ifa, ip_addr ip)
{
	struct apc_neighbor * n;

	WALK_LIST(n, ifa->neigh_list)
	{
		if (ipa_equal(n->ip, ip))
			return n;
	}
	return NULL;
}

static void inactivity_timer_hook(struct _timer * tmr)
{
	struct apc_neighbor * n = (struct apc_neighbor *) tmr->data;

	if (WaitingToReelect)
		return;

	if (n->rid == n->ifa->drip)
	{
		struct apc_spec ApcSpec;
		unsigned int Neigh[APC_MAX_NEIGHBORS];
		int n_neigh = 0;

		WALK_LIST(n, apc_ifa->neigh_list)
		{
			Neigh[n_neigh] = n->rid;
			n_neigh += 1;
			tm_stop(n->inactim);
			rem_node(NODE n);
		}
		if (MyIpAddr == apc_ifa->bdrip)
		{
			Neigh[n_neigh] = MyIpAddr;
			n_neigh += 1;
			//Radius stuff
			BackingUpRadius = 1;
			apc_ifa->priority = 0x33;
		}
		else
			apc_ifa->priority = 0x11;


		apc_ifa->drip = MyIpAddr;
		apc_ifa->bdrip = 0;
		memset(&ApcSpec, 0, sizeof(struct apc_spec));
		WaitingToReelect = 3;

		return;
	}
	printf("Inactivity timer expired for nbr %x on %s", n->rid, 
							    n->ifa->ifname);
	apc_neigh_sm(n, INM_INACTTIM);
}

