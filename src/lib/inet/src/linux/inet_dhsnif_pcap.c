/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <arpa/inet.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <pcap.h>
#include <ev.h>

#include "const.h"
#include "ds_tree.h"
#include "log.h"
#include "util.h"

#include "inet.h"
#include "inet_dhsnif.h"

#define MODULE_ID LOG_MODULE_ID_DHCPS

static bool inet_dhsnif_init(inet_dhsnif_t *self, const char *ifname);
static bool inet_dhsnif_fini(inet_dhsnif_t *self);
static bool __inet_dhsnif_start(inet_dhsnif_t *self);
static bool __inet_dhsnif_stop(inet_dhsnif_t *self);
static void __inet_dhsnif_recv(EV_P_ ev_io *ev, int revents);
static void __inet_dhsnif_process(u_char *__self, const struct pcap_pkthdr *pkt, const u_char *packet);
static void __inet_dhsnif_process_L3(inet_dhsnif_t *self, const struct pcap_pkthdr *pkt, const u_char *packet, uint32_t offset);
static void __inet_dhsnif_process_dhcp(inet_dhsnif_t *self, const struct pcap_pkthdr *pkt, const u_char *packet, uint32_t offset);
static bool inet_dhsnif_fingerprint_to_str(uint8_t *finger, char *s, size_t sz);

static ds_key_cmp_t inet_dhsnif_lease_cmp;

/*
 * ===========================================================================
 *  Public interface
 * ===========================================================================
 */
struct __inet_dhsnif
{
    char                    ds_ifname[C_IFNAME_LEN];    /* Interface name */
    inet_dhcp_lease_fn_t   *ds_lease_fn;                /* Lease callback */
    inet_t                 *ds_lease_inet;              /* Data for ds_lease_fn */
    pcap_t                 *ds_pcap;                    /* PCAP instance -- non-NULL if started */
    ev_io                   ds_pcap_ev;                 /* PCAP filedescriptor watcher */
    int                     ds_pcap_fd;                 /* PCAP select()able FD */
    struct bpf_program      ds_bpf;                     /* PCAP BPF program */
    bool                    ds_bpf_valid;               /* ds_bpf was initialized successfully */
    ds_tree_t               ds_lease_list;              /* List of leases */
};

/*
 * Return a new instance of the DHCP sniffing class
 */
inet_dhsnif_t *inet_dhsnif_new(const char *ifname)
{
    inet_dhsnif_t *self = NULL;

    self = malloc(sizeof(inet_dhsnif_t));
    if (self == NULL)
    {
        LOG(ERR, "inet_dhsnif: %s: Error allocating object.", ifname);
        goto error;
    }

    if (!inet_dhsnif_init(self, ifname))
    {
        goto error;
    }

    return self;

error:
    if (self != NULL) free(self);
    return NULL;
}

/*
 * Destructor function
 */
bool inet_dhsnif_del(inet_dhsnif_t *self)
{
    bool retval = inet_dhsnif_fini(self);

    free(self);

    return retval;
}

/*
 * Start the DHCP service
 */
bool inet_dhsnif_start(inet_dhsnif_t *self)
{
    /* PCAP is already started -- nothing to do */
    if (self->ds_pcap != NULL) return true;

    return __inet_dhsnif_start(self);
}

/*
 * Stop the DHCP service
 */
bool inet_dhsnif_stop(inet_dhsnif_t *self)
{
    if (self->ds_pcap == NULL) return true;

    return __inet_dhsnif_stop(self);
}

/*
 * Set the DHCP sniffing callback
 */
bool inet_dhsnif_notify(inet_dhsnif_t *self, inet_dhcp_lease_fn_t *func, inet_t *inet)
{
    self->ds_lease_fn = func;
    self->ds_lease_inet = inet;

    return true;
}

/*
 * ===========================================================================
 *  DHCP Sniffing PCAP implementation (based upon lib/dhcps)
 * ===========================================================================
 */

/* Define the logging submodule */

/* The PCAP filtering string */
#define DHCPS_PCAP_STRING           "udp and (port bootpc or port bootps)"
/* Maximum number of packets to process each loop */
#define DHCPS_PCAP_DISPATCH_MAX     64

/**
 * Ethernet header
 */
