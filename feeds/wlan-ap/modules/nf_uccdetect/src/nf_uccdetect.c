/* SPDX-License-Identifier BSD-3-Clause */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_bridge.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/version.h>
#include <linux/types.h>

#include <linux/netlink.h>
#include <linux/export.h>
#include <net/genetlink.h>
#include <linux/delay.h>

#include <linux/timer.h>

#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include "nl_ucc.h"

#ifdef BIG_MAC_CACHE
#define MAC_HASH_SIZE           65536
#define HASH_function( addr_high, addr_low ) (addr_low & 0xFFFF)
#define HASH_AGEING_UNIT            10
#else
#define MAC_HASH_SIZE           2048
#define HASH_function( addr_high, addr_low ) (addr_low & 0x7FF)
#define HASH_AGEING_UNIT            1
#endif

#define MAC_HASH_FLAG_ENTRY_VALID       1


#define VOIP_FLAG_RTP_TO_CLT            1

#define VOIP_MARK_SKYPE                 101
#define VOIP_MARK_GOTOMEETING           102
#define VOIP_MARK_ZOOMMEETING           103

#define HEUR_CALL_INACTIVITY_TIME   5
#define SIP_CALL_INACTIVITY_TIME    20
#define SIP_CALL_ENDED_BYE          0
#define SIP_CALL_DROPPED            1

#define VOIP_FLAG_RTP_TO_CLT            1


#define INTRF_UNKNOWN       0
#define INTRF_ETH           1
#define INTRF_TUNNEL        2
#define INTRF_WIFI          3
#define INTRF_DUMMY         4
#define INTRF_WDS           5

/* Mac entries table */
static struct mac_entry * MacHash[MAC_HASH_SIZE];

struct kmem_cache * mac_cache __read_mostly;

unsigned int ageing_time;

unsigned int CurrentHashPos;
#ifdef BIG_MAC_CACHE
    unsigned int HashScanStep = (MAC_HASH_SIZE/10) + 1;
#else
    unsigned int HashScanStep = MAC_HASH_SIZE;
#endif

/* Buffer for sending to userspace */
#define CAPT_NUM_BUFFERS     4096
struct capture_storage
{
    spinlock_t Locked;
    uint32_t CurCaptHead;
    uint32_t CurCaptTail;
    struct capture_buf CaptureArray[CAPT_NUM_BUFFERS];
};

struct capture_storage captures;

/* Voice session data structures */
struct voip_session
{
    uint32_t CltMacHigh;
    uint32_t CltMacLow;

    uint64_t SipSessionId;
    unsigned int SipPort;
    unsigned int SipState;
    uint32_t RtpPortsFrom;
    uint32_t RtpPortsTo;
    uint32_t RtpVideoPortsFrom;
    uint32_t RtpVideoPortsTo;
    uint32_t RtpIpFrom;
    uint32_t RtpIpTo;
};

struct voip_flow
{
    uint32_t SrcIp;
    uint32_t Ports;
    unsigned char Age;
    unsigned char IsVoip;
    uint16_t Count;
    uint32_t LenInSec;
};

struct streaming_video_track
{
    unsigned int ActiveAge;
    uint64_t TotalBytes;
    unsigned int StartTime;
    uint32_t ServerIp;
};

#define MAX_VIDEO_STREAMS       4
#define MAX_MAC_ID_LEN          20
#define MAX_UDP_FLOWS           5
#define MAX_TCP_FLOWS           5

struct mac_entry
{
    uint32_t VlanMacHigh;
    uint32_t MacLow;
    struct net_device *port;
    unsigned int age;
    unsigned int flags;
    unsigned int mac_flags;

    unsigned int ua_flag;
    unsigned int uaTimeOut;

    unsigned char Identification[MAX_MAC_ID_LEN];
    unsigned int IdentLen;

    unsigned int LastAccFrom;
    unsigned int LastSecFrom;
    unsigned int TokensFrom;
    unsigned int DroppedFrom;

    unsigned int LastAccTo;
    unsigned int LastSecTo;
    unsigned int TokensTo;
    unsigned int DroppedTo;

    unsigned int SipCallEndTimer;
    unsigned int SipCallActiveTimer;
    uint64_t SipSessionId;
    unsigned int SipPort;
    unsigned int SipState;
    uint32_t RtpPortsFrom;
    uint32_t RtpPortsTo;
    uint32_t RtpVideoPortsFrom;
    uint32_t RtpVideoPortsTo;
    uint32_t RtpIpFrom;
    uint32_t RtpIpTo;
    unsigned char RtpPayload[4];
    unsigned int VideoIsDetected;
    unsigned int SipReportSent;
    unsigned int SipReportIndex;
    unsigned int VoIPflags;
    uint32_t RtpIpCand1;
    uint32_t RtpIpCand2;
    uint16_t RtpPortCand1;
    uint16_t RtpPortCand2;
    struct streaming_video_track SvideoTrack[MAX_VIDEO_STREAMS];

    struct voip_flow UdpFlows[MAX_UDP_FLOWS];
    struct voip_flow TcpFlows[MAX_TCP_FLOWS];

    char Url[50];

    uint32_t tx_packets;
    uint32_t tx_bytes;
    uint32_t tx_wraps;
    uint32_t rx_packets;
    uint32_t rx_bytes;
    uint32_t rx_wraps;
#ifdef IP_HASH
    uint32_t ip_addr;
#endif
    struct mac_entry * next_entry;
};

struct packet
{
    void *pkt;
    unsigned char * curptr;
    unsigned char * l2ptr;
    unsigned char * l3ptr;
    unsigned char * l4ptr;

    struct intrf * IntrfSrc;
    struct intrf * IntrfDst;
    struct mac_entry * HashDst;
    struct net_device *src_dev;

    unsigned int datalen;
    int offset;
    unsigned int Cloned;

    uint32_t DstMacHigh;
    uint32_t DstMacLow;
    uint32_t SrcMacHigh;
    uint32_t SrcMacLow;

    uint32_t vlan;
    uint32_t proto;

    unsigned int type;
};

/*Layer 3 protocols */
#define L3_PROTO_IP             0x08000000
#define L3_PROTO_ARP            0x08060000
#define L3_PROTO_EAPOL          0x888E0000
#define L3_PROTO_BRCM           0x886C0000

/* Ethernet frame positions */
#define ETH_SRC_MAC
#define ETH_DEST_MAC
#define ETH_TYPE_POS 12
#define ETH_L3_DATA (ETH_TYPE_POS+2)
#define ETH_TYPE_POS_VLAN 16
#define ETH_L3_DATA_VLAN (ETH_TYPE_POS_VLAN+2)

/* mac address tracking */
#define CACHE_MAC_ENTRY         1
#define CACHE_INTF_ENTRY        2
#define CACHE_TUNNEL_ENTRY      3
#define CACHE_CLT_ENTRY         4

#define MAX_MGMT_TRACK  12
struct capt_track
{
    unsigned int TimeGot;
    unsigned int Count;
};

struct clt_entry
{
    uint32_t VlanMacHigh;
    uint32_t MacLow;
    unsigned int age;
    unsigned int flags;

    unsigned int ua_flag;
    unsigned int uaTimeOut;

    unsigned int CpTimeout;
    unsigned int CpBpsUp;
    unsigned int CpBpsDown;
    unsigned int CpWhitelist;

    uint32_t ApIp;
    unsigned int ArpAge;
    uint32_t ArpIp;
    uint32_t vlan;

    uint64_t SipSessionId;
    unsigned int SipPort;
    unsigned int SipState;
    uint32_t RtpPortsFrom;
    uint32_t RtpPortsTo;
    uint32_t RtpVideoPortsFrom;
    uint32_t RtpVideoPortsTo;
    uint32_t RtpIpFrom;
    uint32_t RtpIpTo;

    struct capt_track CaptTrack[MAX_MGMT_TRACK];

    unsigned int FpAge;
    unsigned int TimeGotProbe;
    unsigned int NbrAge;
    unsigned int TimeGotApRecPkt;
    struct clt_entry * next_entry;
};

struct rtp_stat
{
    uint64_t wifiSessionId;
    uint32_t rtpAvLatency;
    uint32_t rtpAvJitter;
    uint32_t rtpPktLoss;
    uint32_t rtpConsecPktLoss;
    uint32_t rtpTotalSent;
    uint32_t rtpTotalLost;
    uint32_t rtpSeqFirst;
    uint32_t rtpSeqLast;
};

#define PKT_TYPE_SIP_CALL_START         103
#define PKT_TYPE_SIP_CALL_STOP          104
#define PKT_TYPE_SIP_CALL_REPORT        105

struct sip_call_end
{
    unsigned long long SessionId;
    unsigned char CltMac[6];
    int WifiIf;                                 /* To get WiFi session ID */
    unsigned int CltMos;
    unsigned int Reason;
    unsigned char Codecs[4];
    unsigned int Latency;
    unsigned int Jitter;
    unsigned int PktLostPerc;
    unsigned int PktLostCons;
    unsigned int VideoCodec;
    unsigned int TotalPktSent;
    unsigned int TotalPktLost;
    unsigned int RtpSeqFirst;
    unsigned int RtpSeqLast;
    unsigned int SipReportIdx;
    char Url[50];
} __attribute__((packed));

struct radio_parms
{
    int Rssi;
    unsigned int Channel;
    unsigned int DataRate;
};

struct kmem_cache * clt_cache __read_mostly;

#define MAC_HASH_FLAG_ENTRY_VALID       1
#define MAC_HASH_FLAG_PORT_VALID        2
#define MAC_HASH_FLAG_IS_WIRELESS       4
#define HASH_DYNAMIC_FLG                1
#define MAC_HASH_LOCAL                  2
#define HASH_BPDU_FLG                   4
#define HASH_ACTIVE_FLG                 8
#define HASH_DYN_AND_ACTIVE_FLG         (HASH_DYNAMIC_FLG | HASH_ACTIVE_FLG)
#define HASH_MULTICAST_FLG              0x10
#define HASH_TUNNEL_FLAG                0x20
#define HASH_ANY_VLAN_FLAG              0x40
#define DNS_CP_LOOKUP                   0x80
#define BPDU_MAC_HIGH       0xFFFF0180
#define BPDU_MAC_LOW        0xC2000000

