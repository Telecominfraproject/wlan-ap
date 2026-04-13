// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2011, 2012 Patrick McHardy <kaber@trash.net>
 */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter/nf_tables.h>
#include <net/ipv6.h>
#include <net/netfilter/nf_tables.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
#include <net/netfilter/nf_conntrack_nptv6_ext.h>
#endif

#ifdef CONFIG_NF_TABLES_IPV6

struct nft_npt {
	u8 			sreg_src_pfx;
	u8			sreg_dst_pfx;
	u8			src_pfx_len;
	u8			dst_pfx_len;
	enum nf_nat_manip_type	type:8;
	u16			flags;
	u8			family;
};

static void nft_npt_info_fill(struct nf_npt_info *npt, const struct nft_expr *expr,
				struct nft_regs *regs, const struct nft_pktinfo *pkt)
{
	struct nft_npt *priv = nft_expr_priv(expr);

	memcpy(npt->src_pfx.ip6, &regs->data[priv->sreg_src_pfx], sizeof(npt->src_pfx.ip6));
	memcpy(npt->dst_pfx.ip6, &regs->data[priv->sreg_dst_pfx], sizeof(npt->dst_pfx.ip6));
	npt->src_pfx_len = (__u8) priv->src_pfx_len;
	npt->dst_pfx_len = (__u8) priv->dst_pfx_len;
}

static bool nft_npt_verify_addr(uint8_t pfx_len, struct in6_addr *addr)
{
	/*
	 * 64 bit IID of IPv6 address cannot be All 0's.
	 */
	if (!addr->s6_addr16[4] && !addr->s6_addr16[5] && !addr->s6_addr16[6] && !addr->s6_addr16[7])
		return false;

	/*
	 * 64 bit IID of IPv6 address cannot be All 1's.
	 */
	if ((addr->s6_addr16[4] == 0xFFFF) && (addr->s6_addr16[5] == 0xFFFF) &&
		(addr->s6_addr16[6] == 0xFFFF) && (addr->s6_addr16[7] == 0xFFFF))
		return false;

	/*
	 * 16 bit Subnet ID if IPv6 address cannot be ALL 1's.
	 */
	if ((pfx_len <= 48) && (addr->s6_addr16[3] == 0xFFFF))
		return false;

	return true;
}

static int nft_npt_adjust_csum(struct nf_npt_info *npt)
{
	struct in6_addr pfx;
	__wsum src_sum, dst_sum;

	if (npt->src_pfx_len > 64 || npt->dst_pfx_len > 64)
		return -EINVAL;

	/* Ensure that LSB of prefix is zero */
	ipv6_addr_prefix(&pfx, &npt->src_pfx.in6, npt->src_pfx_len);
	if (!ipv6_addr_equal(&pfx, &npt->src_pfx.in6))
		return -EINVAL;

	ipv6_addr_prefix(&pfx, &npt->dst_pfx.in6, npt->dst_pfx_len);
	if (!ipv6_addr_equal(&pfx, &npt->dst_pfx.in6))
		return -EINVAL;

	src_sum = csum_partial(&npt->src_pfx.in6, sizeof(npt->src_pfx.in6), 0);
	dst_sum = csum_partial(&npt->dst_pfx.in6, sizeof(npt->dst_pfx.in6), 0);

	npt->adjustment = ~csum_fold(csum_sub(src_sum, dst_sum));
	return 0;
}

