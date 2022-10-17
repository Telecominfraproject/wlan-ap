// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <net/if.h>

#include <libubox/avl-cmp.h>

#include "spotfilter.h"

AVL_TREE(interfaces, avl_strcmp, false, NULL);

void interface_free(struct interface *iface)
{
	struct client *cl, *tmp;

	spotfilter_dns_free(iface);

	vlist_flush_all(&iface->devices);

	avl_for_each_element_safe(&iface->clients, cl, node, tmp)
		client_free(iface, cl);

	spotfilter_bpf_free(iface);

	avl_delete(&interfaces, &iface->node);
	free(iface->config);
	free(iface);
}

static void
interface_check_device(struct interface *iface, struct device *dev)
{
	int old_ifindex = dev->ifindex;

	dev->ifindex = if_nametoindex(device_name(dev));
	if (dev->ifindex != old_ifindex)
		spotfilter_bpf_set_device(iface, dev->ifindex, true);
}

static void
device_update_cb(struct vlist_tree *tree,
		 struct vlist_node *node_new,
		 struct vlist_node *node_old)
{
	struct interface *iface = container_of(tree, struct interface, devices);
	struct device *dev_new = container_of_safe(node_new, struct device, node);
	struct device *dev_old = container_of_safe(node_old, struct device, node);

	if (dev_new) {
		if (dev_old)
			dev_new->ifindex = dev_old->ifindex;
		interface_check_device(iface, dev_new);
	}

	if (dev_old) {
		if (!dev_new && dev_old->ifindex)
			spotfilter_bpf_set_device(iface, dev_old->ifindex, false);
		free(dev_old);
	}
}

static int
interface_parse_class(struct spotfilter_bpf_class *cdata, struct blob_attr *attr)
{
	enum {
		CLASS_ATTR_INDEX,
		CLASS_ATTR_DEV_MAC,
		CLASS_ATTR_MAC,
		CLASS_ATTR_REDIRECT,
		CLASS_ATTR_FWMARK,
		CLASS_ATTR_FWMARK_MASK,
		__CLASS_ATTR_MAX,
	};
	static const struct blobmsg_policy policy[__CLASS_ATTR_MAX] = {
		[CLASS_ATTR_INDEX] = { "index", BLOBMSG_TYPE_INT32 },
		[CLASS_ATTR_DEV_MAC] = { "device_macaddr", BLOBMSG_TYPE_STRING },
		[CLASS_ATTR_MAC] = { "macaddr", BLOBMSG_TYPE_STRING },
		[CLASS_ATTR_REDIRECT] = { "redirect", BLOBMSG_TYPE_STRING },
		[CLASS_ATTR_FWMARK] = { "fwmark", BLOBMSG_TYPE_INT32 },
		[CLASS_ATTR_FWMARK_MASK] = { "fwmark_mask", BLOBMSG_TYPE_INT32 },
	};
	struct blob_attr *tb[__CLASS_ATTR_MAX];
	struct blob_attr *cur;
	unsigned int index;

	if (blobmsg_type(attr) != BLOBMSG_TYPE_TABLE)
		return -1;

	blobmsg_parse(policy, __CLASS_ATTR_MAX, tb,
		      blobmsg_data(attr), blobmsg_len(attr));

	if ((cur = tb[CLASS_ATTR_INDEX]) != NULL)
		index = blobmsg_get_u32(cur);
	else
		return -1;

	if (index >= SPOTFILTER_NUM_CLASS)
		return -1;

	if ((cur = tb[CLASS_ATTR_MAC]) != NULL) {
		void *addr;

		addr = ether_aton(blobmsg_get_string(cur));
		if (!addr)
			goto invalid;

		memcpy(cdata->dest_mac, addr, sizeof(cdata->dest_mac));
		cdata->actions |= SPOTFILTER_ACTION_SET_DEST_MAC;
	} else if ((cur = tb[CLASS_ATTR_DEV_MAC]) != NULL) {
		const char *name = blobmsg_get_string(cur);
		struct ifreq ifr = {};
		int sock;
		int ret;

		if (strlen(name) > IFNAMSIZ)
			goto invalid;

		strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

		sock = socket(AF_INET, SOCK_DGRAM, 0);
		ret = ioctl(sock, SIOCGIFHWADDR, &ifr);
		if (ret < 0)
			perror("ioctl");
		close(sock);

		if (ret < 0)
			goto invalid;

		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			goto invalid;

		memcpy(cdata->dest_mac, ifr.ifr_hwaddr.sa_data, sizeof(cdata->dest_mac));
		cdata->actions |= SPOTFILTER_ACTION_SET_DEST_MAC;
	}

	if ((cur = tb[CLASS_ATTR_REDIRECT]) != NULL) {
		unsigned int ifindex = if_nametoindex(blobmsg_get_string(cur));

		if (!ifindex)
			goto invalid;

		cdata->redirect_ifindex = ifindex;
		cdata->actions |= SPOTFILTER_ACTION_REDIRECT;
	}

	if ((cur = tb[CLASS_ATTR_FWMARK_MASK]) != NULL)
		cdata->fwmark_mask = blobmsg_get_u32(cur);
	else
		cdata->fwmark_mask = ~0;

	if ((cur = tb[CLASS_ATTR_FWMARK]) != NULL) {
		cdata->fwmark_val = blobmsg_get_u32(cur);
		cdata->actions |= SPOTFILTER_ACTION_FWMARK;
	}

	cdata->actions |= SPOTFILTER_ACTION_VALID;
	return index;

invalid:
	cdata->actions = 0;
	return index;
}

