// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs_sta.h"
#include "hw.h"
#include "peer.h"
#include <linux/dma-mapping.h>
#include <linux/cacheflush.h>
#include "hif.h"
#include "ppe.h"
#include "dp.h"
#include "fse.h"
#include "wifi7/hal.h"
#include "ppe_public.h"

extern bool ath12k_fse_enable;
static atomic_t num_ppeds_nodes;

static struct ath12k_base *ds_node_map[PPE_DS_MAX_NODE];

struct nss_plugins_ops *nss_plugin_ops_ptr;

struct nss_plugins_ops *ath12k_get_registered_nss_plugin_ops(void)
{
	if (!nss_plugin_ops_ptr) {
		ath12k_err(NULL, "NSS plugin ops not registered with ath12k\n");
		return NULL;
	}
	return nss_plugin_ops_ptr;
}

extern struct sk_buff *
ath12k_dp_ppeds_tx_release_desc(struct ath12k_dp *dp,
				struct ath12k_ppeds_tx_desc_info *tx_desc);

irqreturn_t ath12k_ds_ppe2tcl_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_ppe2tcl_intr(ab->dp->ppe.ds_node_id);

	return IRQ_HANDLED;
}

irqreturn_t ath12k_ds_reo2ppe_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_reo2ppe_intr(ab->dp->ppe.ds_node_id);
	return IRQ_HANDLED;
}

void *ath12k_dp_get_ppe_ds_ctxt(struct ath12k_base *ab)
{
	if (!ab)
		return NULL;

	if (ab->dp->ppe.nss_plugin_ops)
		return ab->dp->ppe.nss_plugin_ops->ds_inst_get_ctx(ab->dp->ppe.ds_node_id);

	return NULL;
}

void ath12k_ppeds_set_tcl_prod_idx_v2(int ds_node_id, u16 tcl_prod_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring.ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_prod_cnt++;

	srng->u.src_ring.hp = tcl_prod_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_tcl_prod_idx_v2);

u16 ath12k_ppeds_get_tcl_cons_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 tp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_cons_cnt++;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring.ring_id];
	tp = *(volatile u32 *)(srng->u.src_ring.tp_addr);

	return tp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_tcl_cons_idx_v2);

void ath12k_ppeds_set_reo_cons_idx_v2(int ds_node_id,
				      u16 reo_cons_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring.ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_cons_cnt++;

	srng->u.src_ring.hp = reo_cons_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_reo_cons_idx_v2);

u16 ath12k_ppeds_get_reo_prod_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 hp;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring.ring_id];
	hp = *(volatile u32 *)(srng->u.dst_ring.hp_addr);
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_prod_cnt++;
	return hp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_reo_prod_idx_v2);

/* enable/disable PPE2TCL irq */
void ath12k_ppeds_enable_srng_intr_v2(int ds_node_id, bool enable)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (enable) {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.enable_intr_cnt++;

		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE2TCL);
	} else {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.disable_intr_cnt++;

		ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE2TCL);
	}
}
EXPORT_SYMBOL(ath12k_ppeds_enable_srng_intr_v2);

bool ath12k_ppeds_free_rx_desc_v2(struct ppe_ds_wlan_rxdesc_elem *arr,
				  struct ath12k_base *ab, int index,
				  u16 *idx_of_ab)
{
	struct ath12k_rx_desc_info *rx_desc;
	struct sk_buff *skb;

	rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[index]].cookie;

	if (rx_desc->device_id != ab->device_id)
		return false;

	skb = rx_desc->skb;
	rx_desc->skb = NULL;

	spin_lock_bh(&ab->dp->rx_desc_lock);
	list_add_tail(&rx_desc->list, &ab->dp->rx_desc_free_list);
	spin_unlock_bh(&ab->dp->rx_desc_lock);

	if (!skb) {
		ath12k_err(ab, "ppeds rx desc with no skb when freeing\n");
		return false;
	}

	/* When recycled_for_ds is set, packet is used by DS rings and never has
	 * touched by host. So, buffer unmap can be skipped.
	 */
	if (!skb->recycled_for_ds) {
		ath12k_core_dmac_inv_range_no_dsb(skb->data, skb->data + (skb->len +
						  skb_tailroom(skb)));
		ath12k_core_dma_unmap_single_attrs(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
						   skb->len + skb_tailroom(skb),
						   DMA_FROM_DEVICE,
						   DMA_ATTR_SKIP_CPU_SYNC);
	}

	skb->recycled_for_ds = 0;
	skb->fast_recycled = 0;
	dev_kfree_skb_any(skb);
	return true;
}
EXPORT_SYMBOL(ath12k_ppeds_free_rx_desc_v2);

int ath12k_dp_rx_bufs_replenish_ppeds(struct ath12k_base *ab, int req_entries,
				      u16 *idx_of_ab, struct ppe_ds_wlan_rxdesc_elem *arr)
{
	struct dp_rxdma_ring *rx_ring = &ab->dp->rx_refill_buf_ring;
	struct hal_srng *rxdma_srng;
	struct ath12k_buffer_addr *rxdma_desc;
	u32 cookie;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc;
	int count = 0, num_remain, i;
	enum hal_rx_buf_return_buf_manager mgr =  ab->hal.hal_params->rx_buf_rbm;

	rxdma_srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&rxdma_srng->lock);
	ath12k_hal_srng_access_begin(ab, rxdma_srng);

	num_remain = req_entries;
	for (i = 0 ; i < req_entries; i++) {
		if ((i + 1) < (req_entries - 1))
			prefetch((struct ath12k_rx_desc_info *)arr[idx_of_ab[i + 1]].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[i]].cookie;
		if (!rx_desc)
			break;

		if (!rx_desc->skb) {
			ath12k_err(ab, "ppeds rx desc with no skb when reusing!\n");
			break;
		}

		cookie = rx_desc->cookie;
		paddr = rx_desc->paddr;

		rxdma_desc = ath12k_hal_srng_src_get_next_entry(ab, rxdma_srng);
		if (!rxdma_desc)
			break;

		ath12k_hal_rx_buf_addr_info_set(rxdma_desc, paddr, cookie, mgr);
		num_remain--;
	}

	ath12k_hal_srng_access_end(ab, rxdma_srng);
	spin_unlock_bh(&rxdma_srng->lock);

	/* move any remaining descriptors to free list */
	for (; i < req_entries; i++)
		count += ath12k_ppeds_free_rx_desc_v2(arr, ab, i, idx_of_ab);

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.num_rx_desc_freed += count;

	return 0;
}

/* TODO: Fetch this directly from ppe_ds.h file. allocating dynamically will increase CPU */
#ifndef PPE_DS_TXCMPL_DEF_BUDGET
#define PPE_DS_TXCMPL_DEF_BUDGET 256
#endif

void ath12k_ppeds_release_rx_desc_v2(int ds_node_id,
				     struct ppe_ds_wlan_rxdesc_elem *arr, u16 count)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_base *src_ab = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	u32 rx_bufs_reaped[ATH12K_MAX_SOCS] = {0};
	struct ath12k_rx_desc_info *rx_desc;
	int device_id;
	u32 i = 0, new_size, num_free_desc;
	u16 *tmp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_rx_desc_cnt += count;

	if (unlikely(count > ab->dp->ppe.ppeds_rx_num_elem)) {
		new_size = sizeof(u16) * count;
		for (device_id = 0; device_id < ag->num_devices; device_id++) {
			tmp = krealloc(ab->dp->ppe.ppeds_rx_idx[device_id], new_size, GFP_ATOMIC);
			if (!tmp) {
				ath12k_err(ab, "ppeds: rx desc realloc failed for size %u\n",
					   count);
				goto err_h_alloc_failure;
			}

			ab->dp->ppe.ppeds_rx_idx[device_id] = tmp;
		}

		ab->dp->ppe.ppeds_rx_num_elem = count;
		ab->dp->ppe.ppeds_stats.num_rx_desc_realloc += device_id;
	}

	for (i = 0; i < count; i++) {
		if ((i + 1) < (count - 1))
			prefetch((struct ath12k_rx_desc_info *)arr[i + 1].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[i].cookie;
		if (!rx_desc) {
			ath12k_err(ab, "error: rx desc is null\n");
			continue;
		}

		device_id = rx_desc->device_id;
		/* Maintain indexes of arr per ab separately, which can accessed easily
		 * during per ab's rxdma srng replenish
		 */
		ab->dp->ppe.ppeds_rx_idx[device_id][rx_bufs_reaped[device_id]] = i;
		rx_bufs_reaped[device_id]++;
	}

	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		if (!rx_bufs_reaped[device_id])
			continue;

		src_ab = ag->ab[device_id];
		ath12k_dp_rx_bufs_replenish_ppeds(src_ab, rx_bufs_reaped[device_id],
						  &ab->dp->ppe.ppeds_rx_idx[device_id][0], arr);
	}

	return;

