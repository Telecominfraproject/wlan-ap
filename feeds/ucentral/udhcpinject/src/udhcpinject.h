#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#define LEN_ESSID 32
#define LEN_BSSID 12
#define LEN_IFACE 15
#define LEN_FREQ 3

#define MAX_INTERFACES 48
#define MAX_PORTS 8

#define CONFIG_PATH "/etc/config/dhcpinject"

#define IWINFO_CMD "iwinfo | grep '\"%s\"$' -A2 | grep '.*Channel:.*\\(%s\\..*\\) GHz' -B2 | awk '/ESSID/{print $1} /Access Point/{gsub(/:/, \"\", $3); print $3}'"

static pcap_t 		*handle = NULL;

static int 			 iface_map_size = 0;
struct iface_info 	*iface_map = NULL;
static int 			 port_map_size = 0;
struct port_info 	*port_map = NULL;

// DHCP header structure
struct dhcp_packet {
    uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	struct in_addr ciaddr;
	struct in_addr yiaddr;
	struct in_addr siaddr;
	struct in_addr giaddr;
	uint8_t chaddr[16];
	char sname[64];
	char file[128];
	uint32_t magic;
	uint8_t options[];
};

// up4094v4094

// Structure to hold interface info
struct iface_info {
    char iface[LEN_IFACE + 1];
    char essid[LEN_ESSID + 1];
    char bssid[LEN_BSSID + 1];
	char upstream[LEN_IFACE + 1];
	char frequency[4];
    int serial;
};

struct port_info {
    char name[LEN_IFACE + 1];
    int sock;
    int ifindex;
};

// VLAN header structure
struct vlan_hdr {
    __be16 h_vlan_TCI;                // VLAN Tag Control Information
    __be16 h_vlan_encapsulated_proto; // Encapsulated protocol
};

char *get_hostname() {
    static char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    return hostname;
}
