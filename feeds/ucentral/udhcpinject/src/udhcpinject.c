#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/if.h>

#include <pcap.h>
#include <poll.h>
#include <signal.h>

#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
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

// Create the shared ifb-inject mirror interface (once). Returns 0 on success.
int setup_ifb()
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
    return 0;
}

// Install the ingress qdisc + DHCP redirect filter for a single resolved VAP.
// Returns 0 on success, -1 if the filter could not be installed (caller should
// leave the VAP unresolved and retry later).
int setup_tc_for_iface(struct iface_info *iface)
{
    char cmd[512];

    // Adding the ingress qdisc is idempotent-ish: it fails harmlessly if one
    // already exists, so we suppress its output and never treat it as fatal.
    // The filter add below is the operation we actually gate on.
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s ingress 2>/dev/null",
             iface->iface);
    system(cmd);

    snprintf(cmd, sizeof(cmd),
             "tc filter add dev %s ingress protocol ip pref 32 u32 "
             "match ip protocol 17 0xff "
             "match u16 0x0044 0xffff at 20 "
             "match u16 0x0043 0xffff at 22 "
             "match u8 0x01 0xff at 28 "
             "action vlan push id %d pipe "
             "action mirred egress mirror dev ifb-inject pipe "
             "action drop",
             iface->iface, iface->serial);
    if (system(cmd) != 0)
    {
        syslog(LOG_ERR, "Failed to setup tc for %s\n", iface->iface);
        return -1;
    }
    return 0;
}

// Attempt to resolve every not-yet-resolved VAP via iwinfo and, on success,
// install its tc redirect. Missing VAPs are NOT fatal: they are simply left
// unresolved so they can be retried later (e.g. DFS VAPs that appear only after
// CAC completes). Returns the number of VAPs still unresolved.
int resolve_and_setup()
{
    int unresolved = 0;

    for (int i = 0; i < iface_map_size; i++)
    {
        if (iface_map[i].resolved)
            continue;

        if (parse_iwinfo_by_essid(&iface_map[i]) != 0 || iface_map[i].iface[0] == '\0')
        {
            unresolved++;
            continue;
        }

        if (setup_tc_for_iface(&iface_map[i]) != 0)
        {
            // VAP exists but tc install failed; retry on the next pass.
            unresolved++;
            continue;
        }

        iface_map[i].resolved = 1;
        iface_map[i].ifindex = (int)if_nametoindex(iface_map[i].iface);
        syslog(LOG_INFO,
               "Resolved iface_info[%d]: iface='%s', freq='%s', essid='%s', bssid='%s', upstream='%s', serial=%d",
               i, iface_map[i].iface, iface_map[i].frequency, iface_map[i].essid,
               iface_map[i].bssid, iface_map[i].upstream, iface_map[i].serial);
    }

    return unresolved;
}

// Return 1 if our DHCP redirect filter (installed at "pref 32") is currently
// present on the given VAP's ingress, 0 otherwise. If the device is missing tc
// prints nothing (stderr suppressed) and we report absent.
int tc_filter_present(const char *iface)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc filter show dev %s ingress 2>/dev/null", iface);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return 0;

    int present = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        // Our filter renders as "... pref 32 u32 ...". Match "pref 32 " so we
        // don't accidentally match a higher pref like 320, and never confuse it
        // with the unrelated bridger bpf filter at a much higher pref.
        if (strstr(line, "pref 32 "))
        {
            present = 1;
            break;
        }
    }
    pclose(fp);
    return present;
}

