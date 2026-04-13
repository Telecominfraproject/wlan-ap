/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* NPTv6 conntrack extension APIs. */

#ifndef _NF_CONNTRACK_NPTV6_EXT_H
#define _NF_CONNTRACK_NPTV6_EXT_H
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_nat.h>

#define NF_CT_NPTV6_EXT_SNPT	0x1
#define NF_CT_NPTV6_EXT_DNPT	0x2

/*
 * NPTv6 Conntrack Extension Structure
 */
struct nf_ct_nptv6_ext {
	__u32 src_pfx[4];	/* Source prefix of the translator */
	__u32 dst_pfx[4];	/* Destination prefix of the translator */
	__u16 nptv6_flags;	/* Flags used by the translator */
	__u8 src_pfx_len;	/* Source Prefix length */
	__u8 dst_pfx_len;	/* Destination Prefix length */
};

/*
 * nf_ct_nptv6_ext_find()
 *	Finds the extension data of the conntrack entry if it exists.
 */
static inline struct nf_ct_nptv6_ext *nf_ct_nptv6_ext_find(const struct nf_conn *ct)
{
#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
	return nf_ct_ext_find(ct, NF_CT_EXT_NPTV6);
#else
	return NULL;
#endif
}

/*
 * nf_ct_nptv6_ext_add()
 *	Adds the extension data to the conntrack entry.
 */
static inline struct nf_ct_nptv6_ext *nf_ct_nptv6_ext_add(struct nf_conn *ct, gfp_t gfp)
{
#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
	return nf_ct_ext_add(ct, NF_CT_EXT_NPTV6, gfp);
#else
	return NULL;
#endif
}

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
extern int nf_conntrack_nptv6_ext_set_snpt(struct nf_conn *ct, struct nf_npt_info *npt);
extern int nf_conntrack_nptv6_ext_set_dnpt(struct nf_conn *ct, struct nf_npt_info *npt);
#endif
#endif /* _NF_CONNTRACK_NPTV6_EXT_H */