#define ETH_TYPE_IP         0x800
struct eth_hdr
{
    uint8_t                 eth_dst[6];         /* Hardware destination address */
    uint8_t                 eth_src[6];         /* Hardware source address */
    uint16_t                eth_type;           /* Packet type */
};

/**
 * IP header
 */
#define IP_PROTO_ICMP       1                   /* ICMP protocol number */
#define IP_PROTO_IGMP       2                   /* IGMP protocol number */
#define IP_PROTO_TCP        6                   /* TCP protocol number */
#define IP_PROTO_UDP        17                  /* UDP protocol number */

#define IP_HL(ip)           ((ip)->ip_ver_hl & 0x0F)
#define IP_VER(ip)          (((ip)->ip_ver_hl & 0xF0) >> 4)

struct ip_hdr
{
    uint8_t                 ip_ver_hl;          /* Version and and header length */
    uint8_t                 ip_tos;             /* TOS */
    uint16_t                ip_len;             /* Total packet length */
    uint16_t                ip_id;              /* Identification */
    uint16_t                ip_frag_off;        /* Fragment offset */
    uint8_t                 ip_ttl;             /* Time-to-Live field */
    uint8_t                 ip_proto;           /* IP protocol */
    uint16_t                ip_cs;              /* Checksum */
    struct in_addr          ip_src;             /* Source IP address */
    struct in_addr          ip_dst;             /* Destination IP address */
    uint8_t                 ip_opts[0];         /* IP Options array */
};

/**
 * UDP header
 */
struct udp_hdr
{
    uint16_t                udp_src;            /* Source port */
    uint16_t                udp_dst;            /* Destination port */
    uint16_t                udp_len;            /* Total UDP packet length */
    uint16_t                udp_cs;             /* UDP checksum */
};

/**
 * DHCP header
 */
#define DHCP_MAGIC          0x63825363          /* Magic number */

struct dhcp_hdr
{
    uint8_t                 dhcp_op;            /* Operation ID */
    uint8_t                 dhcp_htype;         /* Hardware address type */
    uint8_t                 dhcp_hlen;          /* Hardware address length */
    uint8_t                 dhcp_hops;          /* Hops */
    uint32_t                dhcp_xid;           /* Transaction ID */
    uint16_t                dhcp_secs;          /* Time */
    uint16_t                dhcp_flags;         /* Flags */
    struct in_addr          dhcp_ciaddr;        /* Client IP address */
    struct in_addr          dhcp_yiaddr;        /* Your IP address */
    struct in_addr          dhcp_siaddr;        /* Server IP address */
    struct in_addr          dhcp_giaddr;        /* Gateway IP address */
    union
    {
        uint8_t             dhcp_chaddr[6];     /* Client hardware address */
        uint8_t             dhcp_padaddr[16];
    };
    uint8_t                 dhcp_server[64];    /* Server name */
    uint8_t                 dhcp_boot_file[128];/* Boot filename */
    uint32_t                dhcp_magic;         /* Magic */
    uint8_t                 dhcp_options[0];    /* DHCP options */
};

/*
 * DHCP message type
 */
#define DHCP_MSG_DISCOVER           1
#define DHCP_MSG_OFFER              2
#define DHCP_MSG_REQUEST            3
#define DHCP_MSG_DECLINE            4
#define DHCP_MSG_ACK                5
#define DHCP_MSG_NACK               6
#define DHCP_MSG_RELEASE            7
#define DHCP_MSG_INFORM             8

/**
 * Single DHCP lease entry
 */
struct inet_dhsnif_lease
{
    int                             le_msg_type;    /* Last DHCP message type seen */
    struct osn_dhcp_server_lease    le_info;        /* Lease info -- this will be passed down the API callback */
    ds_tree_node_t                  le_node;        /* Tree node structure */
};


/*
 * Static constructor
 */
bool inet_dhsnif_init(inet_dhsnif_t *self, const char *ifname)
{
    memset(self, 0, sizeof(*self));

    if (strscpy(self->ds_ifname, ifname, sizeof(self->ds_ifname)) < 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Interface name too long.", ifname);
        return false;
    }

    /* Initialize the DHCP leases list */
    ds_tree_init(&self->ds_lease_list, inet_dhsnif_lease_cmp, struct inet_dhsnif_lease, le_node);

    return true;
}

/*
 * Static destructor
 */
