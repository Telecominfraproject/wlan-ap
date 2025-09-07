#include "udevmand.h"

int
avl_mac_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, ETH_ALEN);
}

struct avl_tree mac_tree = AVL_TREE_INIT(mac_tree, avl_mac_cmp, false, NULL);

struct mac*
mac_find(uint8_t *addr)
{
	struct mac *mac;
	uint8_t *addr_buf;

	mac = avl_find_element(&mac_tree, addr, mac, avl);

	if (mac)
		return mac;

	mac = calloc_a(sizeof(struct mac), &addr_buf, 6);
	if (!mac)
		return NULL;
	mac->addr = memcpy(addr_buf, addr, ETH_ALEN);
	mac->avl.key = mac->addr;
	mac->ethers = NULL;
	*mac->interface = '\0';
	INIT_LIST_HEAD(&mac->neigh4);
	INIT_LIST_HEAD(&mac->neigh6);
	INIT_LIST_HEAD(&mac->dhcpv4);
	INIT_LIST_HEAD(&mac->bridge_mac);

	avl_insert(&mac_tree, &mac->avl);
	ULOG_INFO("new mac "MAC_FMT"\n", MAC_VAR(mac->addr));

	return mac;
}

void
mac_update(struct mac *mac, char *iface)
{
	char *interface = interface_resolve(iface);

	if (iface && *iface)
		strncpy(mac->interface, interface, sizeof(mac->interface));

	clock_gettime(CLOCK_MONOTONIC, &mac->ts);
}

void
mac_dump(struct mac *mac, int interface)
{
	struct timespec ts;
	time_t last_seen;
	char buf[18];
	void *c, *d;

	neigh_enum();

	clock_gettime(CLOCK_MONOTONIC, &ts);

	last_seen = ts.tv_sec - mac->ts.tv_sec;
	snprintf(buf, sizeof(buf), MAC_FMT, MAC_VAR(mac->addr));
	c = blobmsg_open_table(&b, buf);
	if (interface && *mac->interface)
		blobmsg_add_string(&b, "interface", mac->interface);
	if (mac->ethers)
		blobmsg_add_string(&b, "ethers", mac->ethers);
	if (last_seen < 5 * 60)
		blobmsg_add_u32(&b, "last_seen", last_seen);
	else
		blobmsg_add_u8(&b, "offline", 1);
	if (!list_empty(&mac->neigh4)) {
		struct neigh *neigh;

		d = blobmsg_open_array(&b, "ipv4");
		list_for_each_entry(neigh, &mac->neigh4, list)
			blobmsg_add_ipv4(&b, NULL, neigh->ip);
		blobmsg_close_array(&b, d);
	}

	if (!list_empty(&mac->neigh6)) {
		struct neigh *neigh;

		d = blobmsg_open_array(&b, "ipv6");
		list_for_each_entry(neigh, &mac->neigh6, list)
			blobmsg_add_ipv6(&b, NULL, neigh->ip);
		blobmsg_close_array(&b, d);
	}

	if (!list_empty(&mac->dhcpv4)) {
		struct dhcpv4 *dhcpv4;

		d = blobmsg_open_array(&b, "dhcpv4");
		list_for_each_entry(dhcpv4, &mac->dhcpv4, mac) {
			blobmsg_add_ipv4(&b, NULL, dhcpv4->ip);
			if (strlen(dhcpv4->name) > 0)
				blobmsg_add_string(&b, NULL, dhcpv4->name);
			break;
		}
		blobmsg_close_array(&b, d);
	}

	if (!list_empty(&mac->bridge_mac)) {
		struct bridge_mac *bridge_mac;

		d = blobmsg_open_array(&b, "fdb");
		list_for_each_entry(bridge_mac, &mac->bridge_mac, mac)
			blobmsg_add_string(&b, NULL, bridge_mac->ifname);
		blobmsg_close_array(&b, d);
	}

	blobmsg_close_array(&b, c);
	neigh_flush();
}

static void
mac_flush(struct mac *mac, struct timespec ts)
{
	time_t last_seen;

	last_seen = ts.tv_sec - mac->ts.tv_sec;
	if (last_seen <  60 * 60)
		return;

	if (!list_empty(&mac->dhcpv4)) {
		struct dhcpv4 *dhcpv4, *t;

		list_for_each_entry_safe(dhcpv4, t, &mac->dhcpv4, mac)
			dhcpv4_del(dhcpv4);
	}

	if (!list_empty(&mac->bridge_mac)) {
		struct bridge_mac *bridge_mac, *t;

		list_for_each_entry_safe(bridge_mac, t, &mac->bridge_mac, mac)
			bridge_mac_del(bridge_mac);
	}
}

int
mac_dump_all(void)
{
	struct mac *mac, *t;
	struct timespec ts;

	blob_buf_init(&b, 0);

	avl_for_each_element(&mac_tree, mac, avl)
		mac_dump(mac, 1);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	avl_for_each_element_safe(&mac_tree, mac, avl, t)
		mac_flush(mac, ts);

	return 0;
}