err_h_alloc_failure:
	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		src_ab = ag->ab[device_id];
		num_free_desc = 0;
		for (i = 0; i < count; i++)
			num_free_desc +=
				ath12k_ppeds_free_rx_desc_v2(arr, src_ab, i,
							     &src_ab->dp->ppe.ppeds_rx_idx[device_id][0]);
		if (!src_ab->stats_disable)
			src_ab->dp->ppe.ppeds_stats.num_rx_desc_freed += num_free_desc;
	}
}
EXPORT_SYMBOL(ath12k_ppeds_release_rx_desc_v2);

void ath12k_ppeds_release_tx_desc_single_v2(int ds_node_id,
					    u32 cookie)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_tx_single_cnt++;
}
EXPORT_SYMBOL(ath12k_ppeds_release_tx_desc_single_v2);

u32 ath12k_ppeds_get_batched_tx_desc_v2(int ds_node_id,
					struct ppe_ds_wlan_txdesc_elem *arr,
					u32 num_buff_req,
					u32 buff_size,
					u32 headroom)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	int i = 0;
	int allocated = 0;
	struct sk_buff *skb = NULL;
	int flags = GFP_ATOMIC;
	dma_addr_t paddr;
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *tmp;
	struct ath12k_ppeds_stats *ppeds_stats = &ab->dp->ppe.ppeds_stats;

#if LINUX_VERSION_IS_GEQ(4, 4, 0)
	flags = flags & ~__GFP_KSWAPD_RECLAIM;
#endif
	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	list_for_each_entry_safe(desc, tmp, &dp->ppe.ppeds_tx_desc_reuse_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;

		dp->ppe.ppeds_tx_desc_reuse_list_len--;

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	if (!num_buff_req) {
		spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
		goto update_stats_and_ret;
	}

	list_for_each_entry_safe(desc, tmp, &dp->ppe.ppeds_tx_desc_free_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;

		if (likely(!desc->skb)) {
		       /* In skb recycler, if recyler module allocates the buffers
			* already used by DS module to DS, then memzero, shinfo
			* reset can be avoided, since the DS packets were not
			* processed by SW
			*/
			skb = __netdev_alloc_skb_no_skb_reset(NULL, buff_size, flags);
			if (unlikely(!skb)) {
				desc->in_use = false;
				list_add_tail(&desc->list, &dp->ppe.ppeds_tx_desc_free_list);
				break;
			}

			skb_reserve(skb, headroom);
			if (!skb->recycled_for_ds) {
				ath12k_core_dmac_inv_range_no_dsb((void *)skb->data,
								  ((void *)skb->data +
								  buff_size - headroom));
				skb->recycled_for_ds = 1;
			}

			paddr = virt_to_phys(skb->data);

			desc->skb = skb;
			desc->paddr = paddr;
			desc->in_use = true;
		} else {
			pr_warn("skb found in ppeds_tx_desc_free_list");
		}

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	dsb(st);

update_stats_and_ret:
	if (unlikely(num_buff_req))
		ppeds_stats->tx_desc_alloc_fails += num_buff_req;

	if (unlikely(!ab->stats_disable)) {
		ppeds_stats->get_tx_desc_cnt++;
		ppeds_stats->tx_desc_allocated += allocated;
	}

	return allocated;
}
EXPORT_SYMBOL(ath12k_ppeds_get_batched_tx_desc_v2);

static int ath12k_dp_ppeds_tx_comp_poll(struct napi_struct *napi, int budget)
{
	struct ath12k_dp *dp = container_of(napi, struct ath12k_dp, ppe.ppeds_napi_ctxt.napi);
	struct ath12k_base *ab = dp->ab;
	int total_budget = (budget << 2) - 1;
	int work_done;

	set_bit(ATH12K_DP_PPEDS_TX_COMP_NAPI_BIT, &dp->service_rings_running);
	work_done = ath12k_ppeds_tx_completion_handler(ab, total_budget);
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tx_desc_freed += work_done;

	work_done = (work_done + 1) >> 2;
	clear_bit(ATH12K_DP_PPEDS_TX_COMP_NAPI_BIT,
		  &dp->service_rings_running);

	if (budget > work_done) {
		napi_complete(napi);
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
		if (ab->dp_umac_reset.umac_pre_reset_in_prog)
			ath12k_umac_reset_notify_pre_reset_done(ab);
	} else if (ab->dp_umac_reset.umac_pre_reset_in_prog) {
		 /* UMAC reset may fail in this case.
		  */
		WARN_ON_ONCE(1);
	}

	return (work_done > budget) ? budget : work_done;
}

/* PPE-DS release interrupt */
irqreturn_t ath12k_dp_ppeds_handle_tx_comp(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
	napi_schedule(&napi_ctxt->napi);
	return IRQ_HANDLED;
}

int ath12k_ppe_napi_budget = NAPI_POLL_WEIGHT;
static int ath12k_dp_ppeds_add_napi_ctxt(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	int ret;

	ret = init_dummy_netdev((struct net_device *)&napi_ctxt->ndev);
	if (ret) {
		ath12k_err(ab, "dummy netdev init fail\n");
		return -ENOSR;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	netif_napi_add(&napi_ctxt->ndev, &napi_ctxt->napi,
		       ath12k_dp_ppeds_tx_comp_poll);
#else
	netif_napi_add_weight(&napi_ctxt->ndev, &napi_ctxt->napi,
			      ath12k_dp_ppeds_tx_comp_poll, ath12k_ppe_napi_budget);
#endif

	return 0;
}

static void ath12k_dp_ppeds_del_napi_ctxt(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	netif_napi_del(&napi_ctxt->napi);
	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);
}

void ath12k_ppeds_notify_napi_done_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;

	clear_bit(ATH12K_DP_PPEDS_NAPI_DONE_BIT, &dp->service_rings_running);

	if (ab->dp_umac_reset.umac_pre_reset_in_prog)
		ath12k_umac_reset_notify_pre_reset_done(ab);
}
EXPORT_SYMBOL(ath12k_ppeds_notify_napi_done_v2);

static struct ppe_ds_wlan_ops_v2 ppeds_ops_v2 = {
	.get_tx_desc_many = ath12k_ppeds_get_batched_tx_desc_v2,
	.release_tx_desc_single = ath12k_ppeds_release_tx_desc_single_v2,
	.enable_tx_consume_intr = ath12k_ppeds_enable_srng_intr_v2,
	.set_tcl_prod_idx  = ath12k_ppeds_set_tcl_prod_idx_v2,
	.set_reo_cons_idx = ath12k_ppeds_set_reo_cons_idx_v2,
	.get_tcl_cons_idx = ath12k_ppeds_get_tcl_cons_idx_v2,
	.get_reo_prod_idx = ath12k_ppeds_get_reo_prod_idx_v2,
	.release_rx_desc = ath12k_ppeds_release_rx_desc_v2,
	.notify_napi_done = ath12k_ppeds_notify_napi_done_v2,
};

void ath12k_dp_peer_ppeds_route_setup(struct ath12k *ar, struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *primary_link_arvif;
	struct ath12k_vif *ahvif = arvif->ahvif;
	u32 service_code = PPE_DRV_SC_SPF_BYPASS;
	bool ppe_routing_enable = true;
	bool use_ppe = true;
	struct ath12k_sta *ahsta = arsta->ahsta;
	u32 priority_valid = 0, src_info = ahsta->ppe_vp_num;
	struct ieee80211_sta *sta;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR ||
	    (ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_MESH_11S))
		return;

	if (ahvif->dp_vif.ppe_vp_num == -1) {
		ath12k_dbg(ab, ATH12K_DBG_PPE, "Invalid ppe vp number\n");
		return;
	}

	/* If FSE is enabled, then let flow rule take decision of routing the
	 * packet to DS or host.
	 */
	if (ab->hw_params->support_fse && ath12k_fse_enable)
		use_ppe = false;

	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	/* When SLO STA is associated to AP link vif which does not have DS rings,
	 * do not enable DS.
	 */
	/* Check is for handling IPQ5322 radio, Can we add IPQ5322 target specific check here */
	if (!sta->mlo && !test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	/* If STA is MLO capable but primary link does not support DS,
	 * disable DS routing on RX.
	 */
	if (sta->mlo) {
		primary_link_arvif = arvif->ahvif->link[ahsta->assoc_link_id];

		if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED,
			      &primary_link_arvif->ar->ab->dev_flags)) {
			ath12k_dbg(ab, ATH12K_DBG_PPE,
				   "Primary link %d does not support DS "
				   "Disabling DS routing on RX for peer %pM\n",
				   ahsta->assoc_link_id, arsta->addr);
			return;
		}
	}

	ath12k_wmi_config_peer_ppeds_routing(ar, arsta->addr, arvif->vdev_id,
					     service_code, priority_valid,
					     src_info, ppe_routing_enable,
					     use_ppe);
}