bool inet_dhsnif_fini(inet_dhsnif_t *self)
{
    (void)self;
    /* Nothing to do, yet */
    return true;
}

/*
 * Start capturing on the device
 */
bool __inet_dhsnif_start(inet_dhsnif_t *self)
{
    int rc;
    char pcap_err[PCAP_ERRBUF_SIZE];

    /*
     * Initialize the PCAP interface
     */
    self->ds_pcap = pcap_create(self->ds_ifname, pcap_err);
    if (self->ds_pcap == NULL)
    {
        LOG(ERR, "inet_dhsnif: %s: PCAP initialization failed.", self->ds_ifname);
        goto error;
    }

#if defined(CONFIG_DHSNIFF_PCAP_IMMEDIATE)
    if (pcap_set_immediate_mode(self->ds_pcap, 1) != 0)
    {
        LOG(WARN, "inet_dhsnif: %s: PCAP failed to set immediate mode!", self->ds_ifname);
    }
#endif

#if defined(CONFIG_DHSNIFF_PCAP_SNAPLEN) && (CONFIG_DHSNIFF_PCAP_SNAPLEN > 0)
    /*
     * Set the snapshot length to something sensible.
     */
    rc = pcap_set_snaplen(self->ds_pcap, CONFIG_DHSNIFF_PCAP_SNAPLEN);
    if (rc != 0)
    {
        LOG(WARN, "inet_dhsnif: %s: Unable to set snapshot length: %d",
                self->ds_ifname,
                CONFIG_DHSNIFF_PCAP_SNAPLEN);
    }
#endif

#if defined(CONFIG_DHSNIFF_PCAP_BUFFER_SIZE) && (CONFIG_DHSNIFF_PCAP_BUFFER_SIZE > 0)
    /*
     * Set the buffer size.
     */
    rc = pcap_set_buffer_size(self->ds_pcap, CONFIG_DHSNIFF_PCAP_BUFFER_SIZE);
    if (rc != 0)
    {
        LOG(WARN, "inet_dhsnif: %s: Unable to set buffer size: %d",
                self->ds_ifname,
                CONFIG_DHSNIFF_PCAP_BUFFER_SIZE);
    }
#endif

#if defined(CONFIG_INET_DHSNIFF_PCAP_PROMISC)
    /*
     * XXX Promisc mode is not needed, keep the code around in case we need it.
     */
    rc = pcap_set_promisc(self->ds_pcap, 1);
    if (rc != 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Unable to set promiscuous mode.", self->ds_ifname);
        goto error;
    }
#endif

    /*
     * Set non-blocking mode
     * XXX pcap_setnonblock() returns the current non-blocking state. However, on savefiles
     * it will always return 0. So the proper way to check for errors is to check if it returns
     * -1.
     */
    rc = pcap_setnonblock(self->ds_pcap, 1, pcap_err);
    if (rc == -1)
    {
        LOG(ERR, "inet_dhsnif: %s: Unable to set non-blocking mode: %s", self->ds_ifname, pcap_err);
        goto error;
    }

    /* Activate the interface */
    rc = pcap_activate(self->ds_pcap);
    if (rc != 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Error activating PCAP: %s", self->ds_ifname, pcap_geterr(self->ds_pcap));
        goto error;
    }

    /*
     * Setup the capture filter -- we want to capture only DHCP packets.
     *
     * XXX This must be done after pcap_activate() or it will fail
     */
    LOG(INFO, "inet_dhsnif: %s: Creating capture filter: '%s'", self->ds_ifname, DHCPS_PCAP_STRING);
    rc = pcap_compile(
            self->ds_pcap,
            &self->ds_bpf,
            DHCPS_PCAP_STRING,
            0,  /* Optimize */
            PCAP_NETMASK_UNKNOWN);
    if (rc  != 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Error compiling capture filter: '%s', error: %s",
                self->ds_ifname,
                DHCPS_PCAP_STRING,
                pcap_geterr(self->ds_pcap));
        goto error;
    }

    self->ds_bpf_valid = true;

    rc = pcap_setfilter(self->ds_pcap, &self->ds_bpf);
    if (rc != 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Error setting the capture filter, error: %s",
                self->ds_ifname,
                pcap_geterr(self->ds_pcap));
        goto error;
    }

    /* We need a selectable fd for libev */
    self->ds_pcap_fd = pcap_get_selectable_fd(self->ds_pcap);
    if (self->ds_pcap_fd < 0)
    {
        LOG(ERR, "inet_dhsnif: %s: Error getting selectable FD, error: %s",
                self->ds_ifname,
                pcap_geterr(self->ds_pcap));
        goto error;
    }

    /* Register FD for libev events */
    ev_io_init(
            &self->ds_pcap_ev,
            __inet_dhsnif_recv,
            self->ds_pcap_fd, EV_READ);

    /* Set user data */
    self->ds_pcap_ev.data = (void *)self;

    /* Start watching it on the default queue */
    ev_io_start(EV_DEFAULT_ &self->ds_pcap_ev);

    LOG(INFO, "inet_dhsnif: %s: Interface registered for DHCP sniffing.", self->ds_ifname);

    return true;

