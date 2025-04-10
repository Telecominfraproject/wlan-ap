#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include "udhcpinject.h"

#define MAX_INTERFACES 48
#define MAX_PORTS 8

// Global variables
struct iface_info *iface_map = NULL;
static struct port_info *ports = NULL;
int iface_count = 0;
int port_count = 0;
static pcap_t *handle = NULL;
static char *provided_ssids = NULL;
static char *provided_ports = NULL;

// Function to cleanup tc rules
void cleanup_tc() {
    char cmd[1024];
    for (int i = 0; i < iface_count; i++) {
        snprintf(cmd, sizeof(cmd), "tc filter del dev %s ingress 2>/dev/null",
                 iface_map[i].iface);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s ingress 2>/dev/null",
                 iface_map[i].iface);
        system(cmd);
    }
}

// Cleanup function
void cleanup() {
    syslog(LOG_INFO, "Cleaning up resources...\n");
    cleanup_tc();

    if (handle) {
        pcap_close(handle);
        handle = NULL;
    }
    if (ports) {
        for (int i = 0; i < port_count; i++) {
            if (ports[i].sock >= 0) {
                close(ports[i].sock);
            }
        }
        free(ports);
        ports = NULL;
        port_count = 0;
    }
    if (iface_map) {
        free(iface_map);
        iface_map = NULL;
        iface_count = 0;
    }
    if (provided_ssids) {
        free(provided_ssids);
        provided_ssids = NULL;
    }
    if (provided_ports) {
        free(provided_ports);
        provided_ports = NULL;
    }
    syslog(LOG_INFO, "Cleanup complete.\n");
}

// Function to parse SSIDs and populate iface_map
int parse_ssids(const char *ssids) {
    if (iface_map) {
        free(iface_map);
        iface_map = NULL;
        iface_count = 0;
    }

    // Create a set of provided SSIDs for efficient lookup
    char ssids_copy[256];
    strncpy(ssids_copy, ssids, sizeof(ssids_copy) - 1);
    ssids_copy[sizeof(ssids_copy) - 1] = '\0';

    // Count number of SSIDs for allocation
    int ssid_count = 1; // Start at 1 for first SSID
    for (int i = 0; ssids_copy[i]; i++) {
        if (ssids_copy[i] == ',')
            ssid_count++;
    }

    char **ssid_set = malloc(ssid_count * sizeof(char *));
    if (!ssid_set) {
        syslog(LOG_ERR, "Failed to allocate memory for SSID set\n");
        return -1;
    }

    int ssid_idx = 0;
    char *token = strtok(ssids_copy, ",");
    while (token) {
        ssid_set[ssid_idx++] = token;
        token = strtok(NULL, ",");
    }

    // Execute iwinfo command and capture output
    FILE *pipe = popen("iwinfo | grep wlan -A1 | grep -v \"^--\" | tr -d '\"' "
                       "| awk '/wlan/ {name=$1; essid=$3} /Access Point/ "
                       "{print name \"=\" essid \",\" $3}' | tr -d ':'",
                       "r");
    if (!pipe) {
        syslog(LOG_ERR, "Failed to execute iwinfo command\n");
        free(ssid_set);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), pipe) != NULL) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        // Parse line format: wlanX=SSID,BSSID
        char *iface = strtok(line, "=");
        char *rest = strtok(NULL, "=");
        if (!iface || !rest)
            continue;

        char *essid = strtok(rest, ",");
        char *bssid = strtok(NULL, ",");
        if (!essid || !bssid)
            continue;

        // Check if this SSID is in our provided set
        int match = 0;
        for (int i = 0; i < ssid_idx; i++) {
            if (strcmp(essid, ssid_set[i]) == 0) {
                match = 1;
                break;
            }
        }

        if (!match)
            continue;

        // Add matching interface to iface_map
        if (iface_count >= MAX_INTERFACES) {
            syslog(LOG_ERR, "Too many matching interfaces, max is %d\n",
                   MAX_INTERFACES);
            pclose(pipe);
            free(ssid_set);
            return -1;
        }

        iface_map =
            realloc(iface_map, (iface_count + 1) * sizeof(struct iface_info));
        if (!iface_map) {
            syslog(LOG_ERR, "Failed to reallocate iface_map\n");
            pclose(pipe);
            free(ssid_set);
            return -1;
        }

        struct iface_info *info = &iface_map[iface_count];
        info->serial = iface_count + 1;

        strncpy(info->iface, iface, LEN_IFACE);
        info->iface[LEN_IFACE] = '\0';

        strncpy(info->essid, essid, LEN_ESSID);
        info->essid[LEN_ESSID] = '\0';

        strncpy(info->bssid, bssid, LEN_BSSID);
        info->bssid[LEN_BSSID] = '\0';

        iface_count++;
    }

    int pipe_status = pclose(pipe);
    if (pipe_status == -1) {
        syslog(LOG_ERR, "Error closing iwinfo pipe: %s\n", strerror(errno));
    }

    free(ssid_set);

    if (iface_count == 0) {
        syslog(LOG_ERR, "No matching interfaces found for provided SSIDs\n");
        return -1;
    }

    syslog(LOG_INFO, "Found %d matching interfaces\n", iface_count);
    return 0;
}

