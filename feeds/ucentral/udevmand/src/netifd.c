#include "udevmand.h"

enum {
	INTERFACE_DEVICE,
	INTERFACE_IFACE,
	__INTERFACE_MAX
};

static const struct blobmsg_policy interface_policy[__INTERFACE_MAX] = {
	[INTERFACE_DEVICE] = { .name = "device", .type = BLOBMSG_TYPE_STRING },
	[INTERFACE_IFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
};

static struct avl_tree interface_tree = AVL_TREE_INIT(interface_tree, avl_strcmp, false, NULL);

void
interface_update(struct blob_attr *msg, int raw)
{
	struct blob_attr *tb[__INTERFACE_MAX];
	struct interface *interface;
	char *device, *iface, *device_buf, *iface_buf;

	if (raw)
		blobmsg_parse(interface_policy, __INTERFACE_MAX, tb, blob_data(msg), blob_len(msg));
	else
		blobmsg_parse(interface_policy, __INTERFACE_MAX, tb, blobmsg_data(msg), blobmsg_data_len(msg));

	if (!tb[INTERFACE_DEVICE] || !tb[INTERFACE_IFACE])
		return;

	iface = blobmsg_get_string(tb[INTERFACE_IFACE]);
	device = blobmsg_get_string(tb[INTERFACE_DEVICE]);

	interface = avl_find_element(&interface_tree, iface, interface, avl);
	if (interface) {
		avl_delete(&interface_tree, &interface->avl);
		free(interface);
	}

	interface = calloc_a(sizeof(struct neigh),
			     &iface_buf, strlen(iface) + 1,
			     &device_buf, strlen(device) + 1);
        if (!interface)
                return;
	interface->iface = strcpy(iface_buf, iface);
	interface->device = strcpy(device_buf, device);
	interface->avl.key = interface->iface;

	avl_insert(&interface_tree, &interface->avl);

	ULOG_INFO("new interface %s:%s\n", iface, device);
}

void
interface_down(struct blob_attr *msg)
{
	struct blob_attr *tb[__INTERFACE_MAX];
	struct interface *interface;
	char *iface;

	blobmsg_parse(interface_policy, __INTERFACE_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[INTERFACE_IFACE])
		return;

	iface = blobmsg_get_string(tb[INTERFACE_IFACE]);

	interface = avl_find_element(&interface_tree, iface, interface, avl);
	if (!interface)
		return;

	ULOG_INFO("del interface %s\n", interface->iface);
	avl_delete(&interface_tree, &interface->avl);
	free(interface);
}

char*
interface_resolve(char *ifname)
{
	struct interface *interface;

	avl_for_each_element(&interface_tree, interface, avl)
		if (!strcmp(interface->device, ifname))
			return interface->iface;
	return ifname;
}

int
interface_dump(void)
{
	struct interface *interface;
	struct mac *mac;

	blob_buf_init(&b, 0);

	avl_for_each_element(&interface_tree, interface, avl) {
		void *c;

		c = blobmsg_open_table(&b, interface->iface);
		avl_for_each_element(&mac_tree, mac, avl)
			if (!strcmp(interface->iface, mac->interface))
				mac_dump(mac, 0);
		blobmsg_close_table(&b, c);
	}
	return 0;
}

void
interface_done(void)
{
	struct interface *i, *t;

	avl_for_each_element_safe(&interface_tree, i, avl, t)
		free(i);
}

