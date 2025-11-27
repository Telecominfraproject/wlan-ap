#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/if.h>

#include <pcap.h>
#include <signal.h>

#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <uci.h>

#include "udhcpinject.h"

// Cleanup function
void cleanup()
{
    syslog(LOG_INFO, "Cleaning up resources...\n");

    if (handle)
    {
        pcap_close(handle);
        handle = NULL;
    }

    if (iface_map)
    {
        char cmd[1024];
        for (int i = 0; i < iface_map_size; i++)
        {
            snprintf(cmd, sizeof(cmd), "tc filter del dev %s ingress pref 32 2>/dev/null",
                     iface_map[i].iface);
            system(cmd);
        }
        free(iface_map);
    }

    if (port_map)
    {
        for (int i = 0; i < port_map_size; i++)
        {
            close(port_map[i].sock);
        }
        free(port_map);
    }

    syslog(LOG_INFO, "Cleanup complete.\n");
}

int setup_tc()
{
    char cmd[512];

    // check if ifb-inject exists, if not create it
    snprintf(cmd, sizeof(cmd), "ip link show ifb-inject >/dev/null 2>&1");
    if (system(cmd) != 0)
    {
        snprintf(cmd, sizeof(cmd),
                 "ip link add name ifb-inject type ifb && ip link set "
                 "ifb-inject up");
        if (system(cmd) != 0)
        {
            syslog(LOG_ERR, "Failed to setup ifb-inject\n");
            return -1;
        }
    }

    for (int i = 0; i < iface_map_size; i++)
    {
        snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s ingress 2>/dev/null 1>2",
                 iface_map[i].iface);
        int result = system(cmd);
        if (result == 2)
        {
            syslog(LOG_INFO, "Ingress qdisc already exists for %s\n", iface_map[i].iface);
        }
        else if (result == 1)
        {
            syslog(LOG_ERR, "Failed to add qdisc for %s\n", iface_map[i].iface);
            return -1;
        }

        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s ingress protocol ip pref 32 u32 "
                 "match ip protocol 17 0xff "
                 "match u16 0x0044 0xffff at 20 "
                 "match u16 0x0043 0xffff at 22 "
                 "match u8 0x01 0xff at 28 "
                 "action vlan push id %d pipe "
                 "action mirred egress mirror dev ifb-inject pipe "
                 "action drop",
                 iface_map[i].iface, iface_map[i].serial);
        if (system(cmd) != 0)
        {
            syslog(LOG_ERR, "Failed to setup tc for %s\n", iface_map[i].iface);
            return -1;
        }
    }
    return 0;
}

int parse_iwinfo_by_essid(struct iface_info *iface)
{
    char cmd[256];
    FILE *fp;
    char output[128];
    char *line;

    snprintf(cmd, sizeof(cmd), IWINFO_CMD, iface->essid, iface->frequency);

    fp = popen(cmd, "r");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "Failed to execute command: %s\n", cmd);
        return 1;
    }

    // Read the first line (interface name)
    line = fgets(output, sizeof(output), fp);
    if (line)
    {
        output[strcspn(output, "\n")] = '\0'; // Remove trailing newline
        snprintf(iface->iface, LEN_IFACE + 1, output);
    }
    else
    {
        return 1;
    }

    // Read the second line (BSSID)
    line = fgets(output, sizeof(output), fp);
    if (line)
    {
        output[strcspn(output, "\n")] = '\0'; // Remove trailing newline
        snprintf(iface->bssid, LEN_BSSID + 1, output);
    }
    else
    {
        return 1;
    }

    // Close the pipe
    pclose(fp);
    return 0;
}

void add_iface_info(const char *essid, const char *upstream, const char *freq, int serial)
{
    iface_map = realloc(iface_map, (iface_map_size + 1) * sizeof(struct iface_info));
    if (!iface_map)
    {
        syslog(LOG_ERR, "Memory allocation failed\n");
        exit(1);
    }
    struct iface_info *info = &iface_map[iface_map_size];
    memset(info, 0, sizeof(struct iface_info));
    // use snprintf to copy essid to info->essid
    snprintf(info->essid, LEN_ESSID + 1, essid);
    snprintf(info->upstream, LEN_IFACE + 1, upstream);
    snprintf(info->frequency, 2, freq);
    info->serial = serial;
    iface_map_size++;
}

