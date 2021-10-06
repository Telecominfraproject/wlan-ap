#define MAX_ETH_PORTS 5
#define TMP_VLAN_JSON "/tmp/vlans.json"

#define SCHEMA_VLAN_TRUNK_SZ            256
#define SCHEMA_VLAN_TRUNK_MAX          2

extern const char vlan_trunk_table[SCHEMA_VLAN_TRUNK_MAX][SCHEMA_VLAN_TRUNK_SZ];

struct eth_port_vlans {
	int vindex;
	int allowed_vlans[64];
	int pvid;
};

struct eth_port_state {
	char ifname[8];
	char state[8];
	char speed[8];
	char duplex[16];
	char bridge[8];
	struct eth_port_vlans vlans;
};

extern struct eth_port_state wanport;
extern struct eth_port_state lanport[];

int vlan_state_json_parse(void);
struct eth_port_state *get_eth_port(const char *ifname);