static bool
__interface_check_whitelist(struct blob_attr *attr)
{
	enum {
		WL_ATTR_CLASS,
		WL_ATTR_HOSTS,
		__WL_ATTR_MAX
	};
	static const struct blobmsg_policy policy[__WL_ATTR_MAX] = {
		[WL_ATTR_CLASS] = { "class", BLOBMSG_TYPE_INT32 },
		[WL_ATTR_HOSTS] = { "hosts", BLOBMSG_TYPE_ARRAY },
	};
	struct blob_attr *tb[__WL_ATTR_MAX];

	blobmsg_parse(policy, __WL_ATTR_MAX, tb, blobmsg_data(attr), blobmsg_len(attr));

	if (!tb[WL_ATTR_CLASS] || !tb[WL_ATTR_HOSTS])
		return false;

	return blobmsg_check_array(tb[WL_ATTR_HOSTS], BLOBMSG_TYPE_STRING) >= 0;
}

static bool
interface_check_whitelist(struct blob_attr *attr)
{
	struct blob_attr *cur;
	int rem;

	if (blobmsg_check_array(attr, BLOBMSG_TYPE_TABLE) <= 0)
		return false;

	blobmsg_for_each_attr(cur, attr, rem) {
		if (!__interface_check_whitelist(cur))
			return false;
	}

	return true;
}

