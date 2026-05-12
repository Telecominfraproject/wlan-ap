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

/* NPTV6 handling conntrack extension registration. */

#include <linux/netfilter.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/export.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_nptv6_ext.h>

/*
 * nf_conntrack_nptv6_ext_set_snpt()
 *	Set the parameters for SNPT rule in the Extension.
 */
int nf_conntrack_nptv6_ext_set_snpt(struct nf_conn *ct, struct nf_npt_info *npt)
{
	struct nf_ct_nptv6_ext *npte;
	int i;

	spin_lock_bh(&ct->lock);
	npte = nf_ct_nptv6_ext_find(ct);
	if (!npte) {
		spin_unlock_bh(&ct->lock);
		return -1;
	}
	spin_unlock_bh(&ct->lock);

	npte->src_pfx_len = npt->src_pfx_len;
	npte->dst_pfx_len = npt->dst_pfx_len;
	npte->nptv6_flags = NF_CT_NPTV6_EXT_SNPT;

	for(i = 0; i < 4; i++) {
		npte->src_pfx[i] = htonl(npt->src_pfx.in6.s6_addr32[i]);
		npte->dst_pfx[i] = htonl(npt->dst_pfx.in6.s6_addr32[i]);
	}
	return 0;
}
EXPORT_SYMBOL(nf_conntrack_nptv6_ext_set_snpt);

/*
 * nf_conntrack_nptv6_ext_set_dnpt()
 *	Set the parameters for DNPT rule in the Extension.
 */
int nf_conntrack_nptv6_ext_set_dnpt(struct nf_conn *ct, struct nf_npt_info *npt)
{
	struct nf_ct_nptv6_ext *npte;
	int i;

	spin_lock_bh(&ct->lock);
	npte = nf_ct_nptv6_ext_find(ct);
	if (!npte) {
		spin_unlock_bh(&ct->lock);
		return -1;
	}
	spin_unlock_bh(&ct->lock);

	npte->src_pfx_len = npt->src_pfx_len;
	npte->dst_pfx_len = npt->dst_pfx_len;
	npte->nptv6_flags = NF_CT_NPTV6_EXT_DNPT;

	for(i = 0; i < 4; i++) {
		npte->src_pfx[i] = htonl(npt->src_pfx.in6.s6_addr32[i]);
		npte->dst_pfx[i] = htonl(npt->dst_pfx.in6.s6_addr32[i]);
	}

	return 0;
}
EXPORT_SYMBOL(nf_conntrack_nptv6_ext_set_dnpt);