int parse_uci_config()
{
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx)
    {
        syslog(LOG_ERR, "Failed to allocate UCI context\n");
        return 1;
    }

    struct uci_package *pkg = NULL;
    if (uci_load(ctx, CONFIG_PATH, &pkg) != UCI_OK)
    {
        syslog(LOG_ERR, "Failed to load UCI config\n");
        uci_free_context(ctx);
        return 1;
    }

    int serial = 1;
    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e)
    {
        struct uci_section *s = uci_to_section(e);
        if (!strcmp(s->type, "network"))
        {
            const char *upstream = uci_lookup_option_string(ctx, s, "upstream");
            if (!upstream)
                continue;
            syslog(LOG_INFO, "Processing ssids with upstream %s", upstream);
            struct uci_option *opt, *opt2;

            opt = uci_lookup_option(ctx, s, "freq");
            if (opt && opt->type == UCI_TYPE_LIST)
            {
                struct uci_element *i;
                char *freq_type = NULL;
                uci_foreach_element(&opt->v.list, i)
                {
                    // parse 6G ifaces
                    if (!strcmp(i->name, "6G"))
                    {
                        opt2 = uci_lookup_option(ctx, s, "ssid6G");
                        freq_type = "6";
                    }
                    // parse 5G ifaces
                    else if (!strcmp(i->name, "5G"))
                    {
                        opt2 = uci_lookup_option(ctx, s, "ssid5G");
                        freq_type = "5";
                    }
                    // parse 2G ifaces
                    else if (!strcmp(i->name, "2G"))
                    {
                        opt2 = uci_lookup_option(ctx, s, "ssid2G");
                        freq_type = "2";
                    }

                    if (opt2 && opt2->type == UCI_TYPE_LIST)
                    {
                        struct uci_element *i;
                        uci_foreach_element(&opt2->v.list, i)
                        {
                            add_iface_info(i->name, upstream, freq_type, serial++);
                        }
                    }
                }
            }

            // initialize socket to upstream interface, say "up0v0"
            port_map = realloc(port_map, (port_map_size + 1) * sizeof(struct port_info));
            int sock = socket(AF_PACKET, SOCK_RAW, 0);
            if (sock < 0)
            {
                syslog(LOG_ERR, "Failed to create socket for %s\n", upstream);
                continue;
            }

            // Get interface index
            int ifindex = if_nametoindex(upstream);
            if (ifindex == 0)
            {
                syslog(LOG_ERR, "Failed to get ifindex for %s\n", upstream);
                close(sock);
            }

            // Store in port_map
            snprintf(port_map[port_map_size].name, LEN_IFACE + 1, upstream);
            port_map[port_map_size].sock = sock;
            port_map[port_map_size].ifindex = ifindex;
            port_map_size++;
            syslog(LOG_INFO, "Initialized socket for upstream interface %s", upstream);
        }
    }

    uci_free_context(ctx);
    return 0;
}

struct iface_info *find_iface_info_by_vlan(int vlan_id)
{
    for (int i = 0; i < iface_map_size; i++)
    {
        if (iface_map[i].serial == vlan_id)
        {
            return &iface_map[i];
        }
    }
    return NULL;
}