error:
    __inet_dhsnif_stop(self);

    return false;
}

/*
 * Stop all capturing
 */
bool __inet_dhsnif_stop(inet_dhsnif_t *self)
{
    /* Stop any libev watchers */
    ev_io_stop(EV_DEFAULT_ &self->ds_pcap_ev);

    /* Cleanup the bpf filtering code stuff */
    if (self->ds_bpf_valid)
    {
        pcap_freecode(&self->ds_bpf);
    }
    self->ds_bpf_valid = false;

    /* Destroy the pcap structure */
    if (self->ds_pcap != NULL)
    {
        pcap_close(self->ds_pcap);
    }

    self->ds_pcap = NULL;

    return true;
}

/*
 * Receive a PCAP packet
 */
void __inet_dhsnif_recv(EV_P_ ev_io *ev, int revents)
{
    (void)loop;
    (void)revents;

    inet_dhsnif_t *self = ev->data;

    /* Ready to receive packets */
    pcap_dispatch(self->ds_pcap, DHCPS_PCAP_DISPATCH_MAX, __inet_dhsnif_process, (void *)self);
}

/*
 * Process a PCAP packet
 */
void __inet_dhsnif_process(
        u_char *__self,
        const struct pcap_pkthdr *pkt,
        const u_char *packet)
{
    inet_dhsnif_t *self = (void *)__self;
    (void)packet;

    uint32_t l2_offset = 0;

    /* Handle l2 packet type */
    int l2_type = pcap_datalink(self->ds_pcap);

    /*
     * Peel off the l2 layer of the onion
     */
    switch (l2_type)
    {
        /* Linux Cooked */
        case DLT_LINUX_SLL:
            l2_offset = 2;
            break;

        /* Ethernet */
        case DLT_EN10MB:
            l2_offset = 0;
            break;

        default:
            LOG(WARNING, "inet_dhsnif: %s: Unknown l2 type: %s (%d). Skipping packet.",
                    self->ds_ifname,
                    pcap_datalink_val_to_description(l2_type),
                    l2_type);
            return;
    }

    struct eth_hdr *eth = (void *)(packet + l2_offset);

    /* We're interested in IP only */
    if (ntohs(eth->eth_type) !=  ETH_TYPE_IP) return;

#if 0
    LOG(DEBUG, "inet_dhsnif: %s: Ethernet: dst:"PRI(inet_macaddr_t)" src:"PRI(inet_macaddr_t)" type:%d",
            self->ds_ifname,
            FMT(inet_macaddr_t, eth->eth_dst),
            FMT(inet_macaddr_t, eth->eth_src),
            ntohs(eth->eth_type));
#endif

    /* Forward the packet to L3 processing */
    __inet_dhsnif_process_L3(self, pkt, packet, l2_offset + sizeof(struct eth_hdr));
}

/*
 * Process a layer 3 packet
 */
