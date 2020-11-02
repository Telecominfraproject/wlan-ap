/* SPDX-License-Identifier: BSD-3-Clause */
#include <apc.h>

const char *apc_is_names[] = {
	"Down", "Loopback", "Waiting", "PtP", "DROther", "Backup", "DR"
};

const char *apc_ism_names[] = {
	"InterfaceUp", "WaitTimer", "BackupSeen", "NeighborChange",
	"LoopInd", "UnloopInd", "InterfaceDown"
};

char * IfName = "br-wan";

extern struct apc_iface * apc_ifa;
extern unsigned int MyIpAddr;


static void hello_timer_hook(struct _timer * tmr)
{
	apc_send_hello(tmr->data, OHS_HELLO);
}


static void wait_timer_hook(struct _timer * tmr)
{
	struct apc_iface * ifa = (struct apc_iface *)tmr->data;
	
	printf("Wait timer fired on %s", ifa->ifname);
	apc_iface_sm(ifa, ISM_WAITF);
}


static inline void
apc_sk_join_dr(struct apc_iface *ifa)
{
	if (ifa->sk_dr)
		return;
	
	ifa->sk_dr = 1;
}

static inline void
apc_sk_leave_dr(struct apc_iface *ifa)
{
	if (!ifa->sk_dr)
		return;
	
	ifa->sk_dr = 0;
}


/**
 * apc_iface_chstate - handle changes of interface state
 * @ifa: APC interface
 * @state: new state
 *
 * Many actions must be taken according to interface state changes. New network
 * LSAs must be originated, flushed, new multicast sockets to listen for messages for
 * %ALLDROUTERS have to be opened, etc.
 */
void apc_iface_chstate(struct apc_iface * ifa, u8 state)
{
	u8 oldstate = ifa->state;
	
	if (state == oldstate)
		return;
	
	printf("Interface %s changed state from %s to %s\n",
		ifa->ifname, apc_is_names[oldstate], apc_is_names[state]);
	
	ifa->state = state;

	if ((ifa->type == APC_IT_BCAST) && ipa_nonzero(ifa->des_routers))
	{
		if ((state == APC_IS_BACKUP) || (state == APC_IS_DR))
			apc_sk_join_dr(ifa);
		else
			apc_sk_leave_dr(ifa);
	}
}


/**
 * apc_iface_sm - APC interface state machine
 * @ifa: APC interface
 * @event: event comming to state machine
 *
 * This fully respects 9.3 of RFC 2328 except we have slightly
 * different handling of %DOWN and %LOOP state. We remove intefaces
 * that are %DOWN. %DOWN state is used when an interface is waiting
 * for a lock. %LOOP state is used when an interface does not have a
 * link.
 */
void apc_iface_sm(struct apc_iface * ifa, int event)
{
	printf("Iface SM Event is '%s' (%u)\n", apc_ism_names[event], ifa->state);

	switch(event)
	{
	case ISM_UP:
		if (ifa->state <= APC_IS_LOOP)
		{
			if (ifa->priority == 0)
				apc_iface_chstate( ifa, APC_IS_DROTHER );
			else
			{
				apc_iface_chstate( ifa, APC_IS_WAITING );
				if (ifa->wait_timer)
					tm_start(ifa->wait_timer, ifa->waitint);
			}
		
			if (ifa->hello_timer)
				tm_start( ifa->hello_timer, ifa->helloint );
			
			if (ifa->poll_timer)
				tm_start( ifa->poll_timer, ifa->pollint );
			
			apc_send_hello(ifa, OHS_HELLO);
		}
		break;

	case ISM_BACKS:
	case ISM_WAITF:
		if (ifa->state == APC_IS_WAITING)
			apc_dr_election(ifa);
		break;

	case ISM_NEICH:
		if (ifa->state >= APC_IS_DROTHER)
			apc_dr_election(ifa);
		break;

	case ISM_LOOP:
		if ((ifa->state > APC_IS_LOOP) && ifa->check_link)
			apc_iface_chstate(ifa, APC_IS_LOOP);
		break;

	case ISM_UNLOOP:
		/* Immediate go UP */
		if (ifa->state == APC_IS_LOOP)
			apc_iface_sm(ifa, ISM_UP);
		break;

	case ISM_DOWN:
		apc_iface_chstate(ifa, APC_IS_DOWN);
		break;

	default:
		printf("APC_I_SM - Unknown event?" );
		break;
	}
}


void apc_iface_new( void )
{
	struct apc_iface * ifa;
	printf("apc new iface\n");
	ifa = mb_allocz(sizeof(struct apc_iface));
	ifa->iface = NULL;
	ifa->addr = NULL;
	ifa->cf = NULL;
	
	ifa->iface_id = 1;
	ifa->ifname = IfName;
	
	ifa->priority = 0x11;
	ifa->drip = MyIpAddr;
	ifa->helloint = 4;
	ifa->deadint = 16;
	ifa->waitint = 16;
	
	ifa->type = APC_IT_BCAST;
	ifa->state = APC_IS_DOWN;
	init_list(&(ifa->neigh_list));
	
	ifa->hello_timer = tm_new_set(hello_timer_hook, ifa, 0, ifa->helloint);
	
	ifa->wait_timer = tm_new_set(wait_timer_hook, ifa, 0, 0);
	
	apc_ifa = ifa;
	
	/* Do iface UP, unless there is no link and we use link detection */
	apc_iface_sm(ifa, ISM_UP);
}