#define HAL_TX_PPE_VP_CONFIG_TABLE_ADDR  0x00a44194
#define HAL_TX_PPE_VP_CONFIG_TABLE_OFFSET 4
void ath12k_hal_tx_set_ppe_vp_entry(struct ath12k_base *ab, u32 ppe_vp_config,
				    u32 ppe_vp_idx)
{
	ath12k_hif_write32(ab, HAL_TX_PPE_VP_CONFIG_TABLE_ADDR +
			   HAL_TX_PPE_VP_CONFIG_TABLE_OFFSET * ppe_vp_idx,
			   ppe_vp_config);
}

static int ath12k_dp_ppeds_alloc_ppe_vp_profile(struct ath12k_base *ab,
						struct ath12k_dp_ppe_vp_profile **vp_profile,
						int vp_num)
{
	int i;

	/* If a VP is already allocated with requested vp number, then return
	 * the same VP instead of creating a new profile
	 */
	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		if (ab->dp->ppe.ppe_vp_profile[i].is_configured &&
		    vp_num == ab->dp->ppe.ppe_vp_profile[i].vp_num) {
			ath12k_dbg(ab, ATH12K_DBG_PPE, "vp profile with num %d will be reused\n", vp_num);
			goto end;
		}
	}

	if (ab->dp->ppe.num_ppe_vp_profiles == PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_profile[i].is_configured)
			break;
	}

	if (i == PPE_VP_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp profile entries are in use!");
		return -ENOSR;
	}
	ab->dp->ppe.num_ppe_vp_profiles++;

	ab->dp->ppe.ppe_vp_profile[i].is_configured = true;

end:
	ab->dp->ppe.ppe_vp_profile[i].ref_count++;
	*vp_profile = &ab->dp->ppe.ppe_vp_profile[i];
	return i;
}

static void
ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(struct ath12k_base *ab,
						int ppe_vp_search_idx)
{
	if (ppe_vp_search_idx < 0 || ppe_vp_search_idx >= PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP search table free index");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE, "dealloc ppe_vp_search_idx %d\n",
		   ppe_vp_search_idx);

	if (!ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx]) {
		ath12k_err(ab, "PPE VP search idx table is not configured at idx:%d",
			   ppe_vp_search_idx);
		return;
	}

	ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx] = 0;
	ab->dp->ppe.num_ppe_vp_search_idx_entries--;
}

static void ath12k_dp_ppeds_dealloc_vp_tbl_entry(struct ath12k_base *ab,
						 int ppe_vp_num_idx)
{
	u32 vp_cfg = 0;

	if (ppe_vp_num_idx < 0 || ppe_vp_num_idx >= PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP free index");
		return;
	}

	/* Prevent register write if SOC is in recovery */
	if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
		ath12k_hal_tx_set_ppe_vp_entry(ab, vp_cfg, ppe_vp_num_idx);

	if (!ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx]) {
		ath12k_err(ab, "PPE VP is not configured at idx:%d", ppe_vp_num_idx);
		return;
	}

	ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx] = 0;
	ab->dp->ppe.num_ppe_vp_entries--;
}

static void
ath12k_dp_ppeds_dealloc_ppe_vp_profile(struct ath12k_base *ab,
				       int ppe_vp_profile_idx,
				       enum nl80211_iftype type)
{
	bool dealloced = false;
	struct ath12k_dp_ppe_vp_profile *vp_profile;

	if (ppe_vp_profile_idx < 0 || ppe_vp_profile_idx >= PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP profile free index");
		return;
	}

	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];

	if (!vp_profile->is_configured) {
		ath12k_err(ab, "PPE VP profile is not configured at idx:%d", ppe_vp_profile_idx);
		return;
	}

	vp_profile->ref_count--;

	if (!vp_profile->ref_count) {
		vp_profile->is_configured = false;
		ab->dp->ppe.num_ppe_vp_profiles--;
		dealloced = true;

		/* For STA mode ast index table reg also needs to be cleaned */
		if (type == NL80211_IFTYPE_STATION)
			ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(ab, vp_profile->search_idx_reg_num);

		ath12k_dp_ppeds_dealloc_vp_tbl_entry(ab, vp_profile->ppe_vp_num_idx);
	}

	if (dealloced)
		ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);
}

static int ath12k_dp_ppeds_alloc_vp_tbl_entry(struct ath12k_base *ab)
{
	int i;

	if (ab->dp->ppe.num_ppe_vp_profiles == PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_tbl_registered[i])
			break;
	}

	if (i == PPE_VP_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp table entries are in use!");
		return -ENOSR;
	}

	ab->dp->ppe.num_ppe_vp_entries++;
	ab->dp->ppe.ppe_vp_tbl_registered[i] = 1;

	return i;
}

static int ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(struct ath12k_base *ab)
{
	int i;

	if (ab->dp->ppe.num_ppe_vp_entries == PPE_VP_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_search_idx_tbl_set[i])
			break;
	}

	if (i == PPE_VP_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp table entries are in use!");
		return -ENOSR;
	}

	ab->dp->ppe.num_ppe_vp_search_idx_entries++;
	ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 1;

	return i;
}

static void ath12k_dp_ppeds_setup_vp_entry(struct ath12k_base *ab,
					   struct ath12k *ar,
					   struct ath12k_link_vif *arvif,
					   struct ath12k_dp_ppe_vp_profile *ppe_vp_profile)
{
	u32 ppe_vp_config = 0;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &arvif->ahvif->dp_vif.dp_link_vif[link_id];

	u8 lmac_id;
	u8 bank_id;

	/* In splitphy DS case, when mlo vaps created on different radios,
	 * the vp profile must be shared between these vaps with wildcard pmac_id,
	 * as they share the same vp number. We need to create a separate bank
	 * corresponding the shared vp. Also, this bank should have vdev id check
	 * disabled, so that the FW can get the ast from any of the lmacs without
	 * throwing vdev id mismatch error
	 */
	if (ppe_vp_profile->ref_count == 1) {
		lmac_id = ar->lmac_id;
		bank_id = dp_link_vif->bank_id;
	} else {
		lmac_id = HAL_TX_PPE_VP_CFG_WILDCARD_LMAC_ID;
		bank_id = arvif->splitphy_ds_bank_id;
	}

	ppe_vp_config |=
		u32_encode_bits(ppe_vp_profile->vp_num,
				HAL_TX_PPE_VP_CFG_VP_NUM) |
		u32_encode_bits(ppe_vp_profile->search_idx_reg_num,
				HAL_TX_PPE_VP_CFG_SRCH_IDX_REG_NUM) |
		u32_encode_bits(ppe_vp_profile->use_ppe_int_pri,
				HAL_TX_PPE_VP_CFG_USE_PPE_INT_PRI) |
		u32_encode_bits(ppe_vp_profile->to_fw,
				HAL_TX_PPE_VP_CFG_TO_FW) |
		u32_encode_bits(ppe_vp_profile->drop_prec_enable,
				HAL_TX_PPE_VP_CFG_DROP_PREC_EN) |
		u32_encode_bits(bank_id, HAL_TX_PPE_VP_CFG_BANK_ID) |
		u32_encode_bits(lmac_id, HAL_TX_PPE_VP_CFG_PMAC_ID) |
		u32_encode_bits(arvif->vdev_id, HAL_TX_PPE_VP_CFG_VDEV_ID);

	ath12k_hal_tx_set_ppe_vp_entry(ab, ppe_vp_config,
				       ppe_vp_profile->ppe_vp_num_idx);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
		    "ppeds_setup_vp_entry vp_num %d search_idx_reg_num %d" \
		    " use_ppe_int_pri %d to_fw %d drop_prec_enable %d bank_id %d" \
		    " lmac_id %d vdev_id %d ppe_vp_num_idx %d\n",
		    ppe_vp_profile->vp_num, ppe_vp_profile->search_idx_reg_num,
		    ppe_vp_profile->use_ppe_int_pri, ppe_vp_profile->to_fw,
		    ppe_vp_profile->drop_prec_enable, bank_id, lmac_id,
		    arvif->vdev_id, ppe_vp_profile->ppe_vp_num_idx);
}