#define HASH_address_is_equal( addr_high, addr_low, ptr ) ((ptr->MacLow == addr_low) && \
                                                           ((ptr->VlanMacHigh == addr_high) || \
                                                            (((ptr->flags == MAC_HASH_LOCAL) || ((addr_high & 0xFFFF0000) == 0xEFFF0000)) && \
                                                             ((ptr->VlanMacHigh & 0xFFFF) == (addr_high & 0xFFFF))) \
                                                           ))


//static int CltHashProt[MAC_HASH_SIZE];
static struct clt_entry * CltMacHash[MAC_HASH_SIZE];

/* Function prototypes */
static void send_data(unsigned int group, void *data, unsigned int len);
void voip_generate_report_event( struct mac_entry * HashPtr, struct net_device * Intrf, unsigned int CltMos, unsigned int Reason);

void voip_generate_end_event( struct mac_entry * HashPtr, unsigned int Reason );
void voip_clean_session( struct mac_entry * HashPtr );
void voip_heuristic_flow_call_start( struct mac_entry * hash_ptr, int flow_index, int IsTcp );


/*
 * get_if_type:Get type of interface
 */
int get_if_type(char *name) {
	if (!strncmp(name, "eth", 3))
		return INTRF_ETH;
	if (!strncmp(name, "wlan", 4))
		return INTRF_WIFI;
}

/*
 * get_radio_id: Get id of wifi interface
 */
int get_radio_id(char *name) {
	char id[4] = {'\0','\0','\0'};
	long res;
	int i;

	if (!strncmp(name, "wlan", 4)) {
		id[0] = name[4];

		for (i = 0; i < 2; i++) {
			if (id[i] != '_')
				id[i] = name[i+4];
		}
		kstrtol(id, 10, &res);
		return res;
	}
    
	if (!strncmp(name, "eth", 3))
		return -1;
	return (res);
}

/* MAC ageing timer */
static struct timer_list CmdTimer;
static int CountTick = 0;

void glue_cache_free( void * Ptr, int Type )
{
	switch( Type )
	{
	case CACHE_MAC_ENTRY:
		kmem_cache_free( mac_cache, Ptr );
		break;
	case CACHE_CLT_ENTRY:
		kmem_cache_free( clt_cache, Ptr );
		break;
	default:
		break;
	}
}


static void mac_free_entry( struct mac_entry * del_ptr )
{
	glue_cache_free( (void *)del_ptr, CACHE_MAC_ENTRY );
}

/* mac_handle_udp_flows: Determine if a udp flow could be a voip call
 *                       based on speed and size of packets.
 * @hash_ptr: mac entry
 */
static void mac_handle_udp_flows( struct mac_entry * hash_ptr )
{
    int iflow; 
    for( iflow = 0; iflow<MAX_UDP_FLOWS; iflow++ )
    {
        if( hash_ptr->UdpFlows[iflow].Age )
        {
            hash_ptr->UdpFlows[iflow].Age -= 1;
            if( hash_ptr->UdpFlows[iflow].Age )
            {
                if( (hash_ptr->UdpFlows[iflow].Count > 13) &&
                    (hash_ptr->UdpFlows[iflow].Count < 80) &&  /* Number of packets per second */
                    (hash_ptr->UdpFlows[iflow].LenInSec/hash_ptr->UdpFlows[iflow].Count < 400) )  /* Average packet size */

                {
                    if( hash_ptr->UdpFlows[iflow].IsVoip >= 3 )
                    {
                        printk(KERN_DEBUG "flow %i (%x %x) detected as VoIP C=%u, l=%x",
                                iflow,
                                hash_ptr->UdpFlows[iflow].SrcIp,
                                hash_ptr->UdpFlows[iflow].Ports,
                                hash_ptr->UdpFlows[iflow].Count,
                                hash_ptr->UdpFlows[iflow].LenInSec );

                        voip_heuristic_flow_call_start( hash_ptr, iflow, 0 );
                    }
                    hash_ptr->UdpFlows[iflow].IsVoip += 1;
                }
                else
                    printk(KERN_DEBUG "flow %i (%x %x) is not VoIP c=%u, l=%x", iflow,
                            hash_ptr->UdpFlows[iflow].SrcIp,
                            hash_ptr->UdpFlows[iflow].Ports,
                            hash_ptr->UdpFlows[iflow].Count,
                            hash_ptr->UdpFlows[iflow].LenInSec);
            }
            else
            {
                if( hash_ptr->UdpFlows[iflow].IsVoip )
                {
                    printk("flow %d is aged %x %x", iflow,
                            hash_ptr->UdpFlows[iflow].SrcIp,
                            hash_ptr->UdpFlows[iflow].Ports);
                    hash_ptr->UdpFlows[iflow].IsVoip = 0;
                }
            }
            hash_ptr->UdpFlows[iflow].Count = 0;
            hash_ptr->UdpFlows[iflow].LenInSec = 0;
        }
    }
}

/* mac_ageing: Timer which checks the mac_entry periodically 
 */
static void mac_ageing(void)
{
    struct mac_entry * hash_ptr, * prev_ptr, * del_ptr;
    unsigned int scan_end = CurrentHashPos + HashScanStep;
    int i;

    if( scan_end > MAC_HASH_SIZE )
        scan_end = MAC_HASH_SIZE;

    for( i=CurrentHashPos; i<scan_end; i++ )
    {
        hash_ptr = MacHash[i];
        if( hash_ptr == NULL )
            continue;
        prev_ptr = NULL;
        while( hash_ptr )
        {
            del_ptr = NULL;
            mac_handle_udp_flows( hash_ptr );

            if( hash_ptr->SipCallEndTimer )
            {
                /* Counting time to intercept SIP PUBLISH packet */
                hash_ptr->SipCallEndTimer -= HASH_AGEING_UNIT;
                if( hash_ptr->SipCallEndTimer == 0 )
                    voip_clean_session( hash_ptr );
            }
            if( hash_ptr->SipCallActiveTimer )
            {
                hash_ptr->SipCallActiveTimer -= HASH_AGEING_UNIT;
                if( hash_ptr->SipCallActiveTimer == 0 )
                {
                    unsigned int Reason = SIP_CALL_DROPPED;

                    if( hash_ptr->SipState >= VOIP_MARK_SKYPE )
                        Reason = SIP_CALL_ENDED_BYE;
                    voip_generate_end_event( hash_ptr, Reason );
                    voip_clean_session( hash_ptr );
                }
            }
            if( hash_ptr->age )
            {
                /* Adjust age if ageing time was reduced by configuration */
                if( (hash_ptr->age > ageing_time)/* &&
                    !(hash_ptr->mac_flags  & MAC_HASH_FLAG_IS_WIRELESS)*/)
                    hash_ptr->age = ageing_time;
                hash_ptr->age -= HASH_AGEING_UNIT;
                if( hash_ptr->age == 0 )
                    del_ptr = hash_ptr;
            }
            if( del_ptr )
            {
                if( prev_ptr )
                    prev_ptr->next_entry = hash_ptr->next_entry;
                else
                    MacHash[i] = hash_ptr->next_entry;
                hash_ptr = hash_ptr->next_entry;

                mac_free_entry( del_ptr );
            }
            else
            {
                prev_ptr = hash_ptr;
                hash_ptr = hash_ptr->next_entry;
            }
        }
    }
    if( scan_end < MAC_HASH_SIZE )
        CurrentHashPos += HashScanStep;
    else
        CurrentHashPos = 0;
}

/*mac entry timer*/
void mac_ageing_tick( void )
{
	mac_ageing();
}

void cmd_timer( unsigned long _data )
{
#ifdef BIG_MAC_CACHE
	mac_ageing_tick( );
#else
	CountTick += 1;
	if( CountTick == 10 )
	{
		mac_ageing_tick( );
		CountTick = 0;
	}
#endif
	mod_timer( &CmdTimer, jiffies + HZ/10 );
}


/* End MAC ageing timer */


void glue_get_uptime( uint64_t * msec, uint64_t * usec )
{
	struct timespec tv;
	uint64_t Sec, Usec;
	tv = ktime_to_timespec(ktime_get_boottime());

	Sec = tv.tv_sec;
	Usec = tv.tv_nsec;
	
	do_div( Usec, 1000000 );
	*msec = (Sec * 1000) + Usec;
	
	Usec = tv.tv_nsec;
	do_div( Usec, 1000 );
	*usec = (Sec * 1000000) + Usec;
}

/* Cache mem allocation for mac entries */
void * glue_cache_allocate( int Type )
{
	switch( Type )
	{
	case CACHE_MAC_ENTRY:
		return( (void *)kmem_cache_alloc( mac_cache, GFP_ATOMIC ) );
	case CACHE_CLT_ENTRY:
		return( (void *)kmem_cache_alloc( clt_cache, GFP_ATOMIC ) );
	default:
		break;
	}
	return( NULL );
}

struct clt_entry * clt_hash_search( uint32_t mac_high, uint32_t mac_low )
{
	if( mac_high & 0x00000100 )
	{
		return( NULL );
	}
	else
	{
		struct clt_entry * hash_ptr;
		unsigned int hash_index = HASH_function( mac_high, mac_low );
		
		hash_ptr = CltMacHash[hash_index];
		while( hash_ptr )
		{
			if( HASH_address_is_equal( mac_high, mac_low, hash_ptr ) )
			{
				return( hash_ptr );
			}
			hash_ptr = hash_ptr->next_entry;
		}
	}

	return( NULL );
}

/*
 * mac_hash_put: Create a new mac entry
 * @addr_high: Mac addr High bytes
 * @addr_low: Mac addr Low bytes
 * @len: Length of packet
 * @intrf: interface device
 * @prev_next_ptr: ptr to hash bucket tail
 */
static struct mac_entry * mac_hash_put( uint32_t addr_high, uint32_t addr_low,
					unsigned int len, 
					struct net_device *intrf,
					struct mac_entry ** prev_next_ptr)
{

