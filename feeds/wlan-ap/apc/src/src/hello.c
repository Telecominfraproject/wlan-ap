/* SPDX-License-Identifier: BSD-3-Clause */
#include <apc.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <interap/interAPcomm.h>

struct apc_hello2_packet
{
	u8 basmac[6];
	u16 dummy1;
	u32 grpid;
	u32 netmask;
	u16 helloint;
	u8 options;
	u8 priority;
	u32 deadint;
	u32 dr;
	u32 bdr;
	
	u32 neighbors[APC_MAX_NEIGHBORS];
};

extern unsigned int MyIpAddr;
extern unsigned int WaitingToReelect;
extern unsigned int ApcGrpId;
extern unsigned char MyBasicMac[6];
extern unsigned int RestartProxy;
extern unsigned int FloatingIpSaved;
extern unsigned int BackingUpRadius;
extern struct apc_spec ApcSpecSaved;
static int ApcNeighborsFound[APC_MAX_NEIGHBORS];

u32 DrIp = 0;
unsigned int DrCount = 0;

/* Get current ip broadcast address */
int get_current_ip(char *ip, char *iface) {

	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error: socket failed");
		return -1;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

	if ((ioctl(fd, SIOCGIFBRDADDR, &ifr)) < 0) {
		printf("Error: ioctl failed");
		return -1;
	}

	memcpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
	       16);

	close(fd);

	return 0;
}



static int apc_check_neighbor_list(u32 Ip, u8 * Mac, int CheckForMissing )
{
	int i;

	for(i=1; i<APC_MAX_NEIGHBORS; i++ )
	{
		if (CheckForMissing )
		{
			if (ApcSpecSaved.Neighbors[i].Ip == 0 )
				return(0 );
			if (!ApcNeighborsFound[i] )
				return(1 );    /* Old list has more entries than new */
		}
		else
		{
			if (ApcSpecSaved.Neighbors[i].Ip == 0 )
				break;
			if (ApcSpecSaved.Neighbors[i].Ip == Ip )
			{
				ApcNeighborsFound[i] = 1;
				/* Existing neighbor - check if MAC is the same */
				if (memcmp(ApcSpecSaved.Neighbors[i].BasicMac,
						 Mac, 6)== 0 )
					return(0 );    /* Existing neighbor */
				else
					return(1 );    /* Something changed */
			}
		}
	}
	return(1 );                                /* New neighbor */
}


void apc_send_hello(struct apc_iface * ifa, int kind )
{
	int i;
	struct apc_neighbor * neigh;
	u32 * neighbors;
	struct apc_hello2_packet ps;
	unsigned int length, report = 0;
	struct apc_spec ApcSpec;
	char dst_ip[16];
	
	if (WaitingToReelect )
		return;
	
	memcpy(ps.basmac, MyBasicMac, 6 );
	ps.dummy1 = htons(0xa2a2 );
	ps.grpid = htonl(ApcGrpId );
	ps.netmask = htonl(MyIpAddr );
	ps.helloint = htons(ifa->helloint );
	ps.options = 0;
	ps.priority = ifa->priority;
	ps.deadint = htonl(ifa->deadint );
	ps.dr = htonl(ipa_to_u32(ifa->drip));
	ps.bdr = htonl(ipa_to_u32(ifa->bdrip));

	if (DrIp == ifa->drip )
		DrCount += 1;
	else
		DrCount = 0;
	
	DrIp = ifa->drip;
	if (DrCount >= 2 )
	{
		report = 1;
		memset(&ApcSpec, 0, sizeof(struct apc_spec));
		memset(ApcNeighborsFound, 0, sizeof(ApcNeighborsFound));
		if (ifa->drip == MyIpAddr )
			ApcSpec.IsApc = I_AM_APC;
		if (ifa->bdrip == MyIpAddr )
			ApcSpec.IsApc = I_AM_BAPC;
		ApcSpec.Neighbors[0].Ip = ifa->drip;
	}
	
	length = 32;
	neighbors = ps.neighbors;
	
	i = 0;

	/* Fill all neighbors */
	if (kind != OHS_SHUTDOWN)
	{
		WALK_LIST(neigh, ifa->neigh_list)
		{
			if (i == APC_MAX_NEIGHBORS)
			{
				printf("Too many neighbors\n" );
				break;
			}
			neighbors[i] = htonl(neigh->rid );
			if (report)
			{
				ApcSpec.Neighbors[i+1].Ip = neigh->rid;
				memcpy(&(ApcSpec.Neighbors[i+1].BasicMac[0]),
					 neigh->basic_mac, 6);
			if (apc_check_neighbor_list(neigh->rid,
						    neigh->basic_mac, 0))
				ApcSpec.Changed = 1;
			}
			i++;
		}
	}
	if (report )
	{
		if (apc_check_neighbor_list(0, 0, 1))
			ApcSpec.Changed = 1;
		if ((ApcSpec.IsApc == I_AM_APC) && (ApcSpecSaved.Neighbors[0].Ip == 0) )
			ApcSpec.Changed = 1;   /* Single AP became APC - has no neighbors */
		if (ApcSpec.IsApc == I_AM_APC )
		{
			ifa->priority = 0x22;
			if (ApcSpec.Changed || RestartProxy )
			{
				int n_neigh = i+1;
				unsigned int Neigh[APC_MAX_NEIGHBORS];
				int ii;
				
				for(ii=0; ii<n_neigh; ii++ )
					Neigh[ii] = ApcSpec.Neighbors[ii].Ip;
				//Radius stuff
				if (RestartProxy )
					RestartProxy = 0;
			}
			else
				ApcSpec.FloatIp = ApcSpecSaved.FloatIp;
		}
		else if (ApcSpec.IsApc == I_AM_BAPC )
		{
			ifa->priority = 0x12;
		}
		else {
			ifa->priority = 0x11;
			if ((ApcSpecSaved.IsApc == I_AM_APC) || BackingUpRadius )
			{
				//radius stuff
				FloatingIpSaved = 0;
				BackingUpRadius = 0;
			}
		}
	}

	length += i * sizeof(u32);

	printf("HELLO packet sent via  %s\n", ifa->ifname );
	memset(dst_ip, 0, 16);
	if ((get_current_ip(dst_ip, IAC_IFACE)) < 0) {
		printf("Error: Cannot get IP for %s", IAC_IFACE);
		return;
	}

	interap_send(IAC_APC_ELECTION_PORT, dst_ip, &ps , length);
}