void __inet_dhsnif_process_L3(
        inet_dhsnif_t *self,
        const struct pcap_pkthdr *pkt,
        const u_char *packet,
        uint32_t offset)
{
    uint32_t udp_offset;

    /* Overimpose the IP header */
    struct ip_hdr *ip = (void *)(packet + offset);

    if (offset + sizeof(struct ip_hdr) > pkt->caplen)
    {
        LOG(WARNING, "inet_dhsnif: %s: IP header truncated. Dropping packet.", self->ds_ifname);
        return;
    }

#if 0
    LOG(DEBUG, "inet_dhsnif: %s: IP ver %d, hlen %d, proto %d received:  "PRI(inet_ip4addr_t)" -> "PRI(inet_ip4addr_t),
            self->ds_ifname,
            IP_VER(ip),
            IP_HL(ip),
            ip->ip_proto,
            FMT(inet_ip4addr_t, ip->ip_src),
            FMT(inet_ip4addr_t, ip->ip_dst));
#endif

    /* Filter only the IPv4 protocol */
    if (IP_VER(ip) != 4)
    {
        LOG(INFO, "inet_dhsnif: %s: IP protocol version not IPv4. Dropping packet.", self->ds_ifname);
        return;
    }

    /* Filter UDP packets */
    if (ip->ip_proto != IP_PROTO_UDP)
    {
        LOG(INFO, "inet_dhsnif: %s: IP protocol not UDP. Dropping packet.", self->ds_ifname);
        return;
    }

    udp_offset = offset + (IP_HL(ip) << 2);
    if (udp_offset + sizeof(struct udp_hdr) > pkt->caplen)
    {
        LOG(INFO, "inet_dhsnif: %s: UDP header truncated. Dropping packet.", self->ds_ifname);
        return;
    }

    struct udp_hdr *udp = (void *)(packet + udp_offset);

    LOG(DEBUG, "inet_dhsnif: %s: UDP src:%d dst:%d len:%d",
            self->ds_ifname,
            ntohs(udp->udp_src),
            ntohs(udp->udp_dst),
            ntohs(udp->udp_len));

    /* Check the DHCP valid port range */
    if ((ntohs(udp->udp_src) != 68 || ntohs(udp->udp_dst) != 67) &&
        (ntohs(udp->udp_src) != 67 || ntohs(udp->udp_dst) != 68))
    {
        LOG(ERR, "inet_dhsnif: %s: UDP src/dst port range mismatch.", self->ds_ifname);
        return;
    }

    if (ntohs(udp->udp_len) < sizeof(struct dhcp_hdr))
    {
        LOG(ERR, "inet_dhsnif: %s: UDP packet too short. DropPING.", self->ds_ifname);
        return;
    }

    __inet_dhsnif_process_dhcp(self, pkt, packet, udp_offset + sizeof(struct udp_hdr));
}

/*
 * Process a DHCP packet
 */