	struct mac_entry * NewEntryPtr = NULL;
	NewEntryPtr = (struct mac_entry *)glue_cache_allocate( CACHE_MAC_ENTRY );
	
	if( NewEntryPtr )
	{
		struct clt_entry * CltHash = clt_hash_search(addr_high,
							     addr_low );
		
		memset( NewEntryPtr, 0, sizeof( struct mac_entry ) );
		NewEntryPtr->VlanMacHigh = addr_high;
		NewEntryPtr->MacLow = addr_low;
		NewEntryPtr->flags  = HASH_DYN_AND_ACTIVE_FLG;
		NewEntryPtr->port   = intrf;
		NewEntryPtr->ua_flag = 0;
		
		if( intrf )
			NewEntryPtr->age = ageing_time;
		if( len )
			NewEntryPtr->rx_packets = 1;
		NewEntryPtr->rx_bytes = len;

		/*Roaming NOT Tested*/
		if( CltHash )
		{
			/* If client just roamed to the AP but it has already */
			/* entry in CltHash for VoIP, learn values from there */
			/* and also retrieves the User Agent flag to prevent  */
			/* sending duplicate User Agent String to cloud       */
			NewEntryPtr->SipSessionId = CltHash->SipSessionId;
			NewEntryPtr->SipPort = CltHash->SipPort;
			NewEntryPtr->SipState = CltHash->SipState;
			NewEntryPtr->RtpIpFrom = CltHash->RtpIpFrom;
			NewEntryPtr->RtpIpTo = CltHash->RtpIpTo;
			NewEntryPtr->RtpPortsFrom = CltHash->RtpPortsFrom;
			NewEntryPtr->RtpPortsTo = CltHash->RtpPortsTo;
			NewEntryPtr->RtpVideoPortsFrom = CltHash->RtpVideoPortsFrom;
			NewEntryPtr->RtpVideoPortsTo = CltHash->RtpVideoPortsTo;
			NewEntryPtr->SipReportIndex = 0;
			
			NewEntryPtr->ua_flag = CltHash->ua_flag;


			printk("Learned (put) VoIP entry (%x %x) %x=>%x %u",
				addr_high, addr_low, CltHash->RtpIpFrom,
				CltHash->RtpIpTo, CltHash->SipPort );

			if( (get_if_type(intrf->name) == INTRF_WIFI) &&
				NewEntryPtr->SipSessionId )
				voip_generate_report_event( NewEntryPtr,
							   intrf, 0, 1 );
		}

		*prev_next_ptr = NewEntryPtr;
		NewEntryPtr->mac_flags = MAC_HASH_FLAG_ENTRY_VALID;
		//printk(KERN_DEBUG "learned %x %x\n", addr_high, addr_low );
	}

	return( NewEntryPtr );
}

/*
 * mac_hash_insert: Insert a new mac entry
 * @addr_high: Mac addr High bytes
 * @addr_low: Mac addr Low bytes
 * @len: Length of packet
 * @intrf: interface device
 */
struct mac_entry * mac_hash_insert( uint32_t mac_high, uint32_t mac_low, unsigned int len, struct net_device *intrf )
{
    unsigned int hash_index;
    struct mac_entry * hash_ptr;
    struct mac_entry * prev_ptr;

    /* Protections */
    if( intrf == NULL )
        return( NULL );


    hash_index = HASH_function( mac_high, mac_low );

    hash_ptr = MacHash[hash_index];

    if( !hash_ptr )
    {
        /*If the designated hash entry is empty,
          insert the new data in its beginning.*/
        hash_ptr = mac_hash_put( mac_high, mac_low, len, intrf, &(MacHash[hash_index]) );

    }
    else
    {
        do
        { /*Seek the MAC & intrf in the designated entry.*/
            if( HASH_address_is_equal( mac_high, mac_low, hash_ptr ) )
            { /*Update the entry if found and if it is not static.*/
                if( !( hash_ptr->flags & MAC_HASH_LOCAL ) )
                {
                    if( hash_ptr->SipSessionId )
                    {
                        if( hash_ptr->port != intrf )
                        {
                            if ((get_if_type(intrf->name) == INTRF_WIFI) &&
                                (hash_ptr->SipCallEndTimer == 0))
                            {
                                hash_ptr->SipReportIndex += 1;
                                voip_generate_report_event(hash_ptr, intrf,
                                                              0, 1);
                            }
                            if ((get_if_type(intrf->name) == INTRF_ETH))
                            {
                                voip_generate_report_event(hash_ptr,
                                                        hash_ptr->port, 0, 0 );
                                hash_ptr->SipCallActiveTimer = 0;
                                hash_ptr->SipCallEndTimer = 0;
                            }
                        }
                    }

                    hash_ptr->port = intrf;
                    if( hash_ptr->flags == HASH_ANY_VLAN_FLAG )
                    {
                        hash_ptr->flags = HASH_DYN_AND_ACTIVE_FLG;
                        hash_ptr->VlanMacHigh = mac_high;
                    }
                    hash_ptr->mac_flags |= MAC_HASH_FLAG_ENTRY_VALID;
                    if(hash_ptr->age)
                    {
                            hash_ptr->age = ageing_time;
                    }
                }
                hash_ptr->rx_packets += 1;
                if( (0xFFFFFFFF-len) < hash_ptr->rx_bytes )
                    hash_ptr->rx_wraps += 1;
                hash_ptr->rx_bytes += len;
                return( hash_ptr );
            }
            prev_ptr = hash_ptr;
            hash_ptr = hash_ptr->next_entry;
        } while( hash_ptr );

        /*If MAC & intrf were not found in the table,
          insert them to a new cell in the designated entry*/
        hash_ptr = mac_hash_put( mac_high, mac_low, len, intrf, &(prev_ptr->next_entry) );
    }

    return( hash_ptr );
}

void mac_hash_search(struct packet * PktPtr, uint32_t mac_high, uint32_t mac_low )
{
	PktPtr->IntrfDst = NULL;
	if( mac_high & 0x00000100 )
	{
		PktPtr->type = HASH_MULTICAST_FLG;
		if( (mac_high == BPDU_MAC_HIGH) && (mac_low == BPDU_MAC_LOW) )
			PktPtr->type = HASH_BPDU_FLG;
		PktPtr->HashDst = NULL;
		return;
	}
	else
	{
		struct mac_entry * hash_ptr;
		unsigned int hash_index = HASH_function( mac_high, mac_low );
		
		PktPtr->type = 0;
		hash_ptr = MacHash[hash_index];

		while( hash_ptr )
		{
			if((hash_ptr->mac_flags & MAC_HASH_FLAG_ENTRY_VALID)
			&& HASH_address_is_equal( mac_high, mac_low, hash_ptr))
			{
				if( hash_ptr->flags == MAC_HASH_LOCAL )
				{
					PktPtr->type = HASH_TUNNEL_FLAG;
					PktPtr->HashDst = NULL;
					PktPtr->IntrfDst = NULL;
					return;
				}
				{
					PktPtr->type = hash_ptr->flags;
					PktPtr->HashDst = hash_ptr;
					//PktPtr->IntrfDst = hash_ptr->port;
					return;
				}
			}
			hash_ptr = hash_ptr->next_entry;
		}
	}

	PktPtr->HashDst = NULL;
	return;
}
/* end: mac address tracking */



/*
 * voip_port_is_sip: Check of the port is a SIP port
 */
int voip_port_is_sip( unsigned int Port )
{
	if( (Port == 5060) || (Port == 5091) ||
	   ((Port >= 5196) && (Port <= 5199)) )
		return( 1 );

	return( 0 );
}

#define MAX_VOIP_CODECS             100
#define VOIP_CALL_READ_TIMER        600

struct voip_codec
{
    uint32_t MacHigh;
    uint32_t MacLow;
    char Url[50];
    char Codecs[15][30];
};

static char * SipMethod[8] = {"REGISTER", "INVITE", "ACK", "CANCEL", "BYE", "OPTIONS", "SIP/2.0", "PUBLISH"};
static int SipLen[8] = {8, 6, 3, 6, 3, 7, 7, 7};
static char * SipString[5] = {"m=", "c=", "f:", "t:", "To:"}; 
static struct voip_codec VoipCodecs[MAX_VOIP_CODECS];

struct sip_call_start
{
    unsigned long long SessionId;
    unsigned char CltMac[6];
    int WifiIf;                                 /* To get WiFi session ID */
    char Url[50];
    char Codecs[15][30];
} __attribute__((packed));

unsigned int VoipCallsCounter[2] = {0, 0};
unsigned int VoipCallsTimer[2] = {0, 0};

static void voip_increment_counter( int WiFiIf )
{
	if( VoipCallsCounter[WiFiIf] == 0 )
		VoipCallsTimer[WiFiIf] = VOIP_CALL_READ_TIMER;

	VoipCallsCounter[WiFiIf] += 1;
	printk("Call counter on radio %d incremented to %u", WiFiIf,
	VoipCallsCounter[WiFiIf]);
}

static unsigned int voip_get_client_mos( char * Data )
{
	char Int, Dot, Dec1, Dec2;
	char * MosPtr = strstr( Data, "MOSCQ=" );
	unsigned int Mos = 0;

	if( MosPtr )
	{
		Int = *(MosPtr+6);
		Dot = *(MosPtr+7);
		Dec1 = *(MosPtr+8);
		Dec2 = *(MosPtr+9);
		if( (Int >= '0') && (Int <= '5') )
		{
			Mos = ((unsigned int)Int - 48) * 100;
			if( (Dot == ' ') || (Dot == 0) )  /* MOSCQ=X */
				return( Mos );
			else
				if( Dot == '.' )
				{
					if( (Dec2 == ' ') || (Dec2 == 0) )
					{
						if( (Dec1 >= '0') && (Dec1 <= '9') )
						{
							Mos += ((unsigned int)Dec1 - 48) * 10;
							return( Mos ); /* MOSCQ=X.Y */
						}
						else
							return( 0 ); /* Invalid format */
					}
					else
					{
						if( (Dec2 >= '0') && (Dec2 <= '9') )
						{
							Mos += ((unsigned int)Dec2 - 48);
							return( Mos );      /* MOSCQ=X.YZ */
						}
						else
							return( 0 );        /* Invalid format */
					}
				}
				else
					return( 0 );                /* Invalid format */
		}
	}

	return( 0 );
}


