#include <stdint.h>
#include <netinet/in.h>

#define LEN_ESSID 32
#define LEN_BSSID 12
#define LEN_IFACE 15

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

// Structure to hold interface info
struct iface_info {
    char iface[LEN_IFACE + 1];
    char essid[LEN_ESSID + 1];
    char bssid[LEN_BSSID + 1];
    int serial;
};

struct port_info {
    char name[LEN_IFACE + 1];
    int sock;
    int ifindex;
};

// VLAN header structure
struct vlan_hdr {
    __be16 h_vlan_TCI;              // VLAN Tag Control Information
    __be16 h_vlan_encapsulated_proto; // Encapsulated protocol
};
