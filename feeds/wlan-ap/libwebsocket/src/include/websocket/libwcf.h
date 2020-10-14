/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _LIBBRIDGE_H
#define _LIBBRIDGE_H

#include <sys/socket.h>
#include <net/if.h>
#include <sys/time.h>
//#include <linux/if_bridge.h>

/* defined in net/if.h but that conflicts with linux/if.h... */
extern unsigned int if_nametoindex (const char *__ifname);
extern char *if_indextoname (unsigned int __ifindex, char *__ifname);


#define APC_MAX_NEIGHBORS       100

struct bridge_id
{
	unsigned char prio[2];
	unsigned char addr[6];
};

struct fdb_entry
{
	unsigned int mac_addr[6];
	unsigned int port_no;
	unsigned char is_local;
	struct timeval ageing_timer_value;
};

struct port_info
{
	unsigned port_no;
	struct bridge_id designated_root;
	struct bridge_id designated_bridge;
	unsigned int port_id;
	unsigned int designated_port;
	unsigned int priority;
	unsigned char top_change_ack;
	unsigned char config_pending;
	unsigned char state;
	unsigned path_cost;
	unsigned designated_cost;
	struct timeval message_age_timer_value;
	struct timeval forward_delay_timer_value;
	struct timeval hold_timer_value;
};

struct sip_call_start
{
    unsigned long long SessionId;
    unsigned char CltMac[6];
    int WifiIf;                                 /* To get WiFi session ID */
    char Url[50];
    char Codecs[15][30];
} __attribute__((packed));

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
} __attribute__((packed));

struct sip_call_report
{
    unsigned int Latency;
    unsigned int Jitter;
    unsigned int PacketLoss;
    unsigned int Mos;
} __attribute__((packed));

struct streaming_video_start
{
    unsigned long long SessionId;
    unsigned char CltMac[6];
    int WifiIf;                                 /* To get WiFi session ID */
    unsigned int ServerIp;
    unsigned int Type;
    char ServerDnsName[100];
} __attribute__((packed));

struct streaming_video_stop
{
    unsigned long long SessionId;
    unsigned char CltMac[6];
    int WifiIf;                                 /* To get WiFi session ID */
    unsigned long long TotalBytes;
    unsigned int Duration;
    unsigned int Type;
    unsigned int ServerIp;
} __attribute__((packed));

struct http_user_agent
{
    unsigned char macAddress[6];
    char userAgent[200];
} __attribute__((packed));

struct add_tun_parms
{
    unsigned int MyIp;
    unsigned int PeerIp;
    unsigned int Primary;
    unsigned char MyMac[6];
} __attribute__((packed));

struct del_tun_parms
{
    unsigned int PeerIp;
    unsigned char PeerMac[6];
} __attribute__((packed));

struct wc_log_line
{
    unsigned int Num;
    unsigned int Level;
    unsigned int TimeStamp;
    unsigned char Message[200];
} __attribute__((packed));

struct fr_record
{
    unsigned int Round;
    unsigned int Timestamp;
    char Message[504];
} __attribute__((packed));

struct wc_tunnel_stats
{
    unsigned int Ip;
    unsigned int ConfTime;
    unsigned int UpTime;
    unsigned int Active;
    unsigned int PingsSent;
    unsigned int PingsReceived;
} __attribute__((packed));

struct radius_report_stats
{
    unsigned int Entry;
    unsigned int ServerIp;
    unsigned int NoAnswers;
    /* All times are in milliseconds */
    unsigned int TimeMin;
    unsigned int TimeMax;
    unsigned int TimeAve;
} __attribute__((packed));

struct local_mac_set
{
    unsigned int Status;
    unsigned int Vlan;
    unsigned char Mac[6];
} __attribute__((packed));

struct wc_capt_buf
{
    unsigned long long TimeStamp;
    unsigned long long tsInUs;
    unsigned long long SessionId;
    unsigned char Type;
    unsigned char From;
    unsigned int Len;
    unsigned int Channel;
    unsigned int Direction;
    unsigned int Count;
    int Rssi;
    unsigned int DataRate;
    int wifiIf;         // for dhcp
    char staMac[6];     // for dhcp
    unsigned char Buff[500];
} __attribute__((packed));