static bool nft_npt_map_pfx(struct nf_npt_info *npt, struct in6_addr *addr)
{
	unsigned int pfx_len;
	unsigned int i, idx;
	__be32 mask;
	__sum16 sum;

	pfx_len = max(npt->src_pfx_len, npt->dst_pfx_len);
	for (i = 0; i < pfx_len; i += 32) {
		if (pfx_len - i >= 32)
			mask = 0;
		else
			mask = htonl((1 << (i - pfx_len + 32)) - 1);

		idx = i / 32;
		addr->s6_addr32[idx] &= mask;
		addr->s6_addr32[idx] |= ~mask & npt->dst_pfx.in6.s6_addr32[idx];
	}

	if (pfx_len <= 48)
		idx = 3;
	else {
		for (idx = 4; idx < ARRAY_SIZE(addr->s6_addr16); idx++) {
			if ((__force __sum16)addr->s6_addr16[idx] !=
			    CSUM_MANGLED_0)
				break;
		}
		if (idx == ARRAY_SIZE(addr->s6_addr16))
			return false;
	}

	sum = ~csum_fold(csum_add(csum_unfold((__force __sum16)addr->s6_addr16[idx]),
				  csum_unfold(npt->adjustment)));
	if (sum == CSUM_MANGLED_0)
		sum = 0;
	*(__force __sum16 *)&addr->s6_addr16[idx] = sum;

	return true;
}

static void nft_npt_src_trans(const struct nft_expr *expr, struct nft_regs *regs, const struct nft_pktinfo *pkt)
{
	struct nf_nat_range2 newrange = {0};
	struct nf_npt_info npt;
	struct nf_conn *ct;
	union nf_inet_addr new_addr;
	enum ip_conntrack_info ctinfo;
	struct sk_buff *skb = pkt->skb;

	ct = nf_ct_get(skb, &ctinfo);
	nft_npt_info_fill(&npt, expr, regs, pkt);

	if (!nft_npt_verify_addr(npt.src_pfx_len, &ipv6_hdr(skb)->saddr)) {
		printk("%px: Invalid Source IP received for NPTv6 Translation\n", skb);
		return;
	}

	if (nft_npt_adjust_csum(&npt)) {
		printk("%px: Failed to Calculate csum Adjustment for NPTv6 Translation\n", skb);
		return;
	}

	if (!nft_npt_map_pfx(&npt, &ipv6_hdr(skb)->saddr)) {
		icmpv6_send(skb, ICMPV6_PARAMPROB, ICMPV6_HDR_FIELD,
				offsetof(struct ipv6hdr, saddr));
		return;
	}

	if (ct->status & IPS_SRC_NAT_DONE) {
		return;
	}

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
	nf_conntrack_nptv6_ext_set_snpt(ct, &npt);
#endif
	new_addr.in6 = ipv6_hdr(skb)->saddr;
	newrange.flags = newrange.flags | NF_NAT_RANGE_MAP_IPS;
	newrange.min_addr = new_addr;
	newrange.max_addr = new_addr;
	regs->verdict.code = nf_nat_setup_info(ct, &newrange, NF_NAT_MANIP_SRC);
}

static void nft_npt_dst_trans(const struct nft_expr *expr, struct nft_regs *regs, const struct nft_pktinfo *pkt)
{
	struct nf_nat_range2 newrange = {0};
	struct nf_npt_info npt;
	struct nf_conn *ct;
	union nf_inet_addr new_addr;
	enum ip_conntrack_info ctinfo;
	struct sk_buff *skb = pkt->skb;

	ct = nf_ct_get(skb, &ctinfo);
	nft_npt_info_fill(&npt, expr, regs, pkt);

	if (!nft_npt_verify_addr(npt.dst_pfx_len, &ipv6_hdr(skb)->daddr)) {
		printk("%px: Invalid Destination IP received for NPTv6 Translation\n", skb);
		return;
	}

	if (nft_npt_adjust_csum(&npt)) {
		printk("%px: Failed to Calculate csum Adjustment for NPTv6 Translation\n", skb);
		return;
	}

	if (!nft_npt_map_pfx(&npt, &ipv6_hdr(skb)->daddr)) {
		icmpv6_send(skb, ICMPV6_PARAMPROB, ICMPV6_HDR_FIELD,
				offsetof(struct ipv6hdr, daddr));
		return;
	}

	if (ct->status & IPS_DST_NAT_DONE) {
		return;
	}

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
	nf_conntrack_nptv6_ext_set_dnpt(ct, &npt);
#endif
	new_addr.in6 = ipv6_hdr(skb)->daddr;
	newrange.flags = newrange.flags | NF_NAT_RANGE_MAP_IPS;
	newrange.min_addr = new_addr;
	newrange.max_addr = new_addr;
	regs->verdict.code = nf_nat_setup_info(ct, &newrange, NF_NAT_MANIP_DST);
}