void ath12k_dp_ppeds_update_vp_entry(struct ath12k *ar,
				     struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	int ppe_vp_profile_idx;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
	    arvif->ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	ppe_vp_profile_idx = arvif->ppe_vp_profile_idx;
	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE, "vp profile not present for arvif\n");
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	ath12k_dp_ppeds_setup_vp_entry(ab, arvif->ar, arvif, vp_profile);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

static int ath12k_ppeds_attach_link_apvlan_vif(struct ath12k_link_vif *arvif, int vp_num,
					       struct ath12k_vlan_iface *vlan_iface, int link_id)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_ppe_vp_profile *vp_profile = NULL;
	int ppe_vp_profile_idx, ppe_vp_tbl_idx = -1;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	int ppe_vp_search_tbl_idx = -1;
	int vdev_id = arvif->vdev_id;
	int ret;
	enum nl80211_iftype vif_type;

	if (!wdev)
		return -EOPNOTSUPP;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return 0;

	/*Allocate a ppe vp profile for a vap */
	spin_lock(&ppe->ppe_vp_tbl_lock);
	ppe_vp_profile_idx = ath12k_dp_ppeds_alloc_ppe_vp_profile(ab, &vp_profile, vp_num);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "flows for %s link %d will use SFE RFS flow distribution",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}

	if (vp_profile->ref_count == 1) {
		ppe_vp_tbl_idx = ath12k_dp_ppeds_alloc_vp_tbl_entry(ab);
		if (ppe_vp_tbl_idx < 0) {
			ath12k_err(ab, "Failed to allocate PPE VP idx for vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}

		if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			ppe_vp_search_tbl_idx = ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(ab);
			if (ppe_vp_search_tbl_idx < 0) {
				ath12k_err(ab,"Failed to allocate PPE VP search table idx for vdev_id:%d",
					   vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
		}

		vp_profile->vp_num = vp_num;
		vp_profile->ppe_vp_num_idx = ppe_vp_tbl_idx;
		vp_profile->to_fw = 0;
		vp_profile->use_ppe_int_pri = 0;
		vp_profile->drop_prec_enable = 0;
		vp_profile->arvif = arvif;

		vlan_iface->ppe_vp_profile_idx[link_id] = ppe_vp_profile_idx;
	} else {
		bool vdev_id_check_en = false;

		vlan_iface->ppe_vp_profile_idx[link_id] = ppe_vp_profile_idx;

		arvif->splitphy_ds_bank_id =
			ath12k_dp_tx_get_bank_profile(ab, arvif, ab->dp, vdev_id_check_en);

		ath12k_ppeds_update_splitphy_bank_id(ab, arvif);
	}

	ath12k_dp_ppeds_setup_vp_entry(ab, ar, arvif, vp_profile);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
		   "PPEDS vdev attach success soc_idx %d ds_node_id %d vdev_id %d vpnum %d ppe_vp_profile_idx %d "
		   "ppe_vp_tbl_idx %d to_fw %d int_pri %d prec_en %d search_idx_reg_num %d\n",
		   ab->dp->ppe.ppeds_soc_idx, ab->dp->ppe.ds_node_id, vdev_id,
		   vp_num, ppe_vp_profile_idx, ppe_vp_tbl_idx, vp_profile->to_fw,
		   vp_profile->use_ppe_int_pri, vp_profile->drop_prec_enable,
		   vp_profile->search_idx_reg_num);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return 0;

dealloc_vp_profile:
	vif_type = arvif->ahvif->vif->type;
	ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab, ppe_vp_profile_idx, vif_type);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return ret;
}

void ath12k_ppe_ds_attach_vlan_vif_link(struct ath12k_vlan_iface *vlan_iface,
					int ppe_vp_num)
{
	struct ieee80211_vif *vif = vlan_iface->parent_vif;
	struct ath12k_vif *ap_ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *ap_arvif;
	struct ath12k *ar;
	int link_id, ret;
	unsigned long links_map;

	if (vlan_iface->attach_link_done)
		return;

	links_map = ap_ahvif->links_map;

	rcu_read_lock();
	for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		ap_arvif = rcu_dereference(ap_ahvif->link[link_id]);

		if (!ap_arvif || !ap_arvif->is_created)
			continue;

		ar = ap_arvif->ar;
		ret = ath12k_ppeds_attach_link_apvlan_vif(ap_arvif, ppe_vp_num, vlan_iface,
							  link_id);
		if (ret)
			ath12k_info(ar->ab, "Unable to attach ppe ds node for arvif %d\n",
				    ret);
	}
	rcu_read_unlock();
	vlan_iface->attach_link_done = true;
}

void ath12k_ppeds_update_splitphy_bank_id(struct ath12k_base *ab,
					  struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_link_vif *iter_arvif;
	unsigned long links_map;
	int link_idx;

	links_map = ahvif->links_map;
	for_each_set_bit(link_idx, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		iter_arvif = ahvif->link[link_idx];
		int splitphy_ds_bank_id = DP_INVALID_BANK_ID;

		if (!iter_arvif || iter_arvif == arvif ||
		    !iter_arvif->is_created || ab != iter_arvif->ar->ab)
			continue;

		splitphy_ds_bank_id = iter_arvif->splitphy_ds_bank_id;
		/**
		 * If the link already has a splitphy_ds_bank_id
		 * then decrement the refcount for that bank_id
		 * before updating the link with a new bank_id
		 */
		if (splitphy_ds_bank_id != DP_INVALID_BANK_ID)
			ath12k_dp_tx_put_bank_profile(ath12k_ab_to_dp(ab),
						      splitphy_ds_bank_id);

		iter_arvif->splitphy_ds_bank_id = arvif->splitphy_ds_bank_id;
		ath12k_dp_increment_bank_num_users(ath12k_ab_to_dp(ab),
						   arvif->splitphy_ds_bank_id);
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "splitphy_ds_bank_id %d for vp_num %d vdev_id %d\n",
			   arvif->splitphy_ds_bank_id,
			   ahvif->dp_vif.ppe_vp_num, iter_arvif->vdev_id);
	}
}