void process_packet(unsigned char *user, const struct pcap_pkthdr *header,
                    const unsigned char *packet)
{
    int orig_len = header->len;
    struct ethhdr *eth = (struct ethhdr *)packet;
    int vlan_id = -1;
    int eth_offset = sizeof(struct ethhdr);

    if (ntohs(eth->h_proto) != ETH_P_8021Q)
    {
        syslog(LOG_DEBUG,
               "No VLAN header found in packet (EtherType: 0x%04x)\n",
               ntohs(eth->h_proto));
        return;
    }

    struct vlan_hdr *vlan = (struct vlan_hdr *)(packet + eth_offset);
    vlan_id = ntohs(vlan->h_vlan_TCI) & 0x0FFF;
    eth_offset += sizeof(struct vlan_hdr);

    struct iface_info *info = find_iface_info_by_vlan(vlan_id);
    if (!info)
    {
        syslog(LOG_ERR, "No interface info found for VLAN ID %d\n", vlan_id);
        return;
    }

    syslog(LOG_INFO, "Received dhcp packet with vlan id %d from iface %s of length: %d", vlan_id, info->iface, header->len);

    char *hostname = get_hostname();
    int circuit_id_len = strlen(info->bssid) + 1 + strlen(info->essid);
    int remote_id_len = strlen(hostname);
    int opt82_len = 2 + 2 + circuit_id_len + 2 + remote_id_len;

    // Find DHCP options end from the end of the packet
    int ip_offset = eth_offset;
    struct iphdr *ip = (struct iphdr *)(packet + ip_offset);
    int udp_offset = ip_offset + (ip->ihl * 4);
    struct udphdr *udp = (struct udphdr *)(packet + udp_offset);
    int dhcp_offset = udp_offset + sizeof(struct udphdr);
    unsigned char *dhcp_start = (unsigned char *)(packet + dhcp_offset);
    int dhcp_len = ntohs(udp->len) - sizeof(struct udphdr);

    int options_end = -1;
    for (int i = dhcp_len - 1; i >= 0; i--)
    {
        if (dhcp_start[i] == 0xFF)
        { // End option
            options_end = i;
            break;
        }
    }
    if (options_end == -1)
    {
        syslog(LOG_DEBUG, "Could not find DHCP options end tag\n");
        return;
    }

    // Calculate new packet size: remove VLAN (-4), remove end tag (-1), add
    // Option 82, add end tag (+1)
    int orig_options_len = options_end;
    int new_len = orig_len - 4 - 1 + opt82_len + 1;
    unsigned char *new_packet = malloc(new_len);
    if (!new_packet)
    {
        syslog(LOG_ERR, "Failed to allocate memory for new packet\n");
        return;
    }

    // Copy Ethernet header
    struct ethhdr *new_eth = (struct ethhdr *)new_packet;
    memcpy(new_eth, eth, sizeof(struct ethhdr));
    new_eth->h_proto = vlan->h_vlan_encapsulated_proto;

    // Copy IP header
    struct iphdr *new_ip = (struct iphdr *)(new_packet + sizeof(struct ethhdr));
    memcpy(new_ip, ip, ip->ihl * 4);

    // Copy UDP header
    struct udphdr *new_udp =
        (struct udphdr *)(new_packet + sizeof(struct ethhdr) + (ip->ihl * 4));
    memcpy(new_udp, udp, sizeof(struct udphdr));

    // Copy DHCP payload up to options end, add Option 82, add end tag
    unsigned char *new_dhcp =
        (unsigned char *)(new_packet + sizeof(struct ethhdr) + (ip->ihl * 4) +
                          sizeof(struct udphdr));
    memcpy(new_dhcp, dhcp_start, orig_options_len);

    // Add Option 82
    int opt82_offset = orig_options_len;
    new_dhcp[opt82_offset++] = 82;            // Option code
    new_dhcp[opt82_offset++] = opt82_len - 2; // Option length

    // Sub-option 1: Circuit ID (BSSID:ESSID)
    new_dhcp[opt82_offset++] = 1;              // Sub-option code
    new_dhcp[opt82_offset++] = circuit_id_len; // Sub-option length
    memcpy(new_dhcp + opt82_offset, info->bssid, strlen(info->bssid));
    opt82_offset += strlen(info->bssid);
    new_dhcp[opt82_offset++] = ':';
    memcpy(new_dhcp + opt82_offset, info->essid, strlen(info->essid));
    opt82_offset += strlen(info->essid);

    // Sub-option 2: Remote ID (hostname)
    new_dhcp[opt82_offset++] = 2; // Sub-option code
    new_dhcp[opt82_offset++] = remote_id_len;
    memcpy(new_dhcp + opt82_offset, hostname, remote_id_len);
    opt82_offset += remote_id_len;

    // Add end tag
    new_dhcp[opt82_offset++] = 0xFF;

    // Update lengths
    new_ip->tot_len = htons(ntohs(ip->tot_len) + opt82_len);
    new_udp->len = htons(ntohs(udp->len) + opt82_len);

    // Recalculate IP checksum
    new_ip->check = 0;
    unsigned int sum = 0;
    unsigned short *ip_ptr = (unsigned short *)new_ip;
    for (int i = 0; i < ip->ihl * 2; i++)
    {
        sum += *ip_ptr++;
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    new_ip->check = ~sum;

    // Recalculate UDP checksum
    new_udp->check = 0;
    sum = 0;
    // Pseudo-header
    sum +=
        (ntohs(new_ip->saddr) & 0xFFFF) + (ntohs(new_ip->saddr >> 16) & 0xFFFF);
    sum +=
        (ntohs(new_ip->daddr) & 0xFFFF) + (ntohs(new_ip->daddr >> 16) & 0xFFFF);
    sum += htons(IPPROTO_UDP);
    sum += new_udp->len;

    // UDP header and data
    unsigned char *udp_start = (unsigned char *)new_udp;
    int udp_total_len = ntohs(new_udp->len);
    unsigned short *udp_ptr = (unsigned short *)udp_start;
    for (int i = 0; i < udp_total_len / 2; i++)
    {
        sum += *udp_ptr++;
    }
    if (udp_total_len % 2)
    {
        sum += *(unsigned char *)udp_ptr;
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    new_udp->check = ~sum;
    if (new_udp->check == 0)
        new_udp->check = 0xFFFF;

    // Send the packet
    struct sockaddr_ll socket_address = {0};
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ALL);

    for (int i = 0; i < port_map_size; i++)
    {
        if (!strcmp(info->upstream, port_map[i].name))
        {
            socket_address.sll_ifindex = port_map[i].ifindex;
            if (sendto(port_map[i].sock, new_packet, new_len, 0, (struct sockaddr *)&socket_address,
                       sizeof(socket_address)) < 0)
            {
                syslog(LOG_ERR, "Failed to send packet to %s: %s\n",
                       info->upstream, strerror(errno));
            }
            else
            {
                syslog(LOG_INFO,
                       "Successfully forwarded packet to %s (new length: %d)\n",
                       info->upstream, new_len);
            }
            break;
        }
    }

    free(new_packet);
}

void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGTERM:
    case SIGHUP:
        cleanup();
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    openlog("dhcp_inject:", LOG_PID | LOG_CONS, LOG_DAEMON);

    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    if (parse_uci_config() != 0)
    {
        syslog(LOG_ERR, "Failed to parse UCI configuration\n");
        cleanup();
        return 1;
    }

    for (int i = 0; i < iface_map_size; i++)
    {
        if (parse_iwinfo_by_essid(&iface_map[i]) != 0)
        {
            syslog(LOG_ERR, "Failed to get iface info for ESSID: %s\n", iface_map[i].essid);
            cleanup();
            return 1;
        }
        else
        {
            syslog(LOG_INFO, "iface_info[%d]: iface='%s', freq='%s', essid='%s', bssid='%s', upstream='%s', serial=%d\n",
                   i, iface_map[i].iface, iface_map[i].frequency, iface_map[i].essid, iface_map[i].bssid,
                   iface_map[i].upstream, iface_map[i].serial);
        }
    }

    if (setup_tc() != 0)
    {
        syslog(LOG_ERR, "Setup failed\n");
        cleanup();
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_live("ifb-inject", BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL)
    {
        syslog(LOG_ERR, "Couldn't open device ifb-inject: %s\n", errbuf);
        cleanup();
        return 1;
    }

    if (pcap_loop(handle, -1, process_packet, NULL) < 0)
    {
        syslog(LOG_ERR, "pcap_loop failed: %s\n", pcap_geterr(handle));
        cleanup();
        return 1;
    }

    cleanup();
    return 0;
}