static void nft_npt_eval(const struct nft_expr *expr, struct nft_regs *regs, const struct nft_pktinfo *pkt)
{
	const struct nft_npt *priv = nft_expr_priv(expr);

	switch (priv->type) {
	case NF_NAT_MANIP_SRC:
		nft_npt_src_trans(expr, regs, pkt);
		break;
	case NF_NAT_MANIP_DST:
		nft_npt_dst_trans(expr, regs, pkt);
		break;
	default:
		printk("%px: Invalid Packet received\n", pkt->skb);
	}
}

static int nft_npt_validate(const struct nft_ctx *ctx, const struct nft_expr *expr, const struct nft_data **data)
{
	struct nft_npt *priv = nft_expr_priv(expr);
	int err;

	err = nft_chain_validate_dependency(ctx->chain, NFT_CHAIN_T_NAT);
	if (err < 0)
		return err;

	switch (priv->type) {
	case NF_NAT_MANIP_SRC:
		err = nft_chain_validate_hooks(ctx->chain,
						(1 << NF_INET_POST_ROUTING) |
						(1 << NF_INET_LOCAL_IN));
		break;
	case NF_NAT_MANIP_DST:
		err = nft_chain_validate_hooks(ctx->chain,
						(1 << NF_INET_PRE_ROUTING) |
						(1 << NF_INET_LOCAL_OUT));
		break;
	}

	return err;
}

static const struct nla_policy nft_npt_policy[NFTA_NPT_MAX + 1] = {
	[NFTA_NPT_TYPE]			= { .type = NLA_U32 },
	[NFTA_NPT_FAMILY]		= { .type = NLA_U32 },
	[NFTA_NPT_REG_SRC_PFX]		= { .type = NLA_U32 },
	[NFTA_NPT_REG_DST_PFX]		= { .type = NLA_U32 },
	[NFTA_NPT_REG_SRC_PFX_LEN]	= { .type = NLA_U8 },
	[NFTA_NPT_REG_DST_PFX_LEN]	= { .type = NLA_U8 },
};