int ath12k_ppeds_attach_link_vif(struct ath12k_link_vif *arvif, int vp_num,
				 int *link_ppe_vp_profile_idx,
				 struct ieee80211_vif *vif)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_ppe_vp_profile *vp_profile = NULL;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	int ppe_vp_profile_idx, ppe_vp_tbl_idx = -1;
	int ppe_vp_search_tbl_idx = -1;
	int vdev_id = arvif->vdev_id;
	int ret;
	enum nl80211_iftype vif_type;

	if (!wdev)
		return -EOPNOTSUPP;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return 0;

	if (ahvif->vif->type != NL80211_IFTYPE_AP && ahvif->vif->type != NL80211_IFTYPE_STATION) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "DS is not supported for vap type %d\n", ahvif->vif->type);
		return 0;
	}

	/*Allocate a ppe vp profile for a vap */
	spin_lock(&ppe->ppe_vp_tbl_lock);
	ppe_vp_profile_idx = ath12k_dp_ppeds_alloc_ppe_vp_profile(ab, &vp_profile, vp_num);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "flows for %s link %d will use SFE RFS flow distribution",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}

	if (vp_profile->ref_count == 1) {
		ppe_vp_tbl_idx = ath12k_dp_ppeds_alloc_vp_tbl_entry(ab);
		if (ppe_vp_tbl_idx < 0) {
			ath12k_err(ab, "Failed to allocate PPE VP idx for vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}

		if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			ppe_vp_search_tbl_idx = ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(ab);
			if (ppe_vp_search_tbl_idx < 0) {
				ath12k_err(ab,
					   "Failed to allocate PPE VP search table idx for vdev_id:%d", vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
		}

		vp_profile->vp_num = vp_num;
		vp_profile->ppe_vp_num_idx = ppe_vp_tbl_idx;
		vp_profile->to_fw = 0;
		vp_profile->use_ppe_int_pri = 0;
		vp_profile->drop_prec_enable = 0;
		vp_profile->arvif = arvif;

		*link_ppe_vp_profile_idx = ppe_vp_profile_idx;
	} else {
		bool vdev_id_check_en = false;
		u8 link_id;

		if (arvif->ahvif->links_map &&
		    arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			struct ath12k_link_vif *arvif;
			struct ath12k_base *prim_ab;
			struct ath12k_dp_ppe_vp_profile *prim_vp_profile;

			rcu_read_lock();
			sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
			if (!sta) {
				ath12k_warn(ab, "failed to find station entry for %pM",
					    vif->cfg.ap_addr);
				spin_unlock(&ppe->ppe_vp_tbl_lock);
				rcu_read_unlock();
				return -ENOENT;
			}

			ahsta = ath12k_sta_to_ahsta(sta);
			link_id = ahsta->deflink.link_id;
			if (!sta->mlo)
				link_id = ahsta->deflink.link_id;
			else
				link_id = ahsta->assoc_link_id;
			arvif = rcu_dereference(ahvif->link[link_id]);

			if (!arvif) {
				ath12k_warn(ab, "arvif not found for station:%pM",
					    sta->addr);
				spin_unlock(&ppe->ppe_vp_tbl_lock);
				rcu_read_unlock();
				return -EINVAL;
			}

			prim_ab = arvif->ar->ab;
			prim_vp_profile = &prim_ab->dp->ppe.ppe_vp_profile[arvif->ppe_vp_profile_idx];

			vp_profile->search_idx_reg_num =
					prim_vp_profile->search_idx_reg_num;
			rcu_read_unlock();
		}

		*link_ppe_vp_profile_idx = ppe_vp_profile_idx;
		arvif->splitphy_ds_bank_id = ath12k_dp_tx_get_bank_profile(ab, arvif, ab->dp, vdev_id_check_en);

		ath12k_ppeds_update_splitphy_bank_id(ab, arvif);

	}

	ath12k_dp_ppeds_setup_vp_entry(ab, ar, arvif, vp_profile);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
		   "PPEDS vdev attach success soc_idx %d ds_node_id %d vdev_id %d vpnum %d ppe_vp_profile_idx %d "
		   "ppe_vp_tbl_idx %d to_fw %d int_pri %d prec_en %d search_idx_reg_num %d\n",
		   ab->dp->ppe.ppeds_soc_idx, ab->dp->ppe.ds_node_id, vdev_id,
		   vp_num, ppe_vp_profile_idx, ppe_vp_tbl_idx, vp_profile->to_fw,
		   vp_profile->use_ppe_int_pri, vp_profile->drop_prec_enable,
		   vp_profile->search_idx_reg_num);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return 0;

dealloc_vp_profile:
	vif_type = arvif->ahvif->vif->type;
	ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab, ppe_vp_profile_idx, vif_type);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return ret;
}

void ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map)
{
	u32 ppeds_idx_map_val = 0;
	int ppe_vp_profile_idx = arvif->ppe_vp_profile_idx;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &arvif->ahvif->dp_vif.dp_link_vif[link_id];

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
	    !arvif->primary_sta_link)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];
	if (!vp_profile->is_configured) {
		ath12k_err(ab, "Invalid PPE VP profile for vdev_id:%d",
			   arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}
	if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
		if (peer_map) {
			ppeds_idx_map_val |=
				u32_encode_bits(dp_link_vif->ast_idx, HAL_TX_PPEDS_CFG_SEARCH_IDX) |
				u32_encode_bits(dp_link_vif->ast_hash, HAL_TX_PPEDS_CFG_CACHE_SET);
		}
		ath12k_hal_ppeds_cfg_ast_override_map_reg(ab, vp_profile->search_idx_reg_num,
							  ppeds_idx_map_val);
	}
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

void ath12k_ppeds_detach_link_apvlan_vif(struct ath12k_link_vif *arvif,
					 struct ath12k_vlan_iface *vlan_iface,
					 int link_id)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	int ppe_vp_profile_idx = vlan_iface->ppe_vp_profile_idx[link_id];
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	enum nl80211_iftype vif_type;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ahvif->dp_vif.ppe_vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];
	if (!vp_profile->is_configured) {
		ath12k_err(ab, "Invalid PPE VP profile for vdev_id:%d",
			   arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	vif_type = arvif->ahvif->vif->type;
	ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab, ppe_vp_profile_idx, vif_type);
	vlan_iface->ppe_vp_profile_idx[link_id] = ATH12K_INVALID_VP_PROFILE_IDX;
	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS vdev detach success vpnum %d  ppe_vp_profile_idx %d\n",
	       vp_profile->vp_num, ppe_vp_profile_idx);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

void ath12k_ppeds_detach_link_vif(struct ath12k_link_vif *arvif, int ppe_vp_profile_idx)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	enum nl80211_iftype vif_type;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ahvif->dp_vif.ppe_vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];
	if (!vp_profile->is_configured) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "No PPE VP profile found for vdev_id:%d", arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	vif_type = arvif->ahvif->vif->type;
	ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab, ppe_vp_profile_idx, vif_type);
	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS vdev detach success vpnum %d  ppe_vp_profile_idx %d\n",
		   vp_profile->vp_num, ppe_vp_profile_idx);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

int ath12k_ppeds_attach(struct ath12k_base *ab)
{
	int i, ret, ds_node_id;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	ath12k_dp_ppeds_tx_cmem_init(ab, ab->dp);
	ret = ath12k_dp_cc_ppeds_desc_init(ab);
	if (ret) {
		ath12k_err(ab, "Failed to allocate ppe-ds descriptors\n");
		return -ENOMEM;
	}

	spin_lock_init(&ab->dp->ppe.ppe_vp_tbl_lock);

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		memset(&ab->dp->ppe.ppe_vp_profile[i], 0, sizeof(struct ath12k_dp_ppe_vp_profile));
	}

	ab->dp->ppe.num_ppe_vp_profiles = 0;
	ab->dp->ppe.num_ppe_vp_entries = 0;

	ds_node_id = ab->dp->ppe.nss_plugin_ops->ds_inst_alloc(&ppeds_ops_v2,
							       sizeof(struct ath12k_base *));

	if (ds_node_id < 0 || ds_node_id == PPE_VP_DS_INVALID_NODE_ID) {
		ath12k_err(ab, "Failed to get DS node id for device_id %d\n",
			   ab->device_id);
		return -ENOSR;
	}
	ab->dp->ppe.ds_node_id = ds_node_id;
	ds_node_map[ds_node_id] = ab;

	WARN_ON(ab->dp->ppe.ppeds_soc_idx != -1);
	/* dec ppeds_soc_idx to start from 0 */
	ab->dp->ppe.ppeds_soc_idx = atomic_inc_return(&num_ppeds_nodes) - 1;

	ath12k_info(ab,
		   "PPEDS attach ab %px ppeds_soc_idx %d num_ppeds_nodes %d\n",
		   ab, ab->dp->ppe.ppeds_soc_idx,
		   atomic_read(&num_ppeds_nodes));

	ret = ath12k_dp_ppeds_add_napi_ctxt(ab);
	if (ret)
		return -ENOSR;

	ath12k_info(ab, "PPEDS attach success\n");

	for (i = 0; i < ab->ag->num_devices; i++) {
		ab->dp->ppe.ppeds_rx_idx[i] = kzalloc((sizeof(u16) * PPE_DS_TXCMPL_DEF_BUDGET),
						      GFP_ATOMIC);
		if (!ab->dp->ppe.ppeds_rx_idx[i]) {
			ath12k_err(ab, "Failed to alloc mem ppeds_rx_desc\n");
			goto err_ppeds_attach;
		}
	}

	ab->dp->ppe.ppeds_rx_num_elem = PPE_DS_TXCMPL_DEF_BUDGET;

	return 0;

err_ppeds_attach:
	ab->dp->ppe.ppeds_rx_num_elem = 0;
	for (i = i - 1; i >= 0; i--) {
		kfree(ab->dp->ppe.ppeds_rx_idx[i]);
		ab->dp->ppe.ppeds_rx_idx[i] = NULL;
	}

	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_ppeds_attach);