int parse_ports(const char *port_list) {
    if (ports) {
        for (int i = 0; i < port_count; i++) {
            if (ports[i].sock >= 0) {
                close(ports[i].sock);
            }
        }
        free(ports);
        ports = NULL;
        port_count = 0;
    }

    char ports_copy[256];
    strncpy(ports_copy, port_list, sizeof(ports_copy) - 1);
    ports_copy[sizeof(ports_copy) - 1] = '\0';

    port_count = 1;
    for (int i = 0; ports_copy[i]; i++) {
        if (ports_copy[i] == ',') {
            port_count++;
        }
    }

    if (port_count > MAX_PORTS) {
        syslog(LOG_ERR, "Too many ports specified, maximum is %d\n", MAX_PORTS);
        return -1;
    }

    ports = calloc(port_count, sizeof(struct port_info));
    if (!ports) {
        syslog(LOG_ERR, "Failed to allocate memory for ports\n");
        return -1;
    }

    char *token = strtok(ports_copy, ",");
    int idx = 0;
    while (token && idx < port_count) {
        strncpy(ports[idx].name, token, LEN_IFACE);
        ports[idx].name[LEN_IFACE] = '\0';

        ports[idx].sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (ports[idx].sock < 0) {
            syslog(LOG_ERR, "Socket creation failed for %s: %s\n",
                   ports[idx].name, strerror(errno));
            return -1;
        }

        ports[idx].ifindex = if_nametoindex(ports[idx].name);
        if (ports[idx].ifindex == 0) {
            syslog(LOG_ERR, "Failed to get interface index for %s: %s\n",
                   ports[idx].name, strerror(errno));
            return -1;
        }

        token = strtok(NULL, ",");
        idx++;
    }

    syslog(LOG_INFO, "Configured %d ports for forwarding\n", port_count);
    return 0;
}