// Re-check every already-resolved VAP and make sure its DHCP redirect is still
// installed. A radar/DFS event or wifi reload can tear down and recreate the AP
// netdevs (dropping the tc ingress filter) or flush the filter in place; either
// way DHCP stops being mirrored to ifb-inject and injection silently dies.
// Instead of inferring this from the ifindex (which misses a flush that keeps
// the same index), we check the actual invariant - is our filter present?
//
//   - device gone     -> mark unresolved so it re-resolves via iwinfo later
//   - filter missing   -> reinstall in place (the VAP name is stable, so no
//                         iwinfo lookup is needed). If that fails, fall back to
//                         unresolved so the iwinfo path retries next pass.
//
// Returns the number of VAPs that still need re-resolution (device gone or an
// in-place reinstall failed), so the caller knows to run resolve_and_setup().
int revalidate_resolved()
{
    int need_resolve = 0;

    for (int i = 0; i < iface_map_size; i++)
    {
        if (!iface_map[i].resolved)
            continue;

        if (if_nametoindex(iface_map[i].iface) == 0)
        {
            syslog(LOG_INFO,
                   "VAP '%s' (essid='%s') disappeared; will re-resolve",
                   iface_map[i].iface, iface_map[i].essid);
            iface_map[i].resolved = 0;
            iface_map[i].ifindex = 0;
            iface_map[i].iface[0] = '\0';
            need_resolve++;
            continue;
        }

        if (tc_filter_present(iface_map[i].iface))
            continue;

        syslog(LOG_INFO,
               "Redirect filter missing on VAP '%s' (essid='%s'); reinstalling",
               iface_map[i].iface, iface_map[i].essid);
        if (setup_tc_for_iface(&iface_map[i]) != 0)
        {
            // Could not reinstall right now; drop to unresolved and let the
            // iwinfo-based path retry on the next pass.
            iface_map[i].resolved = 0;
            iface_map[i].ifindex = 0;
            iface_map[i].iface[0] = '\0';
            need_resolve++;
        }
        else
        {
            // Refresh the recorded ifindex in case the netdev was recreated
            // with the same name but a new index.
            iface_map[i].ifindex = (int)if_nametoindex(iface_map[i].iface);
        }
    }

    return need_resolve;
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
        snprintf(iface->iface, LEN_IFACE + 1, "%s", output);
    }
    else
    {
        pclose(fp);
        return 1;
    }

    // Read the second line (BSSID)
    line = fgets(output, sizeof(output), fp);
    if (line)
    {
        output[strcspn(output, "\n")] = '\0'; // Remove trailing newline
        snprintf(iface->bssid, LEN_BSSID + 1, "%s", output);
    }
    else
    {
        pclose(fp);
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
    snprintf(info->essid, LEN_ESSID + 1, "%s", essid);
    snprintf(info->upstream, LEN_IFACE + 1, "%s", upstream);
    snprintf(info->frequency, sizeof(info->frequency), "%s", freq);
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

            // Get interface index. A zero here is NOT fatal: the upstream
            // 802.1q device may not be up yet, and netifd may later tear it
            // down and recreate it with a different ifindex. We therefore keep
            // the socket open and re-resolve the ifindex lazily at send time
            // (see process_packet), so a stale/absent index self-heals.
            int ifindex = if_nametoindex(upstream);
            if (ifindex == 0)
            {
                syslog(LOG_INFO, "Upstream %s not up yet; will resolve on demand\n", upstream);
            }

            // Store in port_map
            snprintf(port_map[port_map_size].name, LEN_IFACE + 1, "%s", upstream);
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

    // Reset checksum to 0 before recalculating
    new_ip->check = 0;

    // Calculate checksum
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)new_ip;
    int len = new_ip->ihl * 4;

    for (int i = 0; i < len / 2; i++) {
        sum += ntohs(ptr[i]);
    }

    // Fold 32-bit sum into 16 bits
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    new_ip->check = htons((uint16_t)~sum);

    // Reset checksum to 0 before recalculating
    new_udp->check = 0;

    // Build pseudo-header and compute checksum
    sum = 0;
    uint16_t udp_len = ntohs(new_udp->len);

    uint32_t saddr = ntohl(new_ip->saddr);
    uint32_t daddr = ntohl(new_ip->daddr);
    sum += (saddr >> 16) & 0xFFFF;
    sum += saddr & 0xFFFF;
    sum += (daddr >> 16) & 0xFFFF;
    sum += daddr & 0xFFFF;
    sum += IPPROTO_UDP;
    sum += udp_len;

    // Sum the UDP header + payload
    ptr = (uint16_t *)new_udp;
    int i;
    for (i = 0; i < udp_len / 2; i++) {
        sum += ntohs(ptr[i]);
    }

    // If odd length, pad last byte
    if (udp_len & 1) {
        sum += ((uint8_t *)new_udp)[udp_len - 1] << 8;
    }

    // Fold 32-bit sum into 16 bits
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    new_udp->check = htons((uint16_t)~sum);

    // UDP allows 0xFFFF instead of 0x0000 (0 means "no checksum")
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
            // Re-resolve the upstream ifindex on every injection. netifd can
            // tear down and recreate these 802.1q upstream devices (e.g. on a
            // wifi/network reload or a DFS channel change), assigning a new
            // ifindex. A cached ifindex then goes stale and sendto() fails with
            // ENXIO ("No such device or address") even though the device is up.
            unsigned int ifindex = if_nametoindex(port_map[i].name);
            if (ifindex == 0)
            {
                syslog(LOG_ERR, "Upstream %s not present, dropping packet: %s\n",
                       info->upstream, strerror(errno));
                break;
            }
            port_map[i].ifindex = ifindex;
            socket_address.sll_ifindex = ifindex;
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
        exit(0);
        break;
    default:
        break;
    }
}

