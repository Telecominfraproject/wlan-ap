/*
 * Copyright 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 *
 * Backport functionality introduced in Linux 4.20.
 * Much of this is based on upstream lib/nlattr.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <net/genetlink.h>
#include <net/netlink.h>
#include <net/sock.h>

static const struct genl_family *find_family_real_ops(const struct genl_ops **ops)
{
	const struct genl_family *family;
	const struct genl_ops *tmp_ops = *ops;

	/* find the family ... */
	while (tmp_ops->doit || tmp_ops->dumpit)
		tmp_ops++;
	family = (void *)tmp_ops->done;

	/* cast to suppress const warning */
	*ops = (void *)(family->ops + (*ops - family->copy_ops));

	return family;
}

static int backport_pre_doit(const struct genl_ops *ops,
			     struct sk_buff *skb,
			     struct genl_info *info)
{
	const struct genl_family *family = find_family_real_ops(&ops);
	int err;
	struct netlink_ext_ack *extack = info->extack;

	if (ops->validate & GENL_DONT_VALIDATE_STRICT)
		err = nlmsg_validate_deprecated(info->nlhdr,
						GENL_HDRLEN + family->hdrsize,
						family->maxattr, family->policy,
						extack);
	else
		err = nlmsg_validate(info->nlhdr, GENL_HDRLEN + family->hdrsize,
				     family->maxattr, family->policy, extack);

	if (!err && family->pre_doit)
		err = family->pre_doit(ops, skb, info);

	return err;
}

static void backport_post_doit(const struct genl_ops *ops,
			       struct sk_buff *skb,
			       struct genl_info *info)
{
	const struct genl_family *family = find_family_real_ops(&ops);

	if (family->post_doit)
		family->post_doit(ops, skb, info);
}

int backport_genl_register_family(struct genl_family *family)
{
	struct genl_ops *ops;
	int err;

#define COPY(memb)	family->family.memb = family->memb
#define BACK(memb)	family->memb = family->family.memb

	/* we append one entry to the ops to find our family pointer ... */
	ops = kzalloc(sizeof(*ops) * (family->n_ops + 1), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;

	memcpy(ops, family->ops, sizeof(*ops) * family->n_ops);

	/* keep doit/dumpit NULL - that's invalid */
	ops[family->n_ops].done = (void *)family;

	COPY(id);
	memcpy(family->family.name, family->name, sizeof(family->name));
	COPY(hdrsize);
	COPY(version);
	COPY(maxattr);
	COPY(netnsok);
	COPY(parallel_ops);
	/* The casts are OK - we checked everything is the same offset in genl_ops */
	family->family.pre_doit = (void *)backport_pre_doit;
	family->family.post_doit = (void *)backport_post_doit;
	/* attrbuf is output only */
	family->copy_ops = (void *)ops;
	family->family.ops = (void *)ops;
	COPY(mcgrps);
	COPY(n_ops);
	COPY(n_mcgrps);
	COPY(module);

	err = __real_backport_genl_register_family(&family->family);

	BACK(id);
	BACK(attrbuf);

	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL_GPL(backport_genl_register_family);

int backport_genl_unregister_family(struct genl_family *family)
{
	return __real_backport_genl_unregister_family(&family->family);
}
EXPORT_SYMBOL_GPL(backport_genl_unregister_family);

#define INVALID_GROUP	0xffffffff

static u32 __backport_genl_group(const struct genl_family *family,
				 u32 group)
{
	if (WARN_ON_ONCE(group >= family->n_mcgrps))
		return INVALID_GROUP;
	return family->family.mcgrp_offset + group;
}

void genl_notify(const struct genl_family *family, struct sk_buff *skb,
		 struct genl_info *info, u32 group, gfp_t flags)
{
	struct net *net = genl_info_net(info);
	struct sock *sk = net->genl_sock;
	int report = 0;

	if (info->nlhdr)
		report = nlmsg_report(info->nlhdr);

	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return;
	nlmsg_notify(sk, skb, info->snd_portid, group, report, flags);
}
EXPORT_SYMBOL_GPL(genl_notify);

void *genlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
		  const struct genl_family *family, int flags, u8 cmd)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *hdr;

	nlh = nlmsg_put(skb, portid, seq, family->id, GENL_HDRLEN +
			family->hdrsize, flags);
	if (nlh == NULL)
		return NULL;

	hdr = nlmsg_data(nlh);
	hdr->cmd = cmd;
	hdr->version = family->version;
	hdr->reserved = 0;

	return (char *) hdr + GENL_HDRLEN;
}
EXPORT_SYMBOL_GPL(genlmsg_put);

void *genlmsg_put_reply(struct sk_buff *skb,
			struct genl_info *info,
			const struct genl_family *family,
			int flags, u8 cmd)
{
	return genlmsg_put(skb, info->snd_portid, info->snd_seq, family,
			   flags, cmd);
}
EXPORT_SYMBOL_GPL(genlmsg_put_reply);

int genlmsg_multicast_netns(const struct genl_family *family,
			    struct net *net, struct sk_buff *skb,
			    u32 portid, unsigned int group,
			    gfp_t flags)
{
	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return -EINVAL;
	return nlmsg_multicast(net->genl_sock, skb, portid, group, flags);
}
EXPORT_SYMBOL_GPL(genlmsg_multicast_netns);

int genlmsg_multicast(const struct genl_family *family,
		      struct sk_buff *skb, u32 portid,
		      unsigned int group, gfp_t flags)
{
	return genlmsg_multicast_netns(family, &init_net, skb,
				       portid, group, flags);
}
EXPORT_SYMBOL_GPL(genlmsg_multicast);

static int genlmsg_mcast(struct sk_buff *skb, u32 portid, unsigned long group,
			 gfp_t flags)
{
	struct sk_buff *tmp;
	struct net *net, *prev = NULL;
	bool delivered = false;
	int err;

	for_each_net_rcu(net) {
		if (prev) {
			tmp = skb_clone(skb, flags);
			if (!tmp) {
				err = -ENOMEM;
				goto error;
			}
			err = nlmsg_multicast(prev->genl_sock, tmp,
					      portid, group, flags);
			if (!err)
				delivered = true;
			else if (err != -ESRCH)
				goto error;
		}

		prev = net;
	}

	err = nlmsg_multicast(prev->genl_sock, skb, portid, group, flags);
	if (!err)
		delivered = true;
	else if (err != -ESRCH)
		return err;
	return delivered ? 0 : -ESRCH;
 error:
	kfree_skb(skb);
	return err;
}

int backport_genlmsg_multicast_allns(const struct genl_family *family,
				     struct sk_buff *skb, u32 portid,
				     unsigned int group, gfp_t flags)
{
	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return -EINVAL;
	return genlmsg_mcast(skb, portid, group, flags);
}
EXPORT_SYMBOL_GPL(backport_genlmsg_multicast_allns);
