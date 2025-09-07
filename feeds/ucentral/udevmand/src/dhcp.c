#include "udevmand.h"

enum {
	DHCP_MAC,
	DHCP_IP,
	DHCP_NAME,
	DHCP_IFACE,
	__DHCP_MAX
};

static const struct blobmsg_policy dhcp_policy[__DHCP_MAX] = {
	[DHCP_MAC] = { .name = "mac", .type = BLOBMSG_TYPE_STRING },
	[DHCP_IP] = { .name = "ip", .type = BLOBMSG_TYPE_STRING },
	[DHCP_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
	[DHCP_IFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
};

static struct avl_tree dhcpv4_tree = AVL_TREE_INIT(dhcpv4_tree, avl_mac_cmp, false, NULL);

static void
dhcpv4_add(uint8_t *addr, uint8_t *ip, char *name, char *iface)
{
	struct dhcpv4 *dhcpv4;
	struct mac *mac;

	mac = mac_find(addr);

	dhcpv4 = avl_find_element(&dhcpv4_tree, addr, dhcpv4, avl);
	if (dhcpv4) {
		list_del(&dhcpv4->mac);
		avl_delete(&dhcpv4_tree, &dhcpv4->avl);
		free(dhcpv4);
	}

	dhcpv4 = malloc(sizeof(*dhcpv4) + (name ? strlen(name) + 1 : 1));
        if (!dhcpv4)
                return;
	if (iface)
		strncpy(dhcpv4->iface, iface, sizeof(dhcpv4->iface));
	else
		*dhcpv4->iface = '\0';
	if (name)
		strcpy(dhcpv4->name, name);
	else
		*dhcpv4->name = '\0';
	memcpy(dhcpv4->ip, ip, 4);
	memcpy(dhcpv4->addr, addr, ETH_ALEN);
	dhcpv4->avl.key = dhcpv4->addr;

	avl_insert(&dhcpv4_tree, &dhcpv4->avl);
	list_add(&dhcpv4->mac, &mac->dhcpv4);
	mac_update(mac, dhcpv4->iface);
	ULOG_INFO("new dhcpv4 " MAC_FMT "/" IP_FMT " for %s\n",
		  MAC_VAR(dhcpv4->addr), IP_VAR(ip),
		  strlen(dhcpv4->name) ? dhcpv4->name : "<unknown>");
}

void
dhcpv4_ack(struct blob_attr *msg)
{
	struct blob_attr *tb[__DHCP_MAX];
	uint8_t addr[ETH_ALEN], ip[4];
	char *name = NULL;
	int ret;

	blobmsg_parse(dhcp_policy, __DHCP_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[DHCP_MAC] || !tb[DHCP_IP] || !tb[DHCP_IFACE])
		return;

	ret = sscanf(blobmsg_get_string(tb[DHCP_MAC]), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		     &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if (ret != 6)
		return;

	ret = sscanf(blobmsg_get_string(tb[DHCP_IP]), "%hhu.%hhu.%hhu.%hhu",
		     &ip[0], &ip[1], &ip[2], &ip[3]);
	if (ret != 4)
		return;

	if (tb[DHCP_NAME])
		name = blobmsg_get_string(tb[DHCP_NAME]);

	dhcpv4_add(addr, ip, name, blobmsg_get_string(tb[DHCP_IFACE]));
}

void
dhcpv4_release(struct blob_attr *msg)
{
	struct blob_attr *tb[__DHCP_MAX];
	struct dhcpv4 *dhcpv4;
	uint8_t addr[ETH_ALEN];
	int ret;

	blobmsg_parse(dhcp_policy, __DHCP_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[DHCP_MAC])
		return;

	ret = sscanf(blobmsg_get_string(tb[DHCP_MAC]), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		     &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if (ret != 6)
		return;

	dhcpv4 = avl_find_element(&dhcpv4_tree, addr, dhcpv4, avl);
	if (!dhcpv4)
		return;
	ULOG_INFO("del dhcpv4 " MAC_FMT "\n", MAC_VAR(dhcpv4->addr));
	list_del(&dhcpv4->mac);
	avl_delete(&dhcpv4_tree, &dhcpv4->avl);
	free(dhcpv4);
}

void
dhcpv4_del(struct dhcpv4 *dhcpv4)
{
	list_del(&dhcpv4->mac);
	avl_delete(&dhcpv4_tree, &dhcpv4->avl);
	free(dhcpv4);
}

void
dhcp_init(void)
{
	FILE *fp = fopen("/tmp/dhcp.leases", "r");
	char line[1024];

	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp)) {
		uint8_t addr[ETH_ALEN], ip[4];
		char hostname[256 + 1];
		long int timestamp;
		int ret;

		ret = sscanf(line, "%ld %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %hhu.%hhu.%hhu.%hhu %s",
			     &timestamp,
			     &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5],
			     &ip[0], &ip[1], &ip[2], &ip[3],
			     hostname);
		if (ret != 12)
			continue;

		dhcpv4_add(addr, ip, hostname, NULL);
	}

	fclose(fp);
}

void
dhcp_done(void)
{
	struct dhcpv4 *d, *t;

	avl_for_each_element_safe(&dhcpv4_tree, d, avl, t) {
		avl_delete(&dhcpv4_tree, &d->avl);
		free(d);
	}
}