/* Done with call, clean the session info */
void voip_clean_session( struct mac_entry * HashPtr )
{
	struct clt_entry * hashEntry = clt_hash_search( HashPtr->VlanMacHigh,
							HashPtr->MacLow );

	/* Clean call in MAC hash */
	HashPtr->SipCallEndTimer = 0;
	HashPtr->SipCallActiveTimer = 0;
	HashPtr->SipSessionId = 0;
	HashPtr->SipPort = 0;
	HashPtr->SipState = 0;
	HashPtr->RtpPortsFrom = HashPtr->RtpPortsTo = 0;
	HashPtr->RtpVideoPortsFrom = HashPtr->RtpVideoPortsTo = 0;
	HashPtr->RtpIpFrom = HashPtr->RtpIpTo = 0;
	HashPtr->VideoIsDetected = 0;
	HashPtr->SipReportIndex = 0;
	memset( HashPtr->RtpPayload, 0, 4 );
	HashPtr->SipReportSent = 0;
	HashPtr->VoIPflags = 0;

	if( hashEntry )
	{
		/* Clean call in CLT hash */
		hashEntry->SipSessionId      = 0;
		hashEntry->SipPort           = 0;
		hashEntry->SipState          = 0;
		hashEntry->RtpPortsFrom      = 0;
		hashEntry->RtpPortsTo        = 0;
		hashEntry->RtpVideoPortsFrom = 0;
		hashEntry->RtpVideoPortsTo   = 0;
		hashEntry->RtpIpFrom         = 0;
		hashEntry->RtpIpTo           = 0;
	}
}

uint32_t serv_string_to_ip( char * str )
{
	uint32_t ip;
	unsigned int Temp[4];
	unsigned char * ipAddrs = (unsigned char *)&ip;
	int i;

	if(sscanf(str, "%d.%d.%d.%d", &Temp[0], &Temp[1], &Temp[2], &Temp[3]) != 4 )
        return( 1 );
	for( i=0; i<4; i++ )
	{
		if( Temp[i] > 255 )
			return( 1 );
		ipAddrs[i] = (unsigned char)Temp[i];
	}

	ip = htonl( ip );
	return( ip );
}

uint64_t glue_get_time_msec( void )
{
	struct timeval Time;
	uint64_t TimeInMs, Sec;
	
	do_gettimeofday( &Time );
	Sec = Time.tv_sec;
	TimeInMs = (Sec * 1000) + (Time.tv_usec / 1000);
	return( TimeInMs );
}

/* Copy codec being used */
static void voip_sip_add_codec(uint32_t MacHigh, uint32_t MacLow,
                                  char * Codec )
{
	int i, n = -1;

	for( i=0; i<MAX_VOIP_CODECS; i++ )
	{
		if((VoipCodecs[i].MacHigh == 0) && (VoipCodecs[i].MacLow == 0)
			&& (n == -1) )
			n = i;
		if((VoipCodecs[i].MacHigh == MacHigh) &&
		   (VoipCodecs[i].MacLow == MacLow))
		{
			int j, nn = -1, ll = strlen( Codec );

			if( ll > 20 )
				ll = 20;

			for( j=0; j<15; j++ )
			{
				if((VoipCodecs[i].Codecs[j][0] == 0) &&
				   (nn == -1) )
					nn = j;
				if( memcmp(&(VoipCodecs[i].Codecs[j][0]),
					   Codec, ll ) == 0 )
					return; /* Duplicated entry dont save*/
			}
			if( nn != -1 )
			{
				memcpy( &(VoipCodecs[i].Codecs[nn][0]),
					Codec, 29 );
				return;
			}
		}
	}
	if( n == -1 )
		return;

	VoipCodecs[n].MacHigh = MacHigh;
	VoipCodecs[n].MacLow = MacLow;
	memcpy( &(VoipCodecs[n].Codecs[0][0]), Codec, 29 );
}

/* Send the event capture */
void capture_put(unsigned char * Buf, unsigned int From, unsigned int Len,
                 unsigned int Type, unsigned int Direction,
                 struct radio_parms * RadioParms, uint64_t SessionId,
                 int * wifiIf, char* mac, unsigned int Count)
{
unsigned int pos;

	if( Len > CAPT_BUF_SIZE )
		Len = CAPT_BUF_SIZE;

	spin_lock( &captures.Locked );
	pos = captures.CurCaptTail;
	
	captures.CurCaptTail = (captures.CurCaptTail + 1) % CAPT_NUM_BUFFERS;

	if( captures.CurCaptTail == captures.CurCaptHead )
		captures.CurCaptHead = (captures.CurCaptHead + 1)
                % CAPT_NUM_BUFFERS;

	spin_unlock( &captures.Locked );

	glue_get_uptime(&(captures.CaptureArray[pos].TimeStamp),
			&(captures.CaptureArray[pos].tsInUs) );
	
	captures.CaptureArray[pos].SessionId = SessionId;
	captures.CaptureArray[pos].From = From;
	captures.CaptureArray[pos].Type = Type;
	captures.CaptureArray[pos].Direction = Direction;
	captures.CaptureArray[pos].Count = Count;
	memcpy( captures.CaptureArray[pos].Buffer, Buf, Len );
	captures.CaptureArray[pos].Len = Len;

	if( wifiIf )
	{
		captures.CaptureArray[pos].wifiIf = *wifiIf;
	}

	if( mac )
		memcpy( captures.CaptureArray[pos].staMac, mac, 6 );
	else
		memset( captures.CaptureArray[pos].staMac, 0, 6 );

	if (RadioParms) 
	{
		captures.CaptureArray[pos].Rssi = RadioParms->Rssi;
		captures.CaptureArray[pos].Channel = RadioParms->Channel;
		captures.CaptureArray[pos].DataRate = RadioParms->DataRate;
	}
	else {
		captures.CaptureArray[pos].Channel = 0xFFFFFFFF;
	}

	send_data(0, (void *)&captures.CaptureArray[pos],
		  sizeof(struct capture_buf));
}

/* Send Report event to userspace */
void voip_generate_report_event(struct mac_entry * HashPtr,
                                   struct net_device * Intrf, 
                                   unsigned int CltMos, unsigned int Reason )
{
	struct sip_call_end SipCallEnd;
	unsigned char Tmp[4];
	uint64_t WiFiSessionId = 0;
	int WiFiIf = 0;
	
	if( HashPtr->SipReportSent == (Reason+1) )
	{
		printk("SIP report %u already sent", Reason );
		return;
	}

	if( (Reason == 0) && (HashPtr->SipReportSent != 2) )
	{
		printk("Do not send SIP report %u (%u)", Reason,
			HashPtr->SipReportSent );
		return;
	}
	
	WiFiIf = get_radio_id(Intrf->name);
	
	memset( &SipCallEnd, 0, sizeof( SipCallEnd ) );
	SipCallEnd.SessionId = HashPtr->SipSessionId;
	
	memcpy( Tmp, &HashPtr->VlanMacHigh, 4 );
	SipCallEnd.CltMac[0] = Tmp[1];
	SipCallEnd.CltMac[1] = Tmp[0];
	memcpy( Tmp, &HashPtr->MacLow, 4 );
	SipCallEnd.CltMac[2] = Tmp[3];
	SipCallEnd.CltMac[3] = Tmp[2];
	SipCallEnd.CltMac[4] = Tmp[1];
	SipCallEnd.CltMac[5] = Tmp[0];
	
	SipCallEnd.SipReportIdx = HashPtr->SipReportIndex;
	SipCallEnd.Reason = Reason;
	SipCallEnd.CltMos = CltMos;
	SipCallEnd.WifiIf = 0;
	memcpy( SipCallEnd.Codecs, HashPtr->RtpPayload, 4 );
	
        memcpy(&SipCallEnd.Url[0], &HashPtr->Url[0], 50);
	//TODO: rtp_get_stats
	
	capture_put((unsigned char *)&SipCallEnd, 0,
		    sizeof( struct sip_call_end ), PKT_TYPE_SIP_CALL_REPORT, 
		    0, NULL, WiFiSessionId, &WiFiIf, SipCallEnd.CltMac, 0);
	HashPtr->SipReportSent = Reason+1;
	HashPtr->VoIPflags = 0;
}

static void voip_generate_start_event(struct mac_entry * HashPtr,
                                         uint64_t SessionId, int WiFiIf)
{
	struct sip_call_start SipCallStart;
	int i;
	unsigned char Tmp[4];
	uint64_t WiFiSessionId = 0;
	
	memset( &SipCallStart, 0, sizeof( SipCallStart ) );
	SipCallStart.SessionId = SessionId;

	if( HashPtr->SipState == 10 )
	{
		for( i=0; i<MAX_VOIP_CODECS; i++ )
		{
			if((VoipCodecs[i].MacHigh == HashPtr->VlanMacHigh) &&
				(VoipCodecs[i].MacLow == HashPtr->MacLow) )
			{
				memcpy( &(SipCallStart.Url[0]), &(VoipCodecs[i].Url[0]), 50 );
				memcpy( &(HashPtr->Url[0]), &(VoipCodecs[i].Url[0]), 50 );
				memcpy( &(SipCallStart.Codecs[0][0]),
					&(VoipCodecs[i].Codecs[0][0]), 450 );
	
				/* Clean-up entry */
				memset( &(VoipCodecs[i].MacHigh), 0, sizeof( struct voip_codec ) );
				goto send_event;
			}
		}
	}
	else
	{
    if( HashPtr->SipState == VOIP_MARK_SKYPE ) {
        strcpy( &(HashPtr->Url[0]), "skype" );
        strcpy( &(SipCallStart.Url[0]), "skype" );
    }
    if( HashPtr->SipState == VOIP_MARK_GOTOMEETING ) {
        strcpy( &(HashPtr->Url[0]), "gotomeeting" );
        strcpy( &(SipCallStart.Url[0]), "gotomeeting" );
    }
    if( HashPtr->SipState == VOIP_MARK_ZOOMMEETING ) {
        strcpy( &(HashPtr->Url[0]), "zoommeeting" );
        strcpy( &(SipCallStart.Url[0]), "zoommeeting" );
    }
}

send_event:

    memcpy( Tmp, &(HashPtr->VlanMacHigh), 4 );
    SipCallStart.CltMac[0] = Tmp[1];
    SipCallStart.CltMac[1] = Tmp[0];
    memcpy( Tmp, &(HashPtr->MacLow), 4 );
    SipCallStart.CltMac[2] = Tmp[3];
    SipCallStart.CltMac[3] = Tmp[2];
    SipCallStart.CltMac[4] = Tmp[1];
    SipCallStart.CltMac[5] = Tmp[0];

// TODO: rtp_get_stats

    voip_increment_counter(WiFiIf);
    capture_put((unsigned char *)&SipCallStart, 0,
                sizeof( struct sip_call_start ), PKT_TYPE_SIP_CALL_START, 
                0, NULL, WiFiSessionId, &WiFiIf, SipCallStart.CltMac, 0);
}