static void
interface_set_config(struct interface *iface, bool iface_init)
{
	enum {
		CONFIG_ATTR_CLASS,
		CONFIG_ATTR_WHITELIST,
		CONFIG_ATTR_ACTIVE_TIMEOUT,
		CONFIG_ATTR_CLIENT_AUTOCREATE,
		CONFIG_ATTR_CLIENT_AUTOREMOVE,
		CONFIG_ATTR_CLIENT_TIMEOUT,
		CONFIG_ATTR_DEFAULT_CLASS,
		CONFIG_ATTR_DEFAULT_DNS_CLASS,
		__CONFIG_ATTR_MAX,
	};
	static const struct blobmsg_policy policy[__CONFIG_ATTR_MAX] = {
		[CONFIG_ATTR_CLASS] = { "class", BLOBMSG_TYPE_ARRAY },
		[CONFIG_ATTR_WHITELIST] = { "whitelist", BLOBMSG_TYPE_ARRAY },
		[CONFIG_ATTR_ACTIVE_TIMEOUT] = { "active_timeout", BLOBMSG_TYPE_INT32 },
		[CONFIG_ATTR_CLIENT_TIMEOUT] = { "client_timeout", BLOBMSG_TYPE_INT32 },
		[CONFIG_ATTR_CLIENT_AUTOCREATE] = { "client_autocreate", BLOBMSG_TYPE_BOOL },
		[CONFIG_ATTR_CLIENT_AUTOREMOVE] = { "client_autoremove", BLOBMSG_TYPE_BOOL },
		[CONFIG_ATTR_DEFAULT_CLASS] = { "default_class", BLOBMSG_TYPE_INT32 },
		[CONFIG_ATTR_DEFAULT_DNS_CLASS] = { "default_dns_class", BLOBMSG_TYPE_INT32 },
	};
	struct blob_attr *tb[__CONFIG_ATTR_MAX];
	struct blob_attr *cur;
	uint32_t class_mask = 0;
	int i, rem;

	blobmsg_parse(policy, __CONFIG_ATTR_MAX, tb,
		      blobmsg_data(iface->config), blobmsg_len(iface->config));

	if ((cur = tb[CONFIG_ATTR_DEFAULT_CLASS]) != NULL &&
	    blobmsg_get_u32(cur) < SPOTFILTER_NUM_CLASS)
		iface->default_class = blobmsg_get_u32(cur);
	else
		iface->default_class = 0;

	if ((cur = tb[CONFIG_ATTR_DEFAULT_DNS_CLASS]) != NULL &&
	    blobmsg_get_u32(cur) < SPOTFILTER_NUM_CLASS)
		iface->default_dns_class = blobmsg_get_u32(cur);
	else
		iface->default_dns_class = 0;

	if ((cur = tb[CONFIG_ATTR_WHITELIST]) != NULL && interface_check_whitelist(cur))
		iface->whitelist = cur;
	else
		iface->whitelist = NULL;

	if ((cur = tb[CONFIG_ATTR_ACTIVE_TIMEOUT]) != NULL)
		iface->active_timeout = blobmsg_get_u32(cur);
	else
		iface->active_timeout = 300;

	if ((cur = tb[CONFIG_ATTR_CLIENT_TIMEOUT]) != NULL)
		iface->client_timeout = blobmsg_get_u32(cur);
	else
		iface->client_timeout = 30;

	if ((cur = tb[CONFIG_ATTR_CLIENT_AUTOCREATE]) != NULL)
		iface->client_autocreate = blobmsg_get_u8(cur);
	else
		iface->client_autocreate = true;

	if ((cur = tb[CONFIG_ATTR_CLIENT_AUTOREMOVE]) != NULL)
		iface->client_autoremove = blobmsg_get_u8(cur);
	else
		iface->client_autoremove = true;

	blobmsg_for_each_attr(cur, tb[CONFIG_ATTR_CLASS], rem) {
		struct spotfilter_bpf_class cdata = {};
		int index;

		index = interface_parse_class(&cdata, cur);
		if (index < 0)
			continue;

		if (iface_init ||
		    memcmp(&iface->cdata[index], &cdata, sizeof(cdata)) != 0) {
			memcpy(&iface->cdata[index], &cdata, sizeof(cdata));
			spotfilter_bpf_update_class(iface, index);
		}

		class_mask |= 1 << index;
	}

	for (i = 0; i < SPOTFILTER_NUM_CLASS; i++) {
		if (class_mask & (1 << i))
			continue;

		memset(&iface->cdata[i], 0, sizeof(iface->cdata[i]));
		spotfilter_bpf_update_class(iface, i);
	}
}

void interface_check_devices(void)
{
	struct interface *iface;
	struct device *dev;

	avl_for_each_element(&interfaces, iface, node) {
		interface_set_config(iface, false);

		vlist_for_each_element(&iface->devices, dev, node)
			interface_check_device(iface, dev);
	}
}

void interface_add(const char *name, struct blob_attr *config,
		   struct blob_attr *devices)
{
	struct interface *iface;
	struct blob_attr *cur;
	char *name_buf;
	bool iface_init = false;
	int rem;

	iface = avl_find_element(&interfaces, name, iface, node);
	if (!iface) {
		iface = calloc_a(sizeof(*iface), &name_buf, strlen(name) + 1);
		iface->node.key = strcpy(name_buf, name);
		vlist_init(&iface->devices, avl_strcmp, device_update_cb);
		client_init_interface(iface);
		spotfilter_dns_init(iface);

		if (spotfilter_bpf_load(iface)) {
			free(iface);
			return;
		}

		avl_insert(&interfaces, &iface->node);
		iface_init = true;
	}

	if (config && !blob_attr_equal(iface->config, config)) {
		free(iface->config);
		iface->config = blob_memdup(config);
		interface_set_config(iface, iface_init);
	}

	blobmsg_for_each_attr(cur, devices, rem) {
		struct device *dev;
		const char *name = blobmsg_get_string(cur);

		dev = calloc_a(sizeof(*dev), &name_buf, strlen(name) + 1);
		vlist_add(&iface->devices, &dev->node, strcpy(name_buf, name));
	}
}

void interface_done(void)
{
	struct interface *iface, *tmp;

	avl_for_each_element_safe(&interfaces, iface, node, tmp)
		interface_free(iface);
}