int ath12k_ppeds_detach(struct ath12k_base *ab)
{
	int i;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	ath12k_dp_ppeds_del_napi_ctxt(ab);
	ath12k_dp_cc_ppeds_desc_cleanup(ab);

	/* free ppe-ds interrupts before freeing the instance */
	ath12k_hif_ppeds_free_interrupts(ab);

	ab->dp->ppe.nss_plugin_ops->ds_inst_free(ab->dp->ppe.ds_node_id);

	ab->dp->ppe.ppeds_soc_idx = -1;
	atomic_dec(&num_ppeds_nodes);

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		memset(&ab->dp->ppe.ppe_vp_profile[i], 0, sizeof(struct ath12k_dp_ppe_vp_profile));
	}

	ab->dp->ppe.num_ppe_vp_profiles = 0;
	ab->dp->ppe.num_ppe_vp_entries = 0;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS detach success\n");

	if (ab->dp->ppe.ppeds_rx_num_elem) {
		ab->dp->ppe.ppeds_rx_num_elem = 0;
		for (i = 0; i < ab->ag->num_devices; i++) {
			kfree(ab->dp->ppe.ppeds_rx_idx[i]);
			ab->dp->ppe.ppeds_rx_idx[i] = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_ppeds_detach);

int ath12k_dp_ppeds_start(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;
	bool umac_reset_inprogress;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	umac_reset_inprogress = ath12k_dp_umac_reset_in_progress(ab);

	if (!umac_reset_inprogress)
		napi_enable(&napi_ctxt->napi);

	ab->dp->ppe.ppeds_stopped = 0;
	wlan_info_hdl.umac_reset_inprogress = 0;

	if (ab->dp->ppe.nss_plugin_ops->ds_inst_start(&wlan_info_hdl,
						      ab->dp->ppe.ds_node_id) != 0)
		return -EINVAL;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS start success device_id %d ds_node_id %d ppeds_soc_idx %d",
		   ab->device_id, ab->dp->ppe.ds_node_id, ab->dp->ppe.ppeds_soc_idx);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_ppeds_start);

void ath12k_dp_ppeds_stop(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;
	bool umac_reset_in_progress;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return;
	}

	umac_reset_in_progress = ath12k_dp_umac_reset_in_progress(ab);

	if (ab->dp->ppe.ppeds_stopped) {
		ath12k_warn(ab, "PPE DS aleady stopped!\n");
		return;
	}

	ab->dp->ppe.ppeds_stopped = 1;

	if (!umac_reset_in_progress)
		napi_disable(&napi_ctxt->napi);

	wlan_info_hdl.umac_reset_inprogress = umac_reset_in_progress;

	ab->dp->ppe.nss_plugin_ops->ds_inst_stop(&wlan_info_hdl,
						 ab->dp->ppe.ds_node_id);

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS stop success\n");
}
EXPORT_SYMBOL(ath12k_dp_ppeds_stop);

int ath12k_dp_ppeds_register_soc(struct ath12k_dp *dp, struct dp_ppe_ds_idxs *idx)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *ppe2tcl_ring, *reo2ppe_ring;
	struct ppe_ds_wlan_reg_info reg_info = {0};

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	ppe2tcl_ring = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring.ring_id];
	reo2ppe_ring =  &ab->hal.srng_list[dp->ppe.reo2ppe_ring.ring_id];

	reg_info.ppe2tcl_ba = dp->ppe.ppe2tcl_ring.paddr;
	reg_info.reo2ppe_ba = dp->ppe.reo2ppe_ring.paddr;
	reg_info.ppe2tcl_num_desc = DP_PPE2TCL_RING_SIZE;
	reg_info.reo2ppe_num_desc = DP_REO2PPE_RING_SIZE;

	if (ab->dp->ppe.nss_plugin_ops->ds_inst_register(&reg_info,
							 ab->dp->ppe.ds_node_id) != true) {
		ath12k_err(ab, "ppeds not attached");
		return -EINVAL;
	}

	idx->ppe2tcl_start_idx = reg_info.ppe2tcl_start_idx;
	idx->reo2ppe_start_idx = reg_info.reo2ppe_start_idx;
	ab->dp->ppe.ppeds_int_mode_enabled = reg_info.ppe_ds_int_mode_enabled;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS register soc-success device_id %d ppe2tcl_start_idx 0x%x reo2ppe_start_idx 0x%x",
		   ab->device_id, idx->ppe2tcl_start_idx, idx->reo2ppe_start_idx);

	return 0;
}

#define HAL_TCL_RBM_MAPPING0_ADDR_OFFSET        0x00000088
#define HAL_TCL_RBM_MAPPING_SHFT 4
#define HAL_TCL_RBM_MAPPING_BMSK 0xF
#define HAL_TCL_RBM_MAPPING_PPE2TCL_OFFSET  7
#define HAL_TCL_RBM_MAPPING_TCL_CMD_CREDIT_OFFSET  6

void ath12k_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
				      u8 rbm_id, int ring_type)
{
	u32 curr_map, new_map;

	if (ring_type == HAL_PPE2TCL)
		ring_num = ring_num + HAL_TCL_RBM_MAPPING_PPE2TCL_OFFSET;
	else if (ring_type == HAL_TCL_CMD)
		ring_num = ring_num + HAL_TCL_RBM_MAPPING_TCL_CMD_CREDIT_OFFSET;

	curr_map = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				     HAL_TCL_RBM_MAPPING0_ADDR_OFFSET);

	/* Protect the other values and clear the specific fields to be updated */
	curr_map &= (~(HAL_TCL_RBM_MAPPING_BMSK <<
		     (HAL_TCL_RBM_MAPPING_SHFT * ring_num)));
	new_map = curr_map | ((HAL_TCL_RBM_MAPPING_BMSK & rbm_id) <<
			      (HAL_TCL_RBM_MAPPING_SHFT * ring_num));

	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL_RBM_MAPPING0_ADDR_OFFSET, new_map);
}
EXPORT_SYMBOL(ath12k_hal_tx_config_rbm_mapping);

static int ath12k_dp_srng_init_idx(struct ath12k_base *ab, struct dp_srng *ring,
				   enum hal_ring_type type, int ring_num,
				   int mac_id,
				   int num_entries, u32 restore_idx)
{
	struct hal_srng_params params = { 0 };
	bool cached = false;
	int ret;
	int vector = 0;

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;
	ath12k_dp_srng_msi_setup(ab, &params, type, ring_num + mac_id);

	if (ab->hw_params->ds_support && ab->hif.bus == ATH12K_BUS_AHB &&
	    !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);

	switch (type) {
	case HAL_REO_DST:
	case HAL_REO2PPE:
			params.intr_batch_cntr_thres_entries =
				HAL_SRNG_INT_BATCH_THRESHOLD_RX;
			params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
			break;
	case HAL_WBM2SW_RELEASE:
		if (ab->hw_params->hw_ops->dp_srng_is_tx_comp_ring(ring_num)) {
			params.intr_batch_cntr_thres_entries =
				HAL_SRNG_INT_BATCH_THRESHOLD_TX;
			params.intr_timer_thres_us =
				HAL_SRNG_INT_TIMER_THRESHOLD_TX;
			break;
		}
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	case HAL_PPE2TCL:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL;
		break;
	default:
		ath12k_warn(ab, "Not a valid ring type in dp :%d\n", type);
		return -EINVAL;
	}
	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params,
					restore_idx);
	if (ret < 0) {
		ath12k_warn(ab, "failed to setup srng: %d ring_id %d\n",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}

static int ath12k_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
				enum hal_ring_type type, int ring_num,
				int num_entries)
{
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;
#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
		case HAL_WBM2SW_RELEASE:
			cached = true;
			break;
		default:
			cached = false;
		}
	}
#else
	cached = true;
#endif
	if (cached) {
		ring->vaddr_unaligned = kzalloc(ring->size, GFP_KERNEL);
		ring->paddr_unaligned = virt_to_phys(ring->vaddr_unaligned);
	} else {
		ring->vaddr_unaligned = dma_alloc_coherent(ab->dev, ring->size,
							   &ring->paddr_unaligned,
							   GFP_KERNEL);
	}
	if (!ring->vaddr_unaligned)
		return -ENOMEM;

	memset(ring->vaddr_unaligned, 0, ring->size);
	ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
	ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
			(unsigned long)ring->vaddr_unaligned);

	return 0;
}

int ath12k_ppeds_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			       enum hal_ring_type type, int ring_num,
			       int num_entries)
{
	int ret;

	ret = ath12k_dp_srng_alloc(ab, ring, type, ring_num, num_entries);
	if (ret != 0)
		ath12k_warn(ab, "Failed to allocate dp srng ring.\n");

	return 0;
}

static int ath12k_ppeds_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
				     enum hal_ring_type type, int ring_num,
				     int mac_id, int num_entries,
				     u32 restore_idx)
{
	int ret;

	ret = ath12k_dp_srng_init_idx(ab, ring, type, ring_num, mac_id,
				      num_entries, restore_idx);
	if (ret != 0)
		ath12k_warn(ab, "Failed to initialize dp srng ring.\n");

	return 0;
}