/* Generate call end event to userspace */
void voip_generate_end_event(struct mac_entry * HashPtr, unsigned int Reason)
{
	struct voip_session VoipSession;
	struct sip_call_end SipCallEnd;
	unsigned char Tmp[4];
	uint64_t WiFiSessionId = 0;
	int WiFiIf = 0;
	
	WiFiIf = get_radio_id(HashPtr->port->name);
	
	memset( &SipCallEnd, 0, sizeof( SipCallEnd ) );
	SipCallEnd.SessionId = HashPtr->SipSessionId;
	
	memcpy( Tmp, &(HashPtr->VlanMacHigh), 4 );
	SipCallEnd.CltMac[0] = Tmp[1];
	SipCallEnd.CltMac[1] = Tmp[0];
	memcpy( Tmp, &(HashPtr->MacLow), 4 );
	SipCallEnd.CltMac[2] = Tmp[3];
	SipCallEnd.CltMac[3] = Tmp[2];
	SipCallEnd.CltMac[4] = Tmp[1];
	SipCallEnd.CltMac[5] = Tmp[0];
	
	SipCallEnd.CltMos = 0;
	SipCallEnd.Reason = Reason;
	SipCallEnd.SipReportIdx = HashPtr->SipReportIndex;
	memcpy( SipCallEnd.Codecs, HashPtr->RtpPayload, 4 );
	if( HashPtr->VideoIsDetected )
		SipCallEnd.VideoCodec = 1;

        memcpy(&SipCallEnd.Url[0], &HashPtr->Url[0], 50);

	// TODO:rtp_get_stats

	/* Notify other APs that this call is ended */
	VoipSession.SipSessionId = 0;
	VoipSession.CltMacHigh   = htonl( HashPtr->VlanMacHigh );
	VoipSession.CltMacLow    = htonl( HashPtr->MacLow );
	VoipSession.SipPort      = 0;
	VoipSession.SipState     = 0;
	VoipSession.RtpPortsFrom = 0;
	VoipSession.RtpPortsTo   = 0;
	VoipSession.RtpIpFrom    = 0;
	VoipSession.RtpIpTo      = 0;
	VoipSession.RtpVideoPortsFrom = 0;
	VoipSession.RtpVideoPortsTo   = 0;

	// TODO: iac_send_message

	/* Generate event to the cloud */
	capture_put((unsigned char *)&SipCallEnd, 0,
		    sizeof( struct sip_call_end ), PKT_TYPE_SIP_CALL_STOP, 0,
		    NULL, WiFiSessionId, &WiFiIf, SipCallEnd.CltMac, 0);

	HashPtr->VoIPflags = 0;
}

/*
 * voip_sip_packet_analyze: Analyze SIP protocol packets
 */
int voip_sip_packet_analyze(unsigned char * Message, unsigned char * EndPtr,
                               struct mac_entry * HashPtr, unsigned int FromClt)
{
    int i, Method = -1, IsOK = 0, Exist;
    unsigned char * CurPtr, * ValPtr;
    unsigned int MyRtpPort = 0;

    if( HashPtr == NULL )
        return( -1 );

    for( i=0; i<8; i++ )
    {
        if( memcmp( Message, SipMethod[i], SipLen[i] ) == 0 )
        {
            Method = i;
            break;
        }
    }

    if( Method == -1 )
        return( -1 );

    /* Skip start-line */
    CurPtr = strchr( Message, 0xd );
    if( CurPtr == NULL )
    {
        printk("SIP received (%s) %u from=%u - wrong packet\n", SipMethod[Method], HashPtr->SipState, FromClt );
        return( -1 );
    }
    *CurPtr = 0;
    printk("SIP received (%s) %u from=%u (%s)\n", SipMethod[Method], HashPtr->SipState, FromClt, Message );
    if( (Method == 6) && strstr( Message, "200" ) ) {
        IsOK = 1;
    }
    *CurPtr = 0xd;

    if( (Method == 4) && (HashPtr->SipState >= 8) )
    {
        /* Call was set up and we get BYE from the client 
           - start tracing closure */
        HashPtr->SipState = Method;
	
        return( Method );
    }

    if( IsOK && (HashPtr->SipState == 4) )
    {
        HashPtr->SipCallEndTimer = 5;  /*Start counting for PUBLISH packet rx*/
        printk("Call closed by (%s)\n", (FromClt ? "head end" : "client") );
        voip_generate_end_event( HashPtr, SIP_CALL_ENDED_BYE );

        HashPtr->SipState = 0;
        return( Method );
    }