void apc_receive_hello(unsigned char *pkt, struct apc_iface *ifa,
                        struct apc_neighbor *n, ip_addr faddr,
			unsigned int plen)
{
	char * err_dsc = NULL;
	u32 rcv_iface_id, rcv_helloint, rcv_deadint, rcv_dr, rcv_bdr, rcv_grpid;
	u8 rcv_options, rcv_priority;
	u32 * neighbors;
	u32 neigh_count;
	uint i, err_val = 0;
	struct apc_hello2_packet * ps = (struct apc_hello2_packet *)pkt;
	u32 rcv_rid = ntohl(ps->netmask );

	/* RFC 2328 10.5 */
	/*
	 * We may not yet have the associate neighbor, so we use Router ID
	   from the packet instead of one from the neighbor structure for log
	   messages.
	 */

	rcv_iface_id = 0;
	rcv_grpid = ntohl(ps->grpid );
	rcv_helloint = ntohs(ps->helloint );
	rcv_deadint = ntohl(ps->deadint );
	rcv_dr = ntohl(ps->dr );
	rcv_bdr = ntohl(ps->bdr );
	rcv_options = ps->options;
	rcv_priority = ps->priority;

	if (rcv_grpid != ApcGrpId )
		return;

	neighbors = ps->neighbors;
	neigh_count = (plen - 24) / sizeof(u32);
	printf("HELLO packet received from nbr %x on %s (%u) %p n_c=%u prio=%u\n", rcv_rid, ifa->ifname, ifa->state, n, neigh_count, rcv_priority );
	
	if (rcv_helloint != ifa->helloint)
	DROP("hello interval mismatch", rcv_helloint );
	
	if (rcv_deadint != ifa->deadint )
	DROP("dead interval mismatch", rcv_deadint );

	/* Check consistency of existing neighbor entry */
	if (n)
	{
		uint t = ifa->type;
		
		if (t == APC_IT_BCAST)
		{
			/* Neighbor identified by IP address;
			   Router ID may change */
			if (n->rid != rcv_rid)
			{
				printf("Neighbor %x on %s changed Router ID to %x",
				                n->rid, ifa->ifname, rcv_rid );
				apc_neigh_sm(n, INM_KILLNBR);
				n = NULL;
			}
		}
	}

	if (!n )
	{
		printf("New neighbor %x on %s, IP address %x\n",
		                rcv_rid, ifa->ifname, faddr );
		
		n = apc_neighbor_new(ifa );
		
		memcpy(n->basic_mac, ps->basmac, 6 );
		n->rid = rcv_rid;
		n->ip = faddr;
		n->dr = rcv_dr;
		n->bdr = rcv_bdr;
		n->priority = rcv_priority;
		n->iface_id = rcv_iface_id;
	}

	u32 n_id = ipa_to_u32(n->ip );
	
	u32 old_dr = n->dr;
	u32 old_bdr = n->bdr;
	u32 old_priority = n->priority;
	u32 old_iface_id = n->iface_id;
	
	n->dr = rcv_dr;
	n->bdr = rcv_bdr;
	n->priority = rcv_priority;
	n->iface_id = rcv_iface_id;
	
	/* Update inactivity timer */
	apc_neigh_sm(n, INM_HELLOREC);

	/* Examine list of neighbors */
	for(i = 0; i < neigh_count; i++ )
	{
		if (neighbors[i] == htonl(MyIpAddr))
			goto found_self;
	}
	
	apc_neigh_sm(n, INM_1WAYREC );
	return;

found_self:

	apc_neigh_sm(n, INM_2WAYREC);
	
	if (n->iface_id != old_iface_id)
	{
		/* If neighbor is DR, also update cached DR interface ID */
		if (ifa->drid == n->rid)
			ifa->dr_iface_id = n->iface_id;
	}
	
	if (ifa->state == APC_IS_WAITING)
	{
		/* Neighbor is declaring itself DR (and there is no BDR)
		 or as BDR */
		if (((n->dr == n_id) && (n->bdr == 0)) || (n->bdr == n_id))
			apc_iface_sm(ifa, ISM_BACKS);
	}
	else
		if (ifa->state >= APC_IS_DROTHER )
		{
			/* Neighbor changed priority or started/stopped
			   declaring itself as DR/BDR */
			if ((n->priority != old_priority) ||
			    ((n->dr == n_id) && (old_dr != n_id)) ||
			    ((n->dr != n_id) && (old_dr == n_id)) ||
			    ((n->bdr == n_id) && (old_bdr != n_id)) ||
			    ((n->bdr != n_id) && (old_bdr == n_id)) )
			    apc_iface_sm(ifa, ISM_NEICH );
		}
	return;

drop:

	printf("Bad HELLO packet from nbr %x on %s - (%s) (%u)",
                    rcv_rid, ifa->ifname, err_dsc, err_val );
}