int ath12k_dp_srng_ppeds_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct dp_ppe_ds_idxs restore_idx = {0};
	int ret, size;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_ppeds_dp_srng_ring_alloc;

	/* TODO: retain and use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_alloc(ab, &dp->ppe.reo2ppe_ring, HAL_REO2PPE,
					 0, DP_REO2PPE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo2ppe ring :%d\n", ret);
		goto err;
	}

	/* TODO: retain and use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_alloc(ab, &dp->ppe.ppe2tcl_ring, HAL_PPE2TCL,
					 0, DP_PPE2TCL_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
		goto err;
	}

	size = sizeof(struct hal_wbm_release_ring_tx) * DP_TX_COMP_RING_SIZE;
	dp->ppe.ppeds_comp_ring.tx_status_head = 0;
	dp->ppe.ppeds_comp_ring.tx_status_tail = DP_TX_COMP_RING_SIZE - 1;
	dp->ppe.ppeds_comp_ring.tx_status = kmalloc(size, GFP_KERNEL);

skip_ppeds_dp_srng_ring_alloc:
	if (!dp->ppe.ppeds_comp_ring.tx_status) {
		ath12k_err(ab, "PPE tx status completion buffer alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = ath12k_dp_ppeds_register_soc(dp, &restore_idx);
	if (ret) {
		ath12k_err(ab, "ppeds registration failed\n");
		goto err;
	}

	/* TODO: retain and use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_init(ab, &dp->ppe.reo2ppe_ring, HAL_REO2PPE,
					0, 0, DP_REO2PPE_RING_SIZE,
					restore_idx.reo2ppe_start_idx);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo2ppe ring :%d\n", ret);
		goto err;
	}

	ath12k_hal_reo_config_reo2ppe_dest_info(ab);

	/* TODO: retain and use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_init(ab, &dp->ppe.ppe2tcl_ring, HAL_PPE2TCL,
					0, 0, DP_PPE2TCL_RING_SIZE,
					restore_idx.ppe2tcl_start_idx);
	if (ret) {
		ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
		goto err;
	}

	/* TODO: retain and use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_dp_srng_setup(ab, &dp->ppe.ppeds_comp_ring.ppe_wbm2sw_ring,
				   HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM, 0,
				   DP_PPE_WBM2SW_RING_SIZE);
	if (ret) {
		ath12k_err(ab,
			    "failed to set up wbm2sw ppeds tx completion ring :%d\n",
			    ret);
		goto err;
	}
	ath12k_hal_tx_config_rbm_mapping(ab, 0,
					 HAL_WBM2SW_PPEDS_TX_CMPLN_MAP_ID,
					 HAL_PPE2TCL);

err:
	/* caller takes care of calling ath12k_dp_srng_ppeds_cleanup */
	return ret;
}

void ath12k_dp_srng_ppeds_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	ath12k_dp_srng_cleanup(ab, &dp->ppe.ppe2tcl_ring);
	ath12k_dp_srng_cleanup(ab, &dp->ppe.reo2ppe_ring);
	kfree(dp->ppe.ppeds_comp_ring.tx_status);
	dp->ppe.ppeds_comp_ring.tx_status = NULL;
	ath12k_dp_srng_cleanup(ab, &dp->ppe.ppeds_comp_ring.ppe_wbm2sw_ring);
}

int ath12k_ppe_rfs_get_core_mask(void)
{
	return ATH12K_PPE_DEFAULT_CORE_MASK;
}

/* User is expected to flush ecm entries before changing core mask */
int ath12k_change_core_mask_for_ppe_rfs(struct ath12k_base *ab,
					struct ath12k_vif *ahvif,
					int core_mask)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ret;

	if (!wdev)
		return -ENODEV;

	if (ahvif->dp_vif.ppe_vp_num <= 0 ||
	    ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_PASSIVE) {
		ath12k_warn(ab, "invalid vp for dev %s\n", wdev->netdev->name);
		return -EINVAL;
	}

	if (core_mask < 0) {
		ath12k_warn(ab, "Invalid core_mask for PPE RFS\n");
		return -EINVAL;
	}

	if (core_mask == ahvif->dp_vif.ppe_core_mask)
		return 0;

	ret = ath12k_vif_get_vp_num(ahvif, wdev->netdev);
	if (ret) {
		ath12k_warn(ab, "error in enabling ppe vp for netdev %s\n",
			    wdev->netdev->name);
		return ret;
	}

	return 0;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static bool ath12k_stats_update_ppe_vp(struct net_device *dev, ppe_vp_hw_stats_t *vp_stats)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(netdev_tstats(dev));

	if (dev->reg_state != NETREG_REGISTERED)
		return false;

	u64_stats_update_begin(&tstats->syncp);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	tstats->tx_packets += vp_stats->tx_pkt_cnt;
	tstats->tx_bytes += vp_stats->tx_byte_cnt;
	tstats->rx_packets += vp_stats->rx_pkt_cnt;
	tstats->rx_bytes += vp_stats->rx_byte_cnt;
#else
	u64_stats_add(&tstats->tx_packets, vp_stats->tx_pkt_cnt);
	u64_stats_add(&tstats->tx_bytes, vp_stats->tx_byte_cnt);
	u64_stats_add(&tstats->rx_packets, vp_stats->rx_pkt_cnt);
	u64_stats_add(&tstats->rx_bytes, vp_stats->rx_byte_cnt);
#endif
	u64_stats_update_end(&tstats->syncp);

	return true;
}
#endif

void ath12k_vif_free_vp(struct ath12k_vif *ahvif, struct net_device *dev)
{
	/* ahvif->dp_vif.ppe_vp_num gets reset to 0 during memset in mac80211 for vif->drv_priv */
	if (ahvif->dp_vif.ppe_vp_num == ATH12K_INVALID_PPE_VP_NUM || ahvif->dp_vif.ppe_vp_num == 0)
		return;

	ppe_vp_free(ahvif->dp_vif.ppe_vp_num);

	ath12k_dbg(NULL, ATH12K_DBG_PPE, "Destroyed PPE VP port no:%d for dev:%s vdev type %d\n",
	       ahvif->dp_vif.ppe_vp_num, dev->name,
	       ahvif->vdev_type);
	ahvif->dp_vif.ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
}
EXPORT_SYMBOL(ath12k_vif_free_vp);

int ath12k_vif_update_vp_config(struct ath12k_vif *ahvif, int ppe_vp_type)

{
	struct ppe_vp_ui vpui;
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ret;
	uint8_t update_flags = 0;
	struct nss_plugins_ops *plugin_ops = ath12k_get_registered_nss_plugin_ops();

	if ((ahvif->dp_vif.ppe_vp_num == ATH12K_INVALID_PPE_VP_NUM) || !wdev)
		return -EINVAL;

	if (!plugin_ops)
		return -EINVAL;

	memset(&vpui, 0, sizeof(struct ppe_vp_ui));
	vpui.usr_type = ppe_vp_type;
	vpui.core_mask = ahvif->dp_vif.ppe_core_mask;
	update_flags |= PPE_VP_UPDATE_FLAG_VP_CORE_MASK;
	update_flags |= PPE_VP_UPDATE_FLAG_VP_USR_TYPE;
	vpui.update_flags = update_flags;
	vpui.stats_cb = ath12k_stats_update_ppe_vp;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Direct Switching */
	switch (ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
		vpui.core_mask = ath12k_ppe_rfs_get_core_mask();
		break;
	case PPE_VP_USER_TYPE_DS:
	case PPE_VP_USER_TYPE_ACTIVE:
		vpui.core_mask = ATH12K_PPE_DEFAULT_CORE_MASK;
		break;
	}
#endif

	ret = plugin_ops->vp_cfg_update(wdev->netdev, &vpui);

	if (ret) {
		ath12k_err(NULL, "failed to update ppe vp config type %d err %d\n",
			   ppe_vp_type, ret);
		goto exit;
	}

	ahvif->dp_vif.ppe_vp_type = ppe_vp_type;

	ath12k_dbg(NULL, ATH12K_DBG_PPE,
		   "Updated PPE VP port no %d for dev %s type %d\n",
		   ahvif->dp_vif.ppe_vp_num, wdev->netdev->name, ahvif->dp_vif.ppe_vp_type);
exit:
	return ret;
}