    if( CurPtr && (CurPtr < EndPtr) && (*(CurPtr+1) == 0xa) )
    {
        Message = CurPtr + 2;
        while( Message <= EndPtr )
        {
            CurPtr = strchr( Message, 0xd );
            if( CurPtr && (CurPtr < EndPtr) && (*(CurPtr+1) == 0xa) )
            {
                if(HashPtr->SipCallEndTimer && (Method == 7) &&
                   (memcmp( Message, "QualityEst:", 11 ) == 0))
                {
                    unsigned int CltMos;
                    
                    *CurPtr = 0;
                    CltMos = voip_get_client_mos( Message+11 );
                    *CurPtr = 0xd;
                    voip_generate_report_event(HashPtr, HashPtr->port,
                                                  CltMos, 2);
                    voip_clean_session( HashPtr );
                }

                if(((HashPtr->SipState == 1) || (HashPtr->SipState == 8) ||
                   (HashPtr->SipState == 9)) &&
                    (memcmp( Message, "a=rtpmap:", 9 ) == 0))
                {
                    *CurPtr = 0;
                    voip_sip_add_codec(HashPtr->VlanMacHigh,
                                          HashPtr->MacLow, Message+9);
                    *CurPtr = 0xd;
                }

                if( memcmp( Message, SipString[0], 2 ) == 0 )
                {
                    *CurPtr = 0;
                    if( Method == 1 )
                    {
                        ValPtr = strstr( Message, "audio" );
                        if( ValPtr )
                        {
                            Exist = 0;
                            ValPtr += 6;
                            sscanf( ValPtr, "%u", &MyRtpPort );
                            if( FromClt )
                            {
                                printk("Audio ports from %x %x (%u)\n", HashPtr->RtpPortsFrom, htonl( MyRtpPort ), HashPtr->SipState );
                                if(((HashPtr->RtpPortsFrom & 0xFFFF0000) == 
                                   htonl(MyRtpPort)) && (HashPtr->SipState > 8))
                                    Exist = 1;
                                else
                                {
                                    HashPtr->RtpPortsFrom = htonl( MyRtpPort );
                                    HashPtr->RtpPortsTo = htonl(MyRtpPort << 16);
                                }
                            }
                            else
                            {
                                printk("Audio ports to %x %x (%u)\n", HashPtr->RtpPortsTo, htonl( MyRtpPort ), HashPtr->SipState );
                                if(((HashPtr->RtpPortsTo & 0xFFFF0000) ==
                                    htonl( MyRtpPort )) && 
                                    (HashPtr->SipState > 8))
                                    Exist = 1;
                                else
                                {
                                    HashPtr->RtpPortsTo = htonl( MyRtpPort );
                                    HashPtr->RtpPortsFrom =
                                                    htonl( MyRtpPort << 16 );
                                }
                            }
                            if( !Exist ) {
                                HashPtr->SipState = Method;
				}
                            else
                                printk("Session audio port is already set %u\n", HashPtr->SipState );
                        }

                        ValPtr = strstr( Message, "video" );
                        if( ValPtr )
                        {
                            Exist = 0;
                            ValPtr += 6;
                            sscanf( ValPtr, "%u", &MyRtpPort );
                            if( FromClt )
                            {
                                printk("Video ports from %x %x (%u)\n", HashPtr->RtpVideoPortsFrom, htonl( MyRtpPort ), HashPtr->SipState );
                                if(((HashPtr->RtpVideoPortsFrom & 0xFFFF0000) ==
                                htonl( MyRtpPort )) && (HashPtr->SipState > 8))
                                    Exist = 1;
                                else
                                {
                                    HashPtr->RtpVideoPortsFrom = htonl( MyRtpPort );
                                    HashPtr->RtpVideoPortsTo = htonl( MyRtpPort << 16 );
                                }
                            }
				if( !Exist ) {
	                                HashPtr->SipState = Method;
				}
                            else
                                printk("Session video port is already set %u\n", HashPtr->SipState );
                        }
                    }
                    if((Method == 6) && 
                        ((HashPtr->SipState == 1) || (HashPtr->SipState == 8) ||
                        (HashPtr->SipState == 9)))
                    {
                        ValPtr = strstr( Message, "audio" );
                        if( ValPtr )
                        {
                            printk("Ports (%u)\n", HashPtr->SipState );
                            ValPtr += 6;
                            sscanf( ValPtr, "%u", &MyRtpPort );
                            if( FromClt )
                            {
                                HashPtr->RtpPortsTo |= htonl( MyRtpPort << 16 );
                                HashPtr->RtpPortsFrom |= htonl( MyRtpPort );
                            }
                            else
                            {
                                HashPtr->RtpPortsFrom |= htonl(MyRtpPort << 16);
                                HashPtr->RtpPortsTo |= htonl( MyRtpPort );
                            }

                            if( HashPtr->SipState != 9 )
                            {
                                if( HashPtr->SipState == 8 )
                                    HashPtr->SipState = 9;
                                else
                                    HashPtr->SipState = 8;
                            }

                            printk("Call set audio ports %x %x (%u)\n", 
                                             ntohl( HashPtr->RtpPortsFrom ), ntohl( HashPtr->RtpPortsTo ), HashPtr->SipState );
                        }
                    }

                    if( (Method == 6) && 
                        ((HashPtr->SipState == 1) || (HashPtr->SipState == 9)))
                    {
                        ValPtr = strstr( Message, "video" );
                        if( ValPtr )
                        {
                            printk("Ports (%u)\n", HashPtr->SipState );
                            ValPtr += 6;
                            sscanf( ValPtr, "%u", &MyRtpPort );
                            if( FromClt )
                            {
                                HashPtr->RtpVideoPortsTo |= htonl( MyRtpPort << 16 );
                                HashPtr->RtpVideoPortsFrom |= htonl( MyRtpPort );
                            }
                            else
                            {
                                HashPtr->RtpVideoPortsFrom |= htonl( MyRtpPort << 16 );
                                HashPtr->RtpVideoPortsTo |= htonl( MyRtpPort );
                            }
                            printk("Call set video ports %x %x (%u)\n", 
                                             ntohl( HashPtr->RtpVideoPortsFrom ), ntohl( HashPtr->RtpVideoPortsTo ), HashPtr->SipState );
                        }
                    }
                    *CurPtr = 0xd;
                }
                if( memcmp( Message, SipString[1], 2 ) == 0 )
                {
                    *CurPtr = 0;
                    if( Method == 1 )
                    {
                        ValPtr = strstr( Message, "IN IP4" );
                        if( ValPtr )
                        {
                            uint32_t InIp;

                            Exist = 0;
                            ValPtr += 6;
                            InIp = htonl( serv_string_to_ip( ValPtr ) );
                            if( FromClt )
                            {
                                if( (HashPtr->RtpIpFrom == InIp) && (HashPtr->SipState > 8) )
                                    Exist = 1;
                                else
                                    HashPtr->RtpIpFrom = InIp;
                            }
                            else
                            {
                                if( (HashPtr->RtpIpTo == InIp) && (HashPtr->SipState > 8) )
                                    Exist = 1;
                                else
                                    HashPtr->RtpIpTo = InIp;
                            }

                            if( !Exist ) {
                                HashPtr->SipState = Method;
                            }
                            else {
                                printk("Session IP is already set %u\n", HashPtr->SipState );
                            }
                        }
                    }
                    if( (Method == 6) && 
                        ((HashPtr->SipState == 1) || (HashPtr->SipState == 8) || (HashPtr->SipState == 9)) )
                    {
                        ValPtr = strstr( Message, "IN IP4" );
                        if( ValPtr )
                        {
                            printk("IP (%u)\n", HashPtr->SipState );
                            ValPtr += 6;
                            if( FromClt )
                                HashPtr->RtpIpFrom = htonl( serv_string_to_ip( ValPtr ) );
                            else
                                HashPtr->RtpIpTo = htonl( serv_string_to_ip( ValPtr ) );

                            if( HashPtr->SipState != 9 )
                            {
                                if( HashPtr->SipState == 8 )
                                    HashPtr->SipState = 9;
                                else
                                    HashPtr->SipState = 8;
                            }

                            printk("Call set IP %x %x (%u)\n", 
                                             HashPtr->RtpIpFrom, HashPtr->RtpIpTo, HashPtr->SipState );
                        }
                    }
                    *CurPtr = 0xd;
                }
                if( memcmp( Message, SipString[2], 2 ) == 0 )
                {
                    *CurPtr = 0;
                    *CurPtr = 0xd;
                }
                if( (memcmp( Message, SipString[3], 2 ) == 0) || (memcmp( Message, SipString[4], 3 ) == 0) )
                {
                    char * AtPtr, * SemicolonPtr;

                    *CurPtr = 0;
                    AtPtr = strstr( Message, "@" );
                    if( AtPtr )
                    {
                        SemicolonPtr = strstr( Message, ";" );
                        if( SemicolonPtr && (SemicolonPtr > AtPtr) )
                        {
                            int i;

                            for( i=0; i<MAX_VOIP_CODECS; i++ )
                            {
                                if( (VoipCodecs[i].MacHigh == HashPtr->VlanMacHigh) && (VoipCodecs[i].MacLow == HashPtr->MacLow) )
                                {
                                    unsigned int ll = (unsigned int)SemicolonPtr - (unsigned int)AtPtr - 1;

                                    if( ll > 49 )
                                        ll = 49;
                                    memcpy( VoipCodecs[i].Url, AtPtr+1, ll );
                                    VoipCodecs[i].Url[ll] = 0;
                                }
                            }
                        }
                    }
                    *CurPtr = 0xd;
                }
                Message = CurPtr + 2;
            }
            else
                break;
        }
    }

    if( IsOK && (HashPtr->SipState == 9) )
    {
        struct clt_entry * hashEntry = clt_hash_search( HashPtr->VlanMacHigh, HashPtr->MacLow );
        struct voip_session VoipSession;
        int WiFiIf = 0;

        HashPtr->SipSessionId = glue_get_time_msec( );
        VoipSession.SipSessionId = HashPtr->SipSessionId;
        VoipSession.CltMacHigh   = htonl( HashPtr->VlanMacHigh );
        VoipSession.CltMacLow    = htonl( HashPtr->MacLow );
        VoipSession.SipPort      = htonl( HashPtr->SipPort );
        VoipSession.SipState     = htonl( 10 );
        VoipSession.RtpPortsFrom = HashPtr->RtpPortsFrom;
        VoipSession.RtpPortsTo   = HashPtr->RtpPortsTo;
        VoipSession.RtpIpFrom    = HashPtr->RtpIpFrom;
        VoipSession.RtpIpTo      = HashPtr->RtpIpTo;
        VoipSession.RtpVideoPortsFrom = HashPtr->RtpVideoPortsFrom;
        VoipSession.RtpVideoPortsTo   = HashPtr->RtpVideoPortsTo;
        HashPtr->SipState = 10;

        if( hashEntry )
        {
            /* Set local CLT hash */
            hashEntry->SipSessionId      = HashPtr->SipSessionId;
            hashEntry->SipPort           = HashPtr->SipPort;
            hashEntry->SipState          = 10;
            hashEntry->RtpPortsFrom      = HashPtr->RtpPortsFrom;
            hashEntry->RtpPortsTo        = HashPtr->RtpPortsTo;
            hashEntry->RtpVideoPortsFrom = HashPtr->RtpVideoPortsFrom;
            hashEntry->RtpVideoPortsTo   = HashPtr->RtpVideoPortsTo;
            hashEntry->RtpIpFrom         = HashPtr->RtpIpFrom;
            hashEntry->RtpIpTo           = HashPtr->RtpIpTo;
        }

	WiFiIf = get_radio_id(HashPtr->port->name);
/*
        iac_send_message
*/

        voip_generate_start_event( HashPtr, VoipSession.SipSessionId, WiFiIf );
        HashPtr->SipReportSent = 2;
        HashPtr->SipCallEndTimer = 0;
        HashPtr->SipCallActiveTimer = 0;
        HashPtr->SipReportIndex = 0;
    }
    return( Method );
}


/*
 * set_mac_addr: Extract MAC from skb and populate pkt
 */
static void set_mac_addr(struct packet *pkt) 
{
	uint32_t vlan;
	uint32_t temp;
	pkt->proto = ntohl( *((uint32_t *)(pkt->l2ptr + ETH_TYPE_POS)) );
	vlan = pkt->proto & 0xFFFF0FFF;

	if( (vlan & 0xFFFF0000) == 0x81000000 )
	{
		pkt->proto = ntohl( *((uint32_t *)(pkt->l2ptr +
						   ETH_TYPE_POS_VLAN)) );
		pkt->l3ptr = pkt->l2ptr + ETH_L3_DATA_VLAN;
	}
	else
	{
		pkt->l3ptr = pkt->l2ptr + ETH_L3_DATA;
	}
	
	vlan = vlan << 16;
	pkt->DstMacLow = ntohl( *((uint32_t *)(pkt->l2ptr+2)) );
	temp = ntohs( *((uint16_t *)pkt->l2ptr) );
	pkt->DstMacHigh = vlan + temp;
	
	pkt->SrcMacLow = ntohl( *((uint32_t *)(pkt->l2ptr+8)) );
	temp = ntohs( *((uint16_t *)(pkt->l2ptr+6)) );
	pkt->SrcMacHigh = vlan + temp;
}