// Function to setup tc rules (same as before but using iface_map)
int setup_tc() {
    char cmd[1024];

    snprintf(cmd, sizeof(cmd), "ip link show ifb-inject >/dev/null 2>&1");
    if (system(cmd) != 0) {
        snprintf(cmd, sizeof(cmd),
                 "ip link add name ifb-inject type ifb && ip link set "
                 "ifb-inject up");
        if (system(cmd) != 0) {
            syslog(LOG_ERR, "Failed to setup ifb-inject\n");
            return -1;
        }
    }

    for (int i = 0; i < iface_count; i++) {
        snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s ingress 2>/dev/null 1>2", 
            iface_map[i].iface);
        int result = system(cmd);
        if (result == 2) {
            syslog(LOG_INFO, "Ingress qdisc already exists for %s on %s\n", iface_map[i].essid, iface_map[i].iface);
        }
        else if (result == 1) {
            syslog(LOG_INFO, "Failed to add ingress qdisc for %s on %s\n", iface_map[i].essid, iface_map[i].iface);
        }

        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s ingress protocol ip u32 "
                 "match ip protocol 17 0xff "
                 "match u16 0x0044 0xffff at 20 "
                 "match u16 0x0043 0xffff at 22 "
                 "match u8 0x01 0xff at 28 "
                 "action vlan push id %d pipe "
                 "action mirred egress mirror dev ifb-inject pipe "
                 "action drop",
                 iface_map[i].iface, iface_map[i].serial);
        if (system(cmd) != 0) {
            syslog(LOG_ERR, "Failed to setup tc for %s\n", iface_map[i].iface);
            return -1;
        }
    }
    return 0;
}

// Signal handler
void signal_handler(int sig) {
    if (sig == SIGTERM) {
        syslog(LOG_INFO, "Received SIGTERM, cleaning up...\n");
        cleanup();
        exit(0);
    } else if (sig == SIGHUP) {
        syslog(LOG_INFO, "Received reload signal, reconfiguring...\n");
        sleep(5);
        // Clean up existing resources
        cleanup_tc();
        
        // Free old SSIDs and get new ones
        if (provided_ssids) {
            free(provided_ssids);
        }
        provided_ssids = getenv("SSIDs");
        if (!provided_ssids) {
            syslog(LOG_ERR, "No SSIDs provided on reload\n");
            return;
        }
        
        // Reload SSIDs
        if (parse_ssids(provided_ssids) != 0) {
            syslog(LOG_ERR, "Failed to reload SSIDs configuration\n");
            return;
        }
        
        // Free old ports and get new ones
        if (provided_ports) {
            free(provided_ports);
        }
        provided_ports = getenv("PORTs");
        if (!provided_ports) {
            syslog(LOG_ERR, "No PORTs provided on reload\n");
            return;
        }
        
        // Close existing sockets and reopen with new config
        if (ports) {
            for (int i = 0; i < port_count; i++) {
                if (ports[i].sock >= 0) {
                    close(ports[i].sock);
                }
            }
            free(ports);
            ports = NULL;
            port_count = 0;
        }
        
        // Reload ports
        if (parse_ports(provided_ports) != 0) {
            syslog(LOG_ERR, "Failed to reload ports configuration\n");
            return;
        }
        
        // Reapply tc rules
        if (setup_tc() == 0) {
            syslog(LOG_INFO, "Reloaded with SSIDs: %s and Ports: %s\n",
                   provided_ssids, provided_ports);
        } else {
            syslog(LOG_ERR, "Failed to reload tc configuration\n");
        }
    }
}

char *get_hostname() {
    static char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    return hostname;
}

struct iface_info *find_iface_info_by_vlan(int vlan_id) {
    for (int i = 0; i < iface_count; i++) {
        if (iface_map[i].serial == vlan_id) {
            return &iface_map[i];
        }
    }
    return NULL;
}