// Emit a heartbeat naming the SSIDs whose VAP is still not up, so a long wait
// (e.g. a 600s DFS CAC) is visibly "waiting" rather than looking like a hang.
// The SSID list is best-effort and truncated if it does not fit; the count is
// authoritative and passed in by the caller.
void log_pending(int unresolved)
{
    char list[512];
    size_t off = 0;

    list[0] = '\0';
    for (int i = 0; i < iface_map_size; i++)
    {
        if (iface_map[i].resolved)
            continue;

        int w = snprintf(list + off, sizeof(list) - off, "%s'%s'",
                         off ? ", " : "", iface_map[i].essid);
        if (w < 0 || (size_t)w >= sizeof(list) - off)
            break; // buffer full; log what we have so far
        off += (size_t)w;
    }

    syslog(LOG_INFO, "Still waiting for %d SSID(s) to come up: %s", unresolved, list);
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

    if (setup_ifb() != 0)
    {
        syslog(LOG_ERR, "Setup failed\n");
        cleanup();
        return 1;
    }

    // Best-effort initial resolve. VAPs that are not up yet (e.g. 5 GHz DFS
    // VAPs still doing CAC) are not fatal; they are retried from the main loop
    // as they come online. This is what keeps the daemon out of a crash loop.
    int unresolved = resolve_and_setup();
    if (unresolved > 0)
    {
        syslog(LOG_INFO,
               "%d SSID(s) not up yet; will keep retrying every %d seconds",
               unresolved, RESOLVE_RETRY_INTERVAL);
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_live("ifb-inject", BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL)
    {
        syslog(LOG_ERR, "Couldn't open device ifb-inject: %s\n", errbuf);
        cleanup();
        return 1;
    }

    // We must service the resolve/heartbeat timers even when ifb-inject is
    // idle. libpcap's read timeout does NOT guarantee a return when no packets
    // arrive (on Linux the timer only runs once traffic starts), so a plain
    // blocking pcap loop would stall the timers - fatal here, since at startup
    // there may be no tc filters and hence no traffic at all until VAPs
    // resolve. Instead we put the handle in non-blocking mode and drive it from
    // poll() with our own 1s timeout, which fires regardless of packet arrival.
    if (pcap_setnonblock(handle, 1, errbuf) != 0)
    {
        syslog(LOG_ERR, "Failed to set non-blocking mode: %s\n", errbuf);
        cleanup();
        return 1;
    }
    int pcap_fd = pcap_get_selectable_fd(handle);

    time_t last_resolve = time(NULL);
    time_t last_heartbeat = time(NULL);
    while (1)
    {
        if (pcap_fd >= 0)
        {
            struct pollfd pfd = { .fd = pcap_fd, .events = POLLIN };
            int pr = poll(&pfd, 1, 1000);
            if (pr < 0)
            {
                if (errno == EINTR)
                    continue;
                syslog(LOG_ERR, "poll failed: %s\n", strerror(errno));
                break;
            }
        }
        else
        {
            // Fallback: fd not selectable (should not happen for a live Linux
            // capture). Sleep so we don't busy-spin, then drain in non-blocking
            // mode below.
            usleep(200000);
        }

        // Drain whatever is buffered. In non-blocking mode this returns
        // promptly (0 when nothing is available) rather than waiting.
        int n = pcap_dispatch(handle, -1, process_packet, NULL);
        if (n < 0)
        {
            syslog(LOG_ERR, "pcap_dispatch failed: %s\n", pcap_geterr(handle));
            break;
        }

        // Run the maintenance pass on a fixed cadence regardless of whether
        // everything is currently resolved. This is what lets us recover after
        // a radio restart: every resolved VAP is checked for its redirect
        // filter (reinstalled in place if flushed), and any VAP whose netdev
        // has gone is re-resolved via iwinfo once it returns.
        time_t now = time(NULL);
        if (now - last_resolve >= RESOLVE_RETRY_INTERVAL)
        {
            last_resolve = now;

            int need_resolve = revalidate_resolved();
            if (unresolved > 0 || need_resolve > 0)
            {
                unresolved = resolve_and_setup();
                if (unresolved == 0)
                    syslog(LOG_INFO, "All configured SSIDs are now resolved");
            }
        }
        if (unresolved > 0 && now - last_heartbeat >= HEARTBEAT_INTERVAL)
        {
            last_heartbeat = now;
            log_pending(unresolved);
        }
    }

    cleanup();
    return 1;
}