void __inet_dhsnif_process_dhcp(
        inet_dhsnif_t *self,
        const struct pcap_pkthdr *pkt,
        const u_char *packet,
        uint32_t offset)
{
    if (offset + sizeof(struct dhcp_hdr) > pkt->caplen)
    {
        LOG(ERR, "inet_dhsnif: %s: DHCP header truncated. Dropping packet.", self->ds_ifname);
        return;
    }

    struct dhcp_hdr *dhcp = (void *)(packet + offset);
    if (ntohl(dhcp->dhcp_magic) != DHCP_MAGIC)
    {
        LOG(WARNING, "inet_dhsnif: %s: Magic number invalid. Dropping packet.", self->ds_ifname);
        return;
    }

#if 0
    LOG(DEBUG, "inet_dhsnif: %s: DHCP packet htype:%d hlen:%d xid:%08x ClientAddr:"PRI(inet_macaddr_t)" ClientIP:"PRI(inet_ip4addr_t)" Magic:%08x",
            self->ds_ifname,
            dhcp->dhcp_htype,
            dhcp->dhcp_hlen,
            ntohl(dhcp->dhcp_xid),
            FMT(inet_macaddr_t, dhcp->dhcp_chaddr),
            FMT(inet_ip4addr_t, dhcp->dhcp_yiaddr),
            ntohl(dhcp->dhcp_magic));
#endif

    /*
     * CHADDR should be in every DHCP packet and should always contain the client MC address.
     * Lookup the current leases list using the client's hardware address. Use it to lookup
     * the leases structure.
     */
    struct inet_dhsnif_lease *lease = ds_tree_find(&self->ds_lease_list, &dhcp->dhcp_chaddr);
    if (lease == NULL)
    {
        /* Allocate new lease */
        lease = calloc(1, sizeof(struct inet_dhsnif_lease));
        lease->le_info.dl_hwaddr = OSN_MAC_ADDR_INIT;
        lease->le_info.dl_ipaddr = OSN_IP_ADDR_INIT;
        memcpy(lease->le_info.dl_hwaddr.ma_addr, dhcp->dhcp_chaddr, sizeof(lease->le_info.dl_hwaddr.ma_addr));
        ds_tree_insert(&self->ds_lease_list, lease, &lease->le_info.dl_hwaddr);
    }

    /*
     * Parse DHCP options, update the current lease
     */
    uint8_t *popt = dhcp->dhcp_options;
    uint8_t fin[OSN_DHCP_FINGERPRINT_MAX];
    uint8_t msg_type = 0;

    fin[0] = 0;

    while (popt < packet + pkt->caplen)
    {
        uint8_t optid;
        uint8_t optlen;
        uint8_t *pfin;

        /* End option, break out */
        if (*popt == 255) break;

        /* Pad option, continue */
        if (*popt == 0)
        {
            popt++;
            continue;
        }

        if (popt + 2 > packet + pkt->caplen)
        {
            LOG(ERR, "inet_dhsnif: %s: DHCP options truncated.", self->ds_ifname);
            break;
        }

        optid = *popt++;
        optlen = *popt++;

        if (popt + optlen > packet + pkt->caplen)
        {
            LOG(ERR, "inet_dhsnif: %s: DHCP options truncated.", self->ds_ifname);
            break;
        }

        switch (optid)
        {
            case DHCP_OPTION_ADDRESS_REQUEST:
                /* This is not actually the given IP address */
#if 0
                LOG(DEBUG, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": IPv4 Address = "PRI(inet_ip4addr_t),
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr),
                        FMT(inet_ip4addr_t, *(inet_ip4addr_t *)popt));
#endif
                break;

            case DHCP_OPTION_LEASE_TIME:
                lease->le_info.dl_leasetime = ntohl(*(uint32_t *)popt);
#if 0
                LOG(DEBUG, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Lease time =  %ds",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr),
                        (int)lease->le_info.dl_leasetime);
#endif
                break;

            case DHCP_OPTION_MSG_TYPE:
                /* Message type */
#if 0
                LOG(DEBUG, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Message type = %d",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr),
                        *(uint8_t *)popt);
#endif
                msg_type = *popt;
                lease->le_msg_type = msg_type;
                break;

            case DHCP_OPTION_PARAM_LIST:
                /* Parameter list - for fingerprinting */
                pfin = fin;
                int ii;

                for (ii = 0; ii < optlen; ii++)
                {
                    if (popt[ii] == 0) continue;

                    if (pfin > fin + sizeof(fin) - 1)
                    {
                        /* We reached the size limit, ignore rest of options */
                        break;
                    }

                    *pfin++ = popt[ii];
                }

                *pfin = 0;

                break;

            case DHCP_OPTION_HOSTNAME:
                /* Update the lease hostname */
                if (optlen == 0)
                {
                    /* In theory this should never happen */
#if 0
                    LOG(WARN, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Client requested empty hostname.",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif
                    break;
                }

                if (optlen >= sizeof(lease->le_info.dl_hostname) - 1)
                {
#if 0
                    LOG(WARN, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Hostname option too long. Discarding packet.",
                            self->ds_ifname,
                            FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif
                    return;
                }

                /* Client requested Hostname */
                memcpy(lease->le_info.dl_hostname, popt, optlen);
                lease->le_info.dl_hostname[optlen] = '\0';
                break;

            case DHCP_OPTION_VENDOR_CLASS:
                if (optlen == 0)
                {
#if 0
                    LOG(WARN, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Client supplied empty vendor class.",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif
                    break;
                }

                /* Update the lease vendor-class */
                if (optlen >= sizeof(lease->le_info.dl_vendorclass) - 1)
                {
#if 0
                    LOG(WARN, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Vendor class option too long. Discarding packet.",
                            self->ds_ifname,
                            FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif
                    break;
                }

                memcpy(lease->le_info.dl_vendorclass, popt, optlen);
                lease->le_info.dl_vendorclass[optlen] = '\0';
                break;

            default:
#if 0
                LOG(DEBUG, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Received DHCP Option: %d(%d)\n",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr),
                        optid,
                        optlen);
#endif
                break;
        }

        popt += optlen;
    }

    /* Check our current phase */
    switch (msg_type)
    {
        case DHCP_MSG_DISCOVER:
            /* The fingerprint is valid only during this phase */
            if (!inet_dhsnif_fingerprint_to_str(
                    fin,
                    lease->le_info.dl_fingerprint,
                    sizeof(lease->le_info.dl_fingerprint)))
            {
#if 0
                LOG(ERR, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Unable to convert fingerprint to string.",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif

                lease->le_info.dl_fingerprint[0] = '\0';
            }

            break;

        case DHCP_MSG_REQUEST:
            if (lease->le_info.dl_fingerprint[0] == '\0')
            {
                /*
                 * Get options from DHCP REQUEST if we didn't get them at DHCP_DISCOVER
                 *
                 * This may happen if we did not catch the DHCP request, but we did catch the DHCP renewal.
                 *
                 * Note: The fingerbank fingerprint should be the one from the discovery phase.
                 */
                if (!inet_dhsnif_fingerprint_to_str(
                        fin,
                        lease->le_info.dl_fingerprint,
                        sizeof(lease->le_info.dl_fingerprint)))
                {
#if 0
                    LOG(ERR, "inet_dhsnif: %s: "PRI(inet_macaddr_t)": Unable to convert fingerprint to string.",
                        self->ds_ifname,
                        FMT(inet_macaddr_t, lease->le_info.dl_hwaddr));
#endif

                    lease->le_info.dl_fingerprint[0] = '\0';
                }
            }
            break;

        case DHCP_MSG_OFFER:
            /* Save the IP address */
            lease->le_info.dl_ipaddr = OSN_IP_ADDR_INIT;
            lease->le_info.dl_ipaddr.ia_addr = dhcp->dhcp_yiaddr;
            break;

        case DHCP_MSG_ACK:
            /* Update the IP address */
            lease->le_info.dl_ipaddr = OSN_IP_ADDR_INIT;
            lease->le_info.dl_ipaddr.ia_addr = dhcp->dhcp_yiaddr;

            LOG(NOTICE, "inet_dhsnif: ACK IP:"PRI_osn_ip_addr" MAC:"PRI_osn_mac_addr" Hostname:%s",
                    FMT_osn_ip_addr(lease->le_info.dl_ipaddr),
                    FMT_osn_mac_addr(lease->le_info.dl_hwaddr),
                    lease->le_info.dl_hostname);
            /* Call callback */
            if (self->ds_lease_fn != NULL)
            {
                self->ds_lease_fn(self->ds_lease_inet, false, &lease->le_info);
            }

            break;

        case DHCP_MSG_NACK:
        case DHCP_MSG_RELEASE:
            /* Callback */
            LOG(NOTICE, "inet_dhsnif: RELEASE IP:"PRI_osn_ip_addr" MAC:"PRI_osn_mac_addr" Hostname:%s",
                    FMT_osn_ip_addr(lease->le_info.dl_ipaddr),
                    FMT_osn_mac_addr(lease->le_info.dl_hwaddr),
                    lease->le_info.dl_hostname);
            if (self->ds_lease_fn != NULL)
            {
                self->ds_lease_fn(self->ds_lease_inet, false, &lease->le_info);
            }

            /* Remove from the list */
            ds_tree_remove(&self->ds_lease_list, lease);
            free(lease);

            break;
    }
}


/**
 * Convert a binary representation of the fingerprint to a comma delimited string
 */
bool inet_dhsnif_fingerprint_to_str(uint8_t *finger, char *s, size_t sz)
{
    uint8_t *pfin = finger;
    size_t len = 0;

    s[0] = '\0';
    for (pfin = finger; *pfin != 0; pfin++)
    {
        int rc;

        if (len == 0)
        {
            rc = snprintf(s, sz, "%d", *pfin);
        }
        else
        {
            rc = snprintf(s + len, sz - len, ",%d", *pfin);
        }

        if (rc < 0)
        {
            s[len] = '\0';
            return false;
        }

        /* Overflow -- handle this properly by not adding partial strings or appending new entries after this */
        if (len + rc >= sz)
        {
            s[len] = '\0';
            return false;
        }

        len += rc;
    }

    return true;
}

/**
 * Function for comparing keys for a dhcps_lease structure
 */
int inet_dhsnif_lease_cmp(void *a, void *b)
{
    return memcmp(a, b, sizeof(os_macaddr_t));
}