/* Voip Hueristic detect */
void voip_udp_heuristic_flow_detect(struct mac_entry * hash_dst,
                                       uint32_t SrcIp, uint32_t UdpPorts,
                                       unsigned int Len)
{
    int iflow;
    if( (hash_dst->SipState > 0) && (hash_dst->SipState < 10) )
    {
        return;     /* Do not detect flow if in the middle of SIP call setup */
    }

    if( hash_dst->SipCallActiveTimer ) {
        return;     /* Do not detectr flow if another flow is active */
    }

#if 0
    if( Len == 0xFFFFFFFF )
    {
        /* To detect bi-directional flows */
        for( iflow = 0; iflow<MAX_UDP_FLOWS; iflow++ )
        {
            if( (SrcIp == hash_dst->UdpFlows[iflow].SrcIp) && 
                (((UdpPorts & 0xFFFF0000) >> 16) == (hash_dst->UdpFlows[iflow].Ports & 0xFFFF)) )
            {
                hash_dst->UdpFlows[iflow].LenInSec |= 0x01000000;   /* Mark flow as bi-directional */
                printk("flow %i (%x %x) bi-dir %x", iflow, 
                                 hash_dst->UdpFlows[iflow].SrcIp, hash_dst->UdpFlows[iflow].Ports, hash_dst->UdpFlows[iflow].LenInSec );
                break;
            }
        }
        return;
    }
#endif

    if( (UdpPorts & 0xFFFF0000) == 0x073D0000 ) {
        return;       /* Citrix gotomeeting port for video stream */
    }
    if( (UdpPorts & 0xFFFF0000) == 0x01BB0000 ) {
        return;                                 /* QUIC protocol */
    }
    for( iflow = 0; iflow<MAX_UDP_FLOWS; iflow++ )
    {
        if( (SrcIp == hash_dst->UdpFlows[iflow].SrcIp) &&
            (UdpPorts == hash_dst->UdpFlows[iflow].Ports) )
        {
            if( Len < 400 )
            {
                hash_dst->UdpFlows[iflow].LenInSec += Len;
                if( ++(hash_dst->UdpFlows[iflow].Count) > 65 )
                {
                    if( hash_dst->UdpFlows[iflow].IsVoip )
                        printk(KERN_DEBUG "flow %i (%x %x) got too many pkt %u",
                               iflow, hash_dst->UdpFlows[iflow].SrcIp,
                               hash_dst->UdpFlows[iflow].Ports,
                               hash_dst->UdpFlows[iflow].Count);
                    hash_dst->UdpFlows[iflow].IsVoip = 0;
                }
            }

            hash_dst->UdpFlows[iflow].Age = 3;
            break;
        }
        if( hash_dst->UdpFlows[iflow].Age == 0 )
        {
            printk(KERN_DEBUG "flow %i (%x %x) started", iflow, SrcIp, UdpPorts );
            hash_dst->UdpFlows[iflow].SrcIp = SrcIp;
            hash_dst->UdpFlows[iflow].Ports = UdpPorts;
            hash_dst->UdpFlows[iflow].Age = 3;
            hash_dst->UdpFlows[iflow].Count = 1;
            hash_dst->UdpFlows[iflow].LenInSec = Len;
            break;
        }
    }
}


void voip_heuristic_flow_call_start(struct mac_entry * hash_ptr,
                                       int flow_index, int IsTcp)
{
	struct clt_entry * hashEntry = clt_hash_search( hash_ptr->VlanMacHigh,
                                                hash_ptr->MacLow );
	struct voip_session VoipSession;
	int WiFiIf = 0, CalType = VOIP_MARK_SKYPE;

	WiFiIf = get_radio_id(hash_ptr->port->name);

	if( IsTcp )
	{
		hash_ptr->RtpIpTo = hash_ptr->TcpFlows[flow_index].SrcIp;
		hash_ptr->RtpPortsFrom =
				htonl( hash_ptr->TcpFlows[flow_index].Ports );
	}
	else
	{
		uint32_t SrcPort = hash_ptr->UdpFlows[flow_index].Ports & 
				   0xFFFF0000;

		hash_ptr->RtpIpTo = hash_ptr->UdpFlows[flow_index].SrcIp;
		hash_ptr->RtpPortsFrom = htonl( hash_ptr->UdpFlows[flow_index].Ports );
		if( hash_ptr->RtpIpCand1 )
        	{
			uint16_t PortToCmp = (uint16_t)(hash_ptr->RtpPortsFrom &
							0xFFFF);

			printk(KERN_DEBUG "gotomeeting flow ip1=%x ip2=%x ip=%x port=%x (%u %u)",
                             hash_ptr->RtpIpCand1, hash_ptr->RtpIpCand2,
                             hash_ptr->RtpIpTo, hash_ptr->RtpPortsFrom, 
                             hash_ptr->RtpPortCand1, hash_ptr->RtpPortCand2 );
			 if( ((hash_ptr->RtpIpTo == hash_ptr->RtpIpCand1) &&
			(PortToCmp == ntohs( hash_ptr->RtpPortCand1 ))) ||
			((hash_ptr->RtpIpTo == hash_ptr->RtpIpCand2) &&
			(PortToCmp == ntohs( hash_ptr->RtpPortCand2 ))) )
			{
				printk("gotomeeting flow detected" );
				CalType = VOIP_MARK_GOTOMEETING;
			}
		}

		if( SrcPort == 0x20080000 )
		{
			printk(KERN_DEBUG "gotomeeting flow detected by port 8200" );
			CalType = VOIP_MARK_GOTOMEETING;
		}

		if( (SrcPort == 0x22610000) || (SrcPort == 0x22620000) )
		{
			printk(KERN_DEBUG "zoom flow detected by port 8801/8802" );
			CalType = VOIP_MARK_ZOOMMEETING;
		}
	}

	hash_ptr->SipState = CalType;
	hash_ptr->SipSessionId = glue_get_time_msec( );
	
	VoipSession.SipSessionId = hash_ptr->SipSessionId;
	VoipSession.CltMacHigh   = htonl( hash_ptr->VlanMacHigh );
	VoipSession.CltMacLow    = htonl( hash_ptr->MacLow );
	VoipSession.SipPort      = htonl( hash_ptr->SipPort );
	VoipSession.SipState     = htonl( CalType );
	VoipSession.RtpPortsFrom = hash_ptr->RtpPortsFrom;
	VoipSession.RtpPortsTo   = hash_ptr->RtpPortsTo;
	VoipSession.RtpIpFrom    = hash_ptr->RtpIpFrom;
	VoipSession.RtpIpTo      = hash_ptr->RtpIpTo;
	VoipSession.RtpVideoPortsFrom = hash_ptr->RtpVideoPortsFrom;
	VoipSession.RtpVideoPortsTo   = hash_ptr->RtpVideoPortsTo;
	//TODO:iac_send_message

	if( hashEntry )
	{
		/* Set local CLT hash */
		hashEntry->SipSessionId      = hash_ptr->SipSessionId;
		hashEntry->SipPort           = hash_ptr->SipPort;
		hashEntry->SipState          = CalType;
		hashEntry->RtpPortsFrom      = hash_ptr->RtpPortsFrom;
		hashEntry->RtpPortsTo        = hash_ptr->RtpPortsTo;
		hashEntry->RtpVideoPortsFrom = hash_ptr->RtpVideoPortsFrom;
		hashEntry->RtpVideoPortsTo   = hash_ptr->RtpVideoPortsTo;
		hashEntry->RtpIpFrom         = hash_ptr->RtpIpFrom;
		hashEntry->RtpIpTo           = hash_ptr->RtpIpTo;
	}

	voip_generate_start_event( hash_ptr, hash_ptr->SipSessionId, WiFiIf );
	hash_ptr->SipReportIndex = 0;
	hash_ptr->SipCallActiveTimer = HEUR_CALL_INACTIVITY_TIME;
	hash_ptr->SipReportSent = 2;
}



int process_packet( struct packet * pkt )
{
	struct mac_entry * src_hash_ptr;
	struct mac_entry * dst_hash_ptr;

	/* extract mac, l3ptr and protocol from skb and set in pkt */
	set_mac_addr(pkt);
	
	/* find mac address in hash table and get hash pointer */
	mac_hash_search(pkt, pkt->SrcMacHigh, pkt->SrcMacLow);
	src_hash_ptr = pkt->HashDst;

	/* Process IP packets */
	pkt->proto &= 0xFFFF0000;
	if (pkt->proto == L3_PROTO_IP) {
		pkt->l4ptr = pkt->l3ptr + (*pkt->l3ptr & 0x0F)*4;

		/* if IP type is UDP(17) or TCP(6) */
		if( (*(pkt->l3ptr+9) == 17) || (*(pkt->l3ptr+9) == 6) ) {
			uint32_t UdpPorts = ntohl( *((uint32_t *)pkt->l4ptr) );
			uint32_t Sport, Dport = UdpPorts & 0xFFFF;
			unsigned int Offs, Len;
			uint32_t SrcIp = *((uint32_t *)(pkt->l3ptr + 12));

			if( (*(pkt->l3ptr+9) == 17) ) {
				uint16_t UdpLen = ntohs( *((uint16_t *)(pkt->l4ptr+4)) );

				Offs = 8;
				Len = UdpLen;

				if( src_hash_ptr && src_hash_ptr->RtpIpTo && (*((uint32_t *)(pkt->l3ptr + 16)) == src_hash_ptr->RtpIpTo) )
				{
					if( src_hash_ptr->RtpPortsTo && ((*((uint32_t *)pkt->l4ptr) == src_hash_ptr->RtpPortsTo)) )
					{
						unsigned int RtpHeaderLen = 12 + (*(pkt->l4ptr+8) & 0x0F) * 4;

						src_hash_ptr->RtpPayload[0] = *(pkt->l4ptr+9);
						src_hash_ptr->RtpPayload[1] = *(pkt->l4ptr+RtpHeaderLen+8);
						src_hash_ptr->RtpPayload[2] = *(pkt->l4ptr+RtpHeaderLen+12);
						src_hash_ptr->SipCallActiveTimer = SIP_CALL_INACTIVITY_TIME;
#if 0 //dscp stuff
						if( *(pkt->l3ptr + 1) == 0 )
							serv_set_dscp( pkt, 0xB8 );
#endif
					}

				}

			}
			else
			{
				Offs = *(pkt->l4ptr+12) / 4;
				Len = pkt->datalen - 14 - 20;

// Retrieve User Agent
#if 0
				if( src_hash_ptr && ( src_hash_ptr->ua_flag == 0 ) && ( port_is_http( Dport )) ) {
					http_ua_packet_analyze( pkt->l4ptr+Offs, src_hash_ptr );
				}
#endif
			}
			/*Tx SIP*/
			if( voip_port_is_sip( Dport ) ) {
				int IsSip = 0;
				IsSip = voip_sip_packet_analyze(pkt->l4ptr+Offs, pkt->l4ptr+Len, src_hash_ptr, 1 );

				Sport = (UdpPorts & 0xFFFF0000) >> 16;
				if( (IsSip == 0) && src_hash_ptr )
					src_hash_ptr->SipPort = Sport;
#if 0
				if( IsSip != -1 )
					serv_set_dscp( PktPtr, 0xB8 );
#endif
			}
/*Rx SIP*/

                        mac_hash_search(pkt, pkt->DstMacHigh, pkt->DstMacLow);
			dst_hash_ptr = pkt->HashDst;

			if(voip_port_is_sip( Sport ) ||
			  (dst_hash_ptr && (Dport == dst_hash_ptr->SipPort)) )
			{
				voip_sip_packet_analyze( pkt->l4ptr+Offs, pkt->l4ptr+Len, dst_hash_ptr, 0 );
			}

			if(dst_hash_ptr && (SrcIp == dst_hash_ptr->RtpIpTo))
			{
                                    /* Handling VoIP flows detected by call setup interception */
				if( *((uint32_t *)pkt->l4ptr) == dst_hash_ptr->RtpPortsFrom )
				{
					if( dst_hash_ptr->SipState >= VOIP_MARK_SKYPE )
						dst_hash_ptr->SipCallActiveTimer = HEUR_CALL_INACTIVITY_TIME;
					else
						dst_hash_ptr->SipCallActiveTimer = SIP_CALL_INACTIVITY_TIME;
					dst_hash_ptr->VoIPflags |= VOIP_FLAG_RTP_TO_CLT;
//					serv_set_dscp( PktPtr, 0xB8 );
					
				}
				if( (*((uint32_t *)pkt->l4ptr) == dst_hash_ptr->RtpVideoPortsFrom) )
				{
					dst_hash_ptr->SipCallActiveTimer = SIP_CALL_INACTIVITY_TIME;
					dst_hash_ptr->VideoIsDetected = 1;
//					serv_set_dscp( PktPtr, 0xA0 );
				}
			}
			else {
				if( (*(pkt->l3ptr+9) == 17) ) {
					if (dst_hash_ptr) {
						voip_udp_heuristic_flow_detect( dst_hash_ptr, SrcIp, UdpPorts, Len );
                                }
                            }
			}
		}
	}


	src_hash_ptr = mac_hash_insert( pkt->SrcMacHigh, pkt->SrcMacLow, 22/*skb->len+14*/, pkt->src_dev);
return 0;
}