void process_packet(unsigned char *user, const struct pcap_pkthdr *header,
                    const unsigned char *packet) {
    int orig_len = header->len;
    struct ethhdr *eth = (struct ethhdr *)packet;
    int vlan_id = -1;
    int eth_offset = sizeof(struct ethhdr);

    if (ntohs(eth->h_proto) != ETH_P_8021Q) {
        syslog(LOG_DEBUG,
               "No VLAN header found in packet (EtherType: 0x%04x)\n",
               ntohs(eth->h_proto));
        return;
    }

    struct vlan_hdr *vlan = (struct vlan_hdr *)(packet + eth_offset);
    vlan_id = ntohs(vlan->h_vlan_TCI) & 0x0FFF;
    eth_offset += sizeof(struct vlan_hdr);

    struct iface_info *info = find_iface_info_by_vlan(vlan_id);
    if (!info) {
        syslog(LOG_ERR, "No interface info found for VLAN ID %d\n", vlan_id);
        return;
    }

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
    for (int i = dhcp_len - 1; i >= 0; i--) {
        if (dhcp_start[i] == 0xFF) { // End option
            options_end = i;
            break;
        }
    }
    if (options_end == -1) {
        syslog(LOG_DEBUG, "Could not find DHCP options end tag\n");
        return;
    }

    // Calculate new packet size: remove VLAN (-4), remove end tag (-1), add
    // Option 82, add end tag (+1)
    int orig_options_len = options_end;
    int new_len = orig_len - 4 - 1 + opt82_len + 1;
    unsigned char *new_packet = malloc(new_len);
    if (!new_packet) {
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
    for (int i = 0; i < ip->ihl * 2; i++) {
        sum += *ip_ptr++;
    }
    while (sum >> 16) {
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
    for (int i = 0; i < udp_total_len / 2; i++) {
        sum += *udp_ptr++;
    }
    if (udp_total_len % 2) {
        sum += *(unsigned char *)udp_ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    new_udp->check = ~sum;
    if (new_udp->check == 0)
        new_udp->check = 0xFFFF;

    // Send the packet
    struct sockaddr_ll socket_address = {0};
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ALL);

    for (int i = 0; i < port_count; i++) {
        socket_address.sll_ifindex = ports[i].ifindex;
        
        if (sendto(ports[i].sock, new_packet, new_len, 0,
                  (struct sockaddr *)&socket_address,
                  sizeof(socket_address)) < 0) {
            syslog(LOG_ERR, "Failed to send packet to %s: %s\n",
                   ports[i].name, strerror(errno));
        } else {
            syslog(LOG_DEBUG,
                   "Successfully forwarded packet to %s (new length: %d)\n",
                   ports[i].name, new_len);
        }
    }

    free(new_packet);
}

int main(int argc, char *argv[]) {
    openlog("dhcp_inject:", LOG_PID | LOG_CONS, LOG_DAEMON);

    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    provided_ssids = getenv("SSIDs");
    syslog(LOG_INFO, "Provided SSIDs: %s\n", provided_ssids);
    if (!provided_ssids && argc > 1) {
        provided_ssids = strdup(argv[1]);
    }
    if (!provided_ssids) {
        syslog(LOG_ERR, "No SSIDs provided. Exiting...\n");
        return 1;
    }

    provided_ports = getenv("PORTs");
    syslog(LOG_INFO, "Provided PORTs: %s\n", provided_ports);
    if (!provided_ports) {
        syslog(LOG_ERR, "No PORTs provided. Exiting...\n");
        cleanup();
        return 1;
    }

    sleep(5);
    if (parse_ssids(provided_ssids) != 0) {
        syslog(LOG_ERR, "Failed to parse SSIDs\n");
        cleanup();
        return 1;
    }

    if (parse_ports(provided_ports) != 0) {
        syslog(LOG_ERR, "Failed to parse ports\n");
        cleanup();
        return 1;
    }

    if (setup_tc() != 0) {
        syslog(LOG_ERR, "Setup failed\n");
        cleanup();
        return 1;
    }

    syslog(LOG_INFO, "Setup complete for SSIDs: %s and Ports: %s\n",
           provided_ssids, provided_ports);

    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_live("ifb-inject", BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        syslog(LOG_ERR, "Couldn't open device ifb-inject: %s\n", errbuf);
        cleanup();
        return 1;
    }

    if (pcap_loop(handle, -1, process_packet, NULL) < 0) {
        syslog(LOG_ERR, "pcap_loop failed: %s\n", pcap_geterr(handle));
        cleanup();
        return 1;
    }

    cleanup();
    return 0;
}