struct wc_arp_entry
{
    unsigned char mac[6];
    unsigned int ip;
    unsigned long long TimeStamp;
    unsigned int action;
    unsigned int vlan;
} __attribute__((packed));

#define NUM_OF_NEIGH_IN_REQUEST     160

struct wc_ngbr_capt_buf
{
    unsigned char Type;
    unsigned long long TimeStamp;
    unsigned int Channel;
    int Rssi;
    unsigned char Bssid[6];
    unsigned char SrcMac[6];
} __attribute__((packed));

struct wc_perf_stats
{
    unsigned int MemFree;
    unsigned int EthPackets;
    unsigned int WiFiPackets;
    unsigned int CpuBusy[2];
    unsigned int AppRestarted[4];
    unsigned int MgmtBytesTx[2];
    unsigned int MgmtBytesRx[2];
    int EthLinkState;
    int EthSpeed;
    int EthDuplex;
} __attribute__((packed));

struct clt_hash_info
{
    unsigned int VbrInd;
    unsigned int Num;
    void * EntryPtr;
    unsigned short Vlan;
    unsigned char Mac[6];
    unsigned int ApIp;
    unsigned int Age;

    unsigned int CpTimeout;
    unsigned int CpBpsUp;
    unsigned int CpBpsDown;
    unsigned int CpWhitelist;

    unsigned int SipPort;
    unsigned int SipState;
    unsigned int RtpPortsFrom;
    unsigned int RtpPortsTo;
    unsigned int RtpIpFrom;
    unsigned int RtpIpTo;

    unsigned int FpAge;
    unsigned int NbrAge;
    unsigned int ArpAge;
    unsigned int ArpIp;

    unsigned int UaFlag;
    unsigned int UaTimeOut;
} __attribute__((packed));

struct mac_hash_info
{
    unsigned int VbrInd;
    unsigned int Num;
    void * EntryPtr;
    unsigned short Vlan;
    unsigned short Age;
    unsigned char Mac[6];
    unsigned char IfName[20];
    unsigned int CreditsFrom;
    unsigned int CreditsTo;
    unsigned int DroppedFrom;
    unsigned int DroppedTo;
    unsigned char Flags;
} __attribute__((packed));

struct wc_iac_register
{
    unsigned int MessCode;
    unsigned int UdpPort;
    unsigned int Status;                        /* 1 - register; 0 - deregister */
} __attribute__((packed));

struct wc_iac_send
{
    unsigned int DestIp;
    unsigned int MessCode;
    unsigned int MessLen;
    unsigned char Body[15000];
} __attribute__((packed));

struct wc_apc_neigh
{
    unsigned int Ip;
    unsigned char BasicMac[6];
} __attribute__((packed));

#define APC_UNKNOWN     0
#define I_AM_APC        1
#define I_AM_BAPC       2

struct wc_apc_spec
{
    unsigned int IsApc;
    unsigned int Changed;
    unsigned int FloatIp;
    struct wc_apc_neigh Neighbors[APC_MAX_NEIGHBORS];
} __attribute__((packed));

struct wc_mac_ident
{
    unsigned char Mac[6];
    unsigned char Ident[20];
    unsigned long long SessionId;
    unsigned int IdentLen;
} __attribute__((packed));

struct wc_subnet_filter
{
    char IfName[20];
    unsigned int SubnetBase[5];
    unsigned int SubnetMask[5];
} __attribute__((packed));

struct wc_dhcp_release
{
    unsigned int Vlan;
    unsigned int DestIp;
    unsigned int SrcIp;
    unsigned char DestMac[6];
    unsigned char SrcMac[6];
} __attribute__((packed));