/* 
 * ucc_nf_hook: Hook function called from NF bridge preroute 
 * @priv: private data
 * @skb: Received packet on bridge
 * @state: hook state
 */
static unsigned int ucc_nf_hook(void *priv, struct sk_buff *skb,
                                const struct nf_hook_state *state)
{
	struct packet pkt;

	/* No need to process LAN traffic */
	if (!strcmp(skb->dev->name, "eth0"))
		return NF_ACCEPT;

	memset(&pkt, 0, sizeof(struct packet));

	/* skb push till L2 pointer */
	skb_push(skb, 14);
	
	/* Fill up pkt */
	pkt.l2ptr = skb->data;
	pkt.pkt = (void *)skb;
	pkt.offset = 0;
	pkt.datalen = skb->len;
	pkt.src_dev = skb->dev;
	process_packet(&pkt);
	
	/* Return the packet to original state */
	skb_pull(skb, 14);
	
	/*packet goes on*/
	return NF_ACCEPT;
}

/* Netfilter hook */
static struct nf_hook_ops nfho;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
#define KMEM_CACHE_CREATE( name, size, align, flags, ctor, cb ) kmem_cache_create( (name), (size), (align), (flags), (ctor), (cb) )
#define DEV_GET_BY_NAME( name ) dev_get_by_name( name )
#define NETLINK_KERNEL_CREATE( unit, groups, input, module ) netlink_kernel_create( (unit), (groups), (input), (module) )
#define skb_mac_header( skb ) (skb)->mac.raw
#define skb_reset_mac_header( skb ) ((skb)->mac.raw = (skb)->data)
#define skb_set_mac_header( skb, offset ) ((skb)->mac.raw = (skb)->data+(offset))
#define skb_network_header( skb ) (skb)->nh.raw
#define skb_reset_network_header( skb ) ((skb)->nh.raw = (skb)->data)
#define skb_set_network_header( skb, offset ) ((skb)->nh.raw = (skb)->data+(offset))
#define skb_set_nh_header_from_mk( skb, offset ) ((skb)->nh.raw = (skb)->mac.raw+(offset))
#define skb_dst_set( skb, ptr ) ((skb)->dst = (ptr))
#define skb_dst( skb ) ((skb)->dst)
#define skb_rtable( skb ) (struct rtable *)((skb)->dst)
#define qdisc_dev( qdisc ) (qdisc)->dev

static inline void skb_dst_drop( struct sk_buff * skb )
{
	if( skb->dst )
		dst_release( skb->dst );
	skb->dst = NULL;
}
#else
#define KMEM_CACHE_CREATE( name, size, align, flags, ctor, cb ) kmem_cache_create( (name), (size), (align), (flags), (ctor) )
#define DEV_GET_BY_NAME( name ) dev_get_by_name( &init_net, name )
#define NETLINK_KERNEL_CREATE( unit, groups, input, module ) netlink_kernel_create( &init_net, (unit), (groups), (input), NULL, (module) )
#define skb_set_nh_header_from_mk( skb, offset ) ((skb)->network_header = (skb)->mac_header+(offset))
#endif

/* Netlink definitions */
static struct genl_family genl_ucc_family;

static const struct genl_multicast_group genl_ucc_mcgrps[] = {
	[GENL_UCC_MCGRP0] = { .name = GENL_UCC_MCGRP0_NAME, },
	[GENL_UCC_MCGRP1] = { .name = GENL_UCC_MCGRP1_NAME, },
	[GENL_UCC_MCGRP2] = { .name = GENL_UCC_MCGRP2_NAME, },
};

static int genl_ucc_rx_msg(struct sk_buff* skb, struct genl_info* info)
{
	if (!info->attrs[GENL_UCC_ATTR_MSG]) {
		printk(KERN_ERR "empty message from %d!!\n",
			info->snd_portid);
		printk(KERN_ERR "%p\n", info->attrs[GENL_UCC_ATTR_MSG]);
		return -EINVAL;
	}

	printk(KERN_NOTICE "%u says %s \n", info->snd_portid,
		(char*)nla_data(info->attrs[GENL_UCC_ATTR_MSG]));
	return 0;
}

static const struct genl_ops genl_ucc_ops[] = {
	{
		.cmd = GENL_UCC_C_MSG,
		.policy = genl_ucc_policy,
		.doit = genl_ucc_rx_msg,
		.dumpit = NULL,
	},
};

static struct genl_family genl_ucc_family = {
	.name = GENL_UCC_FAMILY_NAME,
	.version = 1,
	.maxattr = GENL_UCC_ATTR_MAX,
	.netnsok = false,
	.module = THIS_MODULE,
	.ops = genl_ucc_ops,
	.n_ops = ARRAY_SIZE(genl_ucc_ops),
	.mcgrps = genl_ucc_mcgrps,
	.n_mcgrps = ARRAY_SIZE(genl_ucc_mcgrps),
};

/* 
 * send_data: Send data to the userspace using netlink
 * @group: Netlink group id
 * @data: Data to send
 * @len: Length of the data
 */
static void send_data(unsigned int group, void *data, unsigned int len)
{	
	void *hdr;
	int res, flags = GFP_ATOMIC;
	struct sk_buff* skb = genlmsg_new(len, flags);
	
	/* Create new skb to send */
	if (!skb) {
		printk(KERN_ERR "%d: OOM!!", __LINE__);
		return;
	}

	/* Insert UCC header */
	hdr = genlmsg_put(skb, 0, 0, &genl_ucc_family, flags, GENL_UCC_C_MSG);
	if (!hdr) {
		printk(KERN_ERR "%d: Unknown err !", __LINE__);
		goto nlmsg_fail;
	}

	/* Set the data */
	res = nla_put(skb, GENL_UCC_ATTR_MSG, len , data);
	if (res) {
		printk(KERN_ERR "%d: err %d ", __LINE__, res);
		goto nlmsg_fail;
	}

	genlmsg_end(skb, hdr);

	/* mcast the data to the group */
	genlmsg_multicast(&genl_ucc_family, skb, 0, group, flags);
	return;

nlmsg_fail:
	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return;
}

/* Initialize netlink for sending data to userspace */
int netlink_init(void)
{
	return (genl_register_family(&genl_ucc_family));
}

int init_module()
{
	if (netlink_init())
	{
		printk("UCC: netlink failed\n");
	}
	
	/* Initialize Netfilter Bridge Hooks */
	nfho.hook = ucc_nf_hook;
	nfho.hooknum = NF_BR_PRE_ROUTING;
	nfho.pf = PF_BRIDGE;
	nfho.priority = NF_BR_PRI_FIRST;
	
	/* Create mem cache for MAC entries */
	mac_cache    = KMEM_CACHE_CREATE("mac_cache",
	                                     sizeof( struct mac_entry ), 0,
	                                     SLAB_HWCACHE_ALIGN, NULL, NULL );
	clt_cache    = KMEM_CACHE_CREATE("clt_cache",
	                                     sizeof( struct clt_entry ), 0,
	                                     SLAB_HWCACHE_ALIGN, NULL, NULL );
	
	/* Register Netfilter Bridge Hooks */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	nf_register_net_hook(&init_net, &nfho);
#else
	nf_register_hook(&nfho);
#endif

	/* Setup MAC entries aging time and timers */    
	ageing_time = 300;
	setup_timer( &CmdTimer, cmd_timer, (unsigned long)0 );
	mod_timer( &CmdTimer, jiffies + HZ );
	
	return 0;
}

void cleanup_module()
{
	del_timer_sync( &CmdTimer );

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	nf_unregister_net_hook(&init_net, &nfho);
#else
	nf_unregister_hook(&nfho);
#endif
	kmem_cache_destroy( clt_cache );
	kmem_cache_destroy( mac_cache );
	genl_unregister_family(&genl_ucc_family);
}

MODULE_LICENSE("GPL"); 