static int nft_npt_init(const struct nft_ctx *ctx, const struct nft_expr *expr, const struct nlattr * const tb[])
{
	struct nft_npt *priv = nft_expr_priv(expr);
	unsigned int alen;
	u32 family;
	int err;

	if (tb[NFTA_NPT_TYPE] == NULL ||
		(tb[NFTA_NPT_REG_SRC_PFX] == NULL &&
		tb[NFTA_NPT_REG_DST_PFX] == NULL &&
		tb[NFTA_NPT_REG_SRC_PFX_LEN] == NULL &&
		tb[NFTA_NPT_REG_DST_PFX_LEN] == NULL))
		return -EINVAL;

	switch (ntohl(nla_get_be32(tb[NFTA_NPT_TYPE]))) {
	case NFT_NAT_SNPT:
		priv->type = NF_NAT_MANIP_SRC;
		break;
	case NFT_NAT_DNPT:
		priv->type = NF_NAT_MANIP_DST;
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (tb[NFTA_NPT_FAMILY] == NULL)
		return -EINVAL;

	family = ntohl(nla_get_be32(tb[NFTA_NPT_FAMILY]));
	if (ctx->family != NFPROTO_IPV6 && ctx->family != family)
		return -EOPNOTSUPP;

	alen = sizeof_field(struct nf_nat_range, min_addr.ip6);
	priv->family = family;

	if (tb[NFTA_NPT_REG_SRC_PFX]) {
		err = nft_parse_register_load(tb[NFTA_NPT_REG_SRC_PFX], &priv->sreg_src_pfx, alen);
		if (err < 0)
			return err;
	}

	if (tb[NFTA_NPT_REG_DST_PFX]) {
		err = nft_parse_register_load(tb[NFTA_NPT_REG_DST_PFX], &priv->sreg_dst_pfx, alen);
		if (err < 0)
			return err;
	}

	if (tb[NFTA_NPT_REG_SRC_PFX_LEN]) {
		priv->src_pfx_len = nla_get_u8(tb[NFTA_NPT_REG_SRC_PFX_LEN]);
	}

	if (tb[NFTA_NPT_REG_DST_PFX_LEN]) {
		priv->dst_pfx_len = nla_get_u8(tb[NFTA_NPT_REG_DST_PFX_LEN]);
	}

	return nf_ct_netns_get(ctx->net, family);
}

static int nft_npt_dump(struct sk_buff *skb, const struct nft_expr *expr, bool reset)
{
	const struct nft_npt *priv = nft_expr_priv(expr);

	switch (priv->type) {
	case NF_NAT_MANIP_SRC:
		if (nla_put_be32(skb, NFTA_NPT_TYPE, htonl(NFT_NAT_SNPT)))
			goto nla_put_failure;
		break;
	case NF_NAT_MANIP_DST:
		if (nla_put_be32(skb, NFTA_NPT_TYPE, htonl(NFT_NAT_DNPT)))
			goto nla_put_failure;
		break;
        }

	if (nla_put_be32(skb, NFTA_NPT_FAMILY, htonl(priv->family)))
		goto nla_put_failure;

	if (priv->sreg_src_pfx) {
		if (nft_dump_register(skb, NFTA_NPT_REG_SRC_PFX, priv->sreg_src_pfx))
			goto nla_put_failure;
	}

	if (priv->sreg_dst_pfx) {
		if (nft_dump_register(skb, NFTA_NPT_REG_DST_PFX, priv->sreg_dst_pfx))
			goto nla_put_failure;
	}

	if (priv->src_pfx_len) {
		if (nla_put_s8(skb, NFTA_NPT_REG_SRC_PFX_LEN, priv->src_pfx_len))
			goto nla_put_failure;
	}

	if (priv->dst_pfx_len) {
		if (nla_put_s8(skb, NFTA_NPT_REG_DST_PFX_LEN, priv->dst_pfx_len))
			goto nla_put_failure;
	}

	return 0;

nla_put_failure:
	return -1;
}

static void nft_npt_ipv6_destroy(const struct nft_ctx *ctx, const struct nft_expr *expr)
{
	nf_ct_netns_put(ctx->net, NFPROTO_IPV6);
}

static struct nft_expr_type nft_npt_ipv6_type;
static const struct nft_expr_ops nft_npt_ipv6_ops = {
	.type		= &nft_npt_ipv6_type,
	.size		= NFT_EXPR_SIZE(sizeof(struct nft_npt)),
	.eval		= nft_npt_eval,
	.init		= nft_npt_init,
	.destroy	= nft_npt_ipv6_destroy,
	.dump		= nft_npt_dump,
	.validate	= nft_npt_validate,
	.reduce		= NFT_REDUCE_READONLY,
};

static struct nft_expr_type nft_npt_ipv6_type __read_mostly = {
        .family         = NFPROTO_IPV6,
        .name           = "npt",
        .ops            = &nft_npt_ipv6_ops,
        .policy         = nft_npt_policy,
        .maxattr        = NFTA_NPT_MAX,
        .owner          = THIS_MODULE,
};

static int __init nft_npt_module_init_ipv6(void)
{
	return nft_register_expr(&nft_npt_ipv6_type);
}

static void nft_npt_module_exit_ipv6(void)
{
	nft_unregister_expr(&nft_npt_ipv6_type);
}
#else
static inline int nft_npt_module_init_ipv6(void) { return 0; }
static inline void nft_npt_module_exit_ipv6(void) {}
#endif

static int __init nft_npt_module_init(void)
{
	int ret;

	ret = nft_npt_module_init_ipv6();
	if (ret < 0)
		return ret;

	return ret;
}

static void __exit nft_npt_module_exit(void)
{
	nft_npt_module_exit_ipv6();
}

module_init(nft_npt_module_init);
module_exit(nft_npt_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS_NFT_EXPR("npt");
MODULE_DESCRIPTION("Netfilter nftables Network Prefix Translation(NPT) expression support");