struct wc_arp_sweep
{
    unsigned int Vlan;
    unsigned int Subnet;	//subnet = IP OR netMask e.g. 172.16.10.0 
    unsigned int SrcIp;		//Src IP
    unsigned char DestMac[6];
    unsigned char SrcMac[6];
    unsigned int SubnetSize;	//num of IPs in subnet e.g. 255
} __attribute__((packed));

struct wc_cp_whitelist
{
    int Type;
    char Hostname[256];
} __attribute__((packed));

struct cp_auth_failed
{
    unsigned char Mac[10][6];
    unsigned int Count[10];
} __attribute__((packed));

struct cp_auth_attempt
{
    unsigned char Mac[6];
    unsigned char Uname[52];
    unsigned int Status;
} __attribute__((packed));

struct band_limit
{
    char IfName[20];
    unsigned int ToLimit;                       /* Downstream */
    unsigned int FromLimit;                     /* Upstream */
    unsigned int Type;
} __attribute__((packed));

struct bss_upstream_stats
{
    char IfName[20];
    unsigned int PktsForwToLocal;
    unsigned int PktsForwToLocalOver;
    unsigned int BytesForwToLocal;
    unsigned int BytesForwToLocalOver;
    unsigned int PktsForwToIntern;
    unsigned int PktsForwToInternOver;
    unsigned int BytesForwToIntern;
    unsigned int BytesForwToInternOver;
} __attribute__((packed));

struct bonjour_forw_rule
{
    char RuleName[20];
    unsigned int VLAN[5];
    unsigned char Services[5][100];
} __attribute__((packed));

struct vlan_ssid
{
    char IfName[20];
    unsigned int VLAN;
} __attribute__((packed));

struct bj_profile_set
{
    char IfName[20];
    char ProfileName[20];
} __attribute__((packed));

struct rogue_ap_macs
{
    unsigned int MacHigh;
    unsigned int MacLow;
    unsigned int MaskBits;
} __attribute__((packed));

struct raw_pkt_throttle
{
    unsigned int Type;
    unsigned int Interval;
    unsigned int Limit;
} __attribute__((packed));

struct forw_mode_set
{
    char IfName[20];
    int ForwMode;
} __attribute__((packed));

struct wc_ethports_stats
{
    int port0LinkState;
    int port1LinkState;
} __attribute__((packed));

struct ua_timeout_upd
{
    unsigned char cltMac[6];
    unsigned int  uaTimeout;
} __attribute__((packed));

struct delClient
{
    unsigned char cltMac[6];
} __attribute__((packed));

struct stream_srvr
{
    unsigned int IpAddr;
    unsigned int Type;
    unsigned char DnsName[100];
} __attribute__((packed));

extern int br_init(void);
extern int br_refresh(void);
extern void br_shutdown(void);

//extern int br_foreach_bridge(int (*iterator)(const char *brname, void *), void *arg);
extern int br_foreach_port(const char *brname,
			   int (*iterator)(const char *brname, const char *port,
					   void *arg ),
			   void *arg);
extern const char *br_get_state_name(int state);

//extern int br_get_bridge_info(const char *br, struct bridge_info *info);
extern int br_get_port_info(const char *brname, const char *port,
			    struct port_info *info);
extern int wc_add_vbr( const char * name );
extern int wc_del_vbr( const char * name );
extern int wc_add_interface( const char * vbr, const char * dev );
extern int wc_del_interface( const char * vbr, const char * dev );
//extern int br_set_bridge_forward_delay(const char *br, struct timeval *tv);
//extern int br_set_bridge_hello_time(const char *br, struct timeval *tv);
//extern int br_set_bridge_max_age(const char *br, struct timeval *tv);
//extern int br_set_ageing_time(const char *br, struct timeval *tv);
//extern int br_set_stp_state(const char *br, int stp_state);
//extern int br_set_bridge_priority(const char *br, int bridge_priority);
//extern int br_set_port_priority(const char *br, const char *p, int port_priority);
int wc_set_socket( void );

//extern int br_set_path_cost(const char *br, const char *p, int path_cost);
//extern int br_read_fdb(const char *br, struct fdb_entry *fdbs, unsigned long skip, int num);
#endif