int ath12k_vif_set_mtu(struct ath12k_vif *ahvif, int mtu)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ppe_vp_num = ahvif->dp_vif.ppe_vp_num;

	if (!wdev)
		return -ENODEV;

	if (ppe_vp_mtu_set(ppe_vp_num, mtu) != PPE_VP_STATUS_SUCCESS) {
		pr_err("\ndev:%px, dev->name:%s mtu %d vp num = %d set failed ",
			wdev->netdev, wdev->netdev->name, mtu, ppe_vp_num);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_vif_set_mtu);

int ath12k_vif_get_vp_num(struct ath12k_vif *ahvif, struct net_device *dev)
{
	int ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
	struct nss_plugins_ops *plugin_ops = ath12k_get_registered_nss_plugin_ops();

	if (dev->ieee80211_ptr &&
	    dev->ieee80211_ptr->iftype == NL80211_IFTYPE_MONITOR)
		return 0;

	if (!plugin_ops)
		return -EINVAL;

	ppe_vp_num = plugin_ops->get_vp_num(dev);

	if (ppe_vp_num <= 0) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE,
			   "Error in getting VP num for netdev %s err %d\n",
			   dev->name, ppe_vp_num);
		return -ENOSR;
	}

	ahvif->dp_vif.ppe_vp_num = ppe_vp_num;

	ath12k_dbg(NULL, ATH12K_DBG_PPE,
		   "PPE VP assignment: device '%s' VP num %d assigned by ath client\n",
		   dev->name, ahvif->dp_vif.ppe_vp_num);
	return 0;
}
EXPORT_SYMBOL(ath12k_vif_get_vp_num);

static void
ath12k_dp_rx_ppeds_fse_update_flow_info(struct ath12k_base *ab,
					struct rx_flow_info *flow_info,
					struct ppe_drv_fse_rule_info *ppe_flow_info,
					int operation)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;
	struct ppe_drv_fse_tuple *ppe_tuple = &ppe_flow_info->tuple;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "%s S_IP:%x:%x:%x:%x,sPort:%u,D_IP:%x:%x:%x:%x,dPort:%u,Proto:%d,flags:%d",
		   fse_state_to_string(operation),
		   ppe_tuple->src_ip[0], ppe_tuple->src_ip[1],
		   ppe_tuple->src_ip[2], ppe_tuple->src_ip[3],
		   ppe_tuple->src_port,
		   ppe_tuple->dest_ip[0], ppe_tuple->dest_ip[1],
		   ppe_tuple->dest_ip[2], ppe_tuple->dest_ip[3],
		   ppe_tuple->dest_port,
		   ppe_tuple->protocol,
		   ppe_flow_info->flags);

	tuple_info->src_port = ppe_tuple->src_port;
	tuple_info->dest_port = ppe_tuple->dest_port;
	tuple_info->l4_protocol = ppe_tuple->protocol;
	flow_info->fse_metadata = ppe_flow_info->vp_num;

	if (ppe_flow_info->flags & PPE_DRV_FSE_IPV4) {
		flow_info->is_addr_ipv4 = 1;
		tuple_info->src_ip_31_0 = ntohl(ppe_tuple->src_ip[0]);
		tuple_info->dest_ip_31_0 = ntohl(ppe_tuple->dest_ip[0]);
	} else if (ppe_flow_info->flags & PPE_DRV_FSE_IPV6) {
		tuple_info->src_ip_31_0 = ntohl(ppe_tuple->src_ip[3]);
		tuple_info->src_ip_63_32 = ntohl(ppe_tuple->src_ip[2]);
		tuple_info->src_ip_95_64 = ntohl(ppe_tuple->src_ip[1]);
		tuple_info->src_ip_127_96 = ntohl(ppe_tuple->src_ip[0]);

		tuple_info->dest_ip_31_0 = ntohl(ppe_tuple->dest_ip[3]);
		tuple_info->dest_ip_63_32 = ntohl(ppe_tuple->dest_ip[2]);
		tuple_info->dest_ip_95_64 = ntohl(ppe_tuple->dest_ip[1]);
		tuple_info->dest_ip_127_96 = ntohl(ppe_tuple->dest_ip[0]);
	}

	if (ppe_flow_info->flags & PPE_DRV_FSE_DS)
		flow_info->use_ppe = 1;
}

bool
ath12k_dp_rx_ppeds_fse_add_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info)
{
	struct rx_flow_info flow_info = { 0 };
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath12k_base *ab = NULL;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct net_device *dev = ppe_flow_info->dev;
	unsigned long links;
	u8 link_id;

	if (!ath12k_fse_enable)
		return false;

	if (!dev)
		return false;

	wdev = dev->ieee80211_ptr;

	vif = wdev_to_ieee80211_vif_vlan(wdev, false);
	if (!vif) {
		pr_warn("FSE flow rule addition failed vif = NULL\n");
		return false;
	}

	ahvif = ath12k_vif_to_ahvif(vif);

	ah = ahvif->ah;
	if (!ah) {
		pr_warn("FSE flow rule addition failed ah = NULL \n");
		return false;
	}

	links = ahvif->links_map;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ahvif->link[link_id];
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar)
			continue;
		ab = ar->ab;
		if (ab)
			break;
	}

	/* TODO: protect ag->ab[] by spin lock */
	/* NOTE: ag->ab[0] can be any arbitirary ab but first ab is used to cover non-MLO */
	if (!ab) {
		pr_warn("FSE flow rule addition failed ab = NULL \n");
		return false;
	}

	ath12k_dp_rx_ppeds_fse_update_flow_info(ab, &flow_info, ppe_flow_info,
						FSE_RULE_ADD);

	return ath12k_dp_rx_flow_add_entry(ab, &flow_info);
}
EXPORT_SYMBOL(ath12k_dp_rx_ppeds_fse_add_flow_entry);

bool
ath12k_dp_rx_ppeds_fse_del_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info)
{
	struct rx_flow_info flow_info = { 0 };
	struct wireless_dev *wdev;
	struct ath12k_hw *ah;
	struct ieee80211_hw *hw = NULL;
	struct ath12k_base *ab = NULL;
	struct net_device *dev = ppe_flow_info->dev;

	if (!ath12k_fse_enable)
		return false;

	if (!dev)
		return false;

	wdev = dev->ieee80211_ptr;

	hw = wiphy_to_ieee80211_hw(wdev->wiphy);

	if (!hw) {
		pr_warn("failed to find the ieee80211_hw for netdev %p\n", dev);
		return false;
	}

	ah = hw->priv;
	if (!ah) {
		pr_warn("FSE flow rule deletion failed ah = NULL \n");
		return false;
	}

	/* TODO: protect ah->radio[0].ab  by spin lock */
	/* NOTE: ah->radio[0].ab can be any arbitirary ab but first ab is used to cover non-MLO */
	ab = ah->radio[0].ab;
	if (!ab) {
		pr_warn("FSE flow rule deletion failed ab = NULL \n");
		return false;
	}

	ath12k_dp_rx_ppeds_fse_update_flow_info(ab, &flow_info, ppe_flow_info,
						FSE_RULE_DELETE);

	return ath12k_dp_rx_flow_delete_entry(ab, &flow_info);
}
EXPORT_SYMBOL(ath12k_dp_rx_ppeds_fse_del_flow_entry);

void ath12k_dp_ppeds_service_enable_disable(struct ath12k_base *ab,
					    bool enable)
{
	struct ath12k_dp *dp = ab->dp;

	if (enable)
		set_bit(ATH12K_DP_PPEDS_NAPI_DONE_BIT, &dp->service_rings_running);

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->service_status_update(ab->dp->ppe.ds_node_id, enable);
}

void ath12k_dp_ppeds_interrupt_stop(struct ath12k_base *ab)
{
	if (ab->pm_suspend)
		return;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_REO2PPE);
	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
}

void ath12k_dp_ppeds_interrupt_start(struct ath12k_base *ab)
{
	ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_REO2PPE);
	ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
}

int ath12k_nss_plugin_register_ops(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;

	dp->ppe.nss_plugin_ops = qca_nss_wifi_plugins_get_ops();
	nss_plugin_ops_ptr = dp->ppe.nss_plugin_ops;
	if (!dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, " error in registering nss plugin ops\n");
		return -EINVAL;
	}

	ath12k_info(ab, "NSS plugin ops registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ath12k_nss_plugin_register_ops);

void ath12k_nss_plugin_unregister_ops(struct ath12k_base *ab)
{
	ab->dp->ppe.nss_plugin_ops = NULL;
}
EXPORT_SYMBOL(ath12k_nss_plugin_unregister_ops);
