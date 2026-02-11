// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/dma-mapping.h>
#include "debug.h"
#include "hif.h"
#include "pcic.h"
#include "dp_mon.h"

static void ath12k_hal_ce_dst_setup(struct ath12k_base *ab,
				    struct hal_srng *srng, int ring_num)
{
	ab->hal.hal_ops->ce_dst_setup(ab, srng, ring_num);
}

static void ath12k_hal_srng_src_hw_init(struct ath12k_base *ab,
					struct hal_srng *srng,
					u32 restore_idx)
{
	ab->hal.hal_ops->srng_src_hw_init(ab, srng, restore_idx);
}

static void ath12k_hal_srng_dst_hw_init(struct ath12k_base *ab,
					struct hal_srng *srng,
					u32 restore_idx)
{
	ab->hal.hal_ops->srng_dst_hw_init(ab, srng, restore_idx);
}

static void ath12k_hal_set_umac_srng_ptr_addr(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      enum hal_ring_type type, int ring_num)
{
	ab->hal.hal_ops->set_umac_srng_ptr_addr(ab, srng, type, ring_num);
}

static int ath12k_hal_srng_get_ring_id(struct ath12k_hal *hal,
				       enum hal_ring_type type,
				       int ring_num, int mac_id)
{
	return hal->hal_ops->srng_get_ring_id(hal, type, ring_num, mac_id);
}

int ath12k_hal_srng_update_shadow_config(struct ath12k_base *ab,
					 enum hal_ring_type ring_type,
					 int ring_num)
{
	return ab->hal.hal_ops->srng_update_shadow_config(ab, ring_type,
							  ring_num);
}

void ath12k_hal_set_link_desc_addr(struct ath12k_hal *hal,
				   struct wbm_link_desc*desc, u32 cookie,
				   dma_addr_t paddr, int rbm)
{
	hal->hal_ops->set_link_desc_addr((struct hal_wbm_link_desc *)desc,
					 cookie, paddr, rbm);
}

u32 ath12k_hal_ce_get_desc_size(struct ath12k_hal *hal, enum hal_ce_desc type)
{
	return hal->hal_ops->ce_get_desc_size(type);
}

void ath12k_hal_tx_update_dscp_tid_map(struct ath12k_base *ab, int id, u8 dscp, u8 tid)
{
	ab->hal.hal_ops->tx_update_dscp_tid_map(ab, id, dscp, tid);
}

void ath12k_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id)
{
        ab->hal.hal_ops->tx_set_dscp_tid_map(ab, map, id);
}
EXPORT_SYMBOL(ath12k_hal_tx_set_dscp_tid_map);

void ath12k_hal_tx_configure_bank_register(struct ath12k_base *ab,
					   u32 bank_config, u8 bank_id)
{
        ab->hal.hal_ops->tx_configure_bank_register(ab, bank_config, bank_id);
}

void ath12k_hal_reoq_lut_addr_read_enable(struct ath12k_base *ab)
{
	ab->hal.hal_ops->reoq_lut_addr_read_enable(ab);
}

void ath12k_hal_reoq_lut_set_max_peerid(struct ath12k_base *ab)
{
	ab->hal.hal_ops->reoq_lut_set_max_peerid(ab);
}

void ath12k_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr)
{
	ab->hal.hal_ops->write_ml_reoq_lut_addr(ab, paddr);
}

void ath12k_hal_write_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr)
{
	ab->hal.hal_ops->write_reoq_lut_addr(ab, paddr);
}

void ath12k_hal_reo_hw_setup(struct ath12k_base *ab)
{
	ab->hal.hal_ops->reo_hw_setup(ab);
}

void ath12k_hal_reo_init_cmd_ring(struct ath12k_base *ab, struct hal_srng *srng)
{
	ab->hal.hal_ops->reo_init_cmd_ring(ab, srng);
}

void ath12k_hal_rx_buf_addr_info_set(struct ath12k_buffer_addr *b_info,
				     dma_addr_t paddr, u32 cookie,
				     u8 manager)
{
	struct ath12k_buffer_addr *binfo = (struct ath12k_buffer_addr *)b_info;
	u32 paddr_lo, paddr_hi;

	paddr_lo = lower_32_bits(paddr);
	paddr_hi = upper_32_bits(paddr);
	binfo->info0 = le32_encode_bits(paddr_lo, BUFFER_ADDR_INFO0_ADDR);
	binfo->info1 = le32_encode_bits(paddr_hi, BUFFER_ADDR_INFO1_ADDR) |
		le32_encode_bits(cookie, BUFFER_ADDR_INFO1_SW_COOKIE) |
		le32_encode_bits(manager, BUFFER_ADDR_INFO1_RET_BUF_MGR);
}
EXPORT_SYMBOL(ath12k_hal_rx_buf_addr_info_set);

void ath12k_hal_rx_buf_addr_info_get(struct ath12k_buffer_addr *b_info,
				     dma_addr_t *paddr, u32 *cookie,
				     u8 *rbm)
{
	struct ath12k_buffer_addr *binfo = (struct ath12k_buffer_addr *)b_info;

	*paddr = (((u64)le32_get_bits(binfo->info1,
				      BUFFER_ADDR_INFO1_ADDR)) << 32) |
		le32_get_bits(binfo->info0, BUFFER_ADDR_INFO0_ADDR);
	*cookie = le32_get_bits(binfo->info1, BUFFER_ADDR_INFO1_SW_COOKIE);
	*rbm = le32_get_bits(binfo->info1, BUFFER_ADDR_INFO1_RET_BUF_MGR);
}
EXPORT_SYMBOL(ath12k_hal_rx_buf_addr_info_get);

void ath12k_hal_cc_config(struct ath12k_base *ab)
{
        ab->hal.hal_ops->cc_config(ab);
}

enum hal_rx_buf_return_buf_manager
ath12k_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id)
{
	return hal->hal_ops->get_idle_link_rbm(hal, device_id);
}
EXPORT_SYMBOL(ath12k_hal_get_idle_link_rbm);

void ath12k_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab)
{
	ab->hal.hal_ops->reo_shared_qaddr_cache_clear(ab);
}

u8 *
ath12k_hal_rxdesc_get_mpdu_start_addr2(struct ath12k_hal *hal, struct hal_rx_desc *desc)
{
	return hal->hal_ops->rxdesc_get_mpdu_start_addr2(desc);
}

bool ath12k_hal_rx_h_is_decrypted(struct ath12k_hal *hal, struct hal_rx_desc *desc)
{
	return hal->hal_ops->rx_h_is_decrypted(desc);
}
EXPORT_SYMBOL(ath12k_hal_rx_h_is_decrypted);

u32 ath12k_hal_rx_desc_get_mpdu_ppdu_id(struct ath12k_hal *hal,
					struct hal_rx_desc *rx_desc)
{
	return hal->hal_ops->rx_desc_get_mpdu_ppdu_id(rx_desc);
}
EXPORT_SYMBOL(ath12k_hal_rx_desc_get_mpdu_ppdu_id);

u32 hal_rx_desc_get_mpdu_start_tag(struct ath12k_hal *hal,
				   struct hal_rx_desc *rx_desc)
{
	return hal->hal_ops->rx_desc_get_mpdu_start_tag(rx_desc);
}
EXPORT_SYMBOL(hal_rx_desc_get_mpdu_start_tag);

void ath12k_hal_rx_reo_ent_buf_paddr_get(struct ath12k_hal *hal,
					 void *rx_desc, dma_addr_t *paddr,
					 u32 *sw_cookie,
					 struct ath12k_buffer_addr **pp_buf_addr,
					 u8 *rbm, u32 *msdu_cnt)
{
	hal->hal_ops->rx_reo_ent_buf_paddr_get(rx_desc, paddr,
					       sw_cookie, pp_buf_addr,
					       rbm, msdu_cnt);
}
EXPORT_SYMBOL(ath12k_hal_rx_reo_ent_buf_paddr_get);

void ath12k_hal_rx_msdu_list_get(struct ath12k_hal *hal,
				 void *link_desc,
				 void *msdu_list,
				 u16 *num_msdus)
{
	hal->hal_ops->rx_msdu_list_get(link_desc,
				       msdu_list,
				       num_msdus);
}
EXPORT_SYMBOL(ath12k_hal_rx_msdu_list_get);

u8 ath12k_hal_rx_h_l3pad_get(struct ath12k_hal *hal,
			     struct hal_rx_desc *desc)
{
	return hal->hal_ops->rx_h_l3pad_get(desc);
}
EXPORT_SYMBOL(ath12k_hal_rx_h_l3pad_get);

static inline void ath12k_hal_mon_ops_init(struct ath12k_hal *hal,
					   u8 hw_rev)
{
	if (hal->hal_ops->hal_mon_ops_init)
		hal->hal_ops->hal_mon_ops_init(hal, hw_rev);
}

static int ath12k_hal_alloc_cont_rdp(struct ath12k_hal *hal)
{
	size_t size;

	size = sizeof(u32) * HAL_SRNG_RING_ID_MAX;
	hal->rdp.vaddr = ath12k_hal_dma_alloc_coherent(hal->dev, size, &hal->rdp.paddr,
							GFP_KERNEL);
	if (!hal->rdp.vaddr)
		return -ENOMEM;

	return 0;
}

static void ath12k_hal_free_cont_rdp(struct ath12k_hal *hal)
{
	size_t size;

	if (!hal->rdp.vaddr)
		return;

	size = sizeof(u32) * HAL_SRNG_RING_ID_MAX;
	ath12k_hal_dma_free_coherent(hal->dev, size,
				      hal->rdp.vaddr, hal->rdp.paddr);
	hal->rdp.vaddr = NULL;
}

static int ath12k_hal_alloc_cont_wrp(struct ath12k_hal *hal)
{
	size_t size;

	size = sizeof(u32) * (HAL_SRNG_NUM_PMAC_RINGS + HAL_SRNG_NUM_DMAC_RINGS);
	hal->wrp.vaddr = ath12k_hal_dma_alloc_coherent(hal->dev, size, &hal->wrp.paddr,
							GFP_KERNEL);
	if (!hal->wrp.vaddr)
		return -ENOMEM;

	return 0;
}

static void ath12k_hal_free_cont_wrp(struct ath12k_hal *hal)
{
	size_t size;

	if (!hal->wrp.vaddr)
		return;

	size = sizeof(u32) * (HAL_SRNG_NUM_PMAC_RINGS + HAL_SRNG_NUM_DMAC_RINGS);
	ath12k_hal_dma_free_coherent(hal->dev, size,
				      hal->wrp.vaddr, hal->wrp.paddr);
	hal->wrp.vaddr = NULL;
}

static void ath12k_hal_srng_hw_init(struct ath12k_base *ab,
				    struct hal_srng *srng,
				    u32 idx)
{
	if (srng->ring_dir == HAL_SRNG_DIR_SRC)
		ath12k_hal_srng_src_hw_init(ab, srng, idx);
	else
		ath12k_hal_srng_dst_hw_init(ab, srng, idx);
}

int ath12k_hal_srng_get_entrysize(struct ath12k_base *ab, u32 ring_type)
{
	struct hal_srng_config *srng_config;

	if (WARN_ON(ring_type >= HAL_MAX_RING_TYPES))
		return -EINVAL;

	srng_config = &ab->hal.srng_config[ring_type];

	return (srng_config->entry_size << 2);
}
EXPORT_SYMBOL(ath12k_hal_srng_get_entrysize);

int ath12k_hal_srng_get_max_entries(struct ath12k_base *ab, u32 ring_type)
{
	struct hal_srng_config *srng_config;

	if (WARN_ON(ring_type >= HAL_MAX_RING_TYPES))
		return -EINVAL;

	srng_config = &ab->hal.srng_config[ring_type];

	return (srng_config->max_size / srng_config->entry_size);
}

void ath12k_hal_srng_get_params(struct ath12k_base *ab, struct hal_srng *srng,
				struct hal_srng_params *params)
{
	params->ring_base_paddr = srng->ring_base_paddr;
	params->ring_base_vaddr = srng->ring_base_vaddr;
	params->num_entries = srng->num_entries;
	params->intr_timer_thres_us = srng->intr_timer_thres_us;
	params->intr_batch_cntr_thres_entries =
		srng->intr_batch_cntr_thres_entries;
	params->low_threshold = srng->u.src_ring.low_threshold;
	params->msi_addr = srng->msi_addr;
	params->msi2_addr = srng->msi2_addr;
	params->msi_data = srng->msi_data;
	params->msi2_data = srng->msi2_data;
	params->flags = srng->flags;
}
EXPORT_SYMBOL(ath12k_hal_srng_get_params);

dma_addr_t ath12k_hal_srng_get_hp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng)
{
	if (!(srng->flags & HAL_SRNG_FLAGS_LMAC_RING))
		return 0;

	if (srng->ring_dir == HAL_SRNG_DIR_SRC)
		return ab->hal.wrp.paddr +
		       ((unsigned long)srng->u.src_ring.hp_addr -
			(unsigned long)ab->hal.wrp.vaddr);
	else
		return ab->hal.rdp.paddr +
		       ((unsigned long)srng->u.dst_ring.hp_addr -
			 (unsigned long)ab->hal.rdp.vaddr);
}

dma_addr_t ath12k_hal_srng_get_tp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng)
{
	if (!(srng->flags & HAL_SRNG_FLAGS_LMAC_RING))
		return 0;

	if (srng->ring_dir == HAL_SRNG_DIR_SRC)
		return ab->hal.rdp.paddr +
		       ((unsigned long)srng->u.src_ring.tp_addr -
			(unsigned long)ab->hal.rdp.vaddr);
	else
		return ab->hal.wrp.paddr +
		       ((unsigned long)srng->u.dst_ring.tp_addr -
			(unsigned long)ab->hal.wrp.vaddr);
}

void ath12k_hal_ce_src_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_src_desc *desc,
				dma_addr_t paddr, u32 len, u32 id,
				u8 byte_swap_data)
{
	hal->hal_ops->ce_src_set_desc(desc, paddr, len, id, byte_swap_data);
}

void ath12k_hal_ce_dst_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_dest_desc *desc,
				dma_addr_t paddr)
{
	hal->hal_ops->ce_dst_set_desc(desc, paddr);
}

u32 ath12k_hal_ce_dst_status_get_length(struct ath12k_hal *hal,
                                        struct hal_ce_srng_dst_status_desc *desc)
{
        return hal->hal_ops->ce_dst_status_get_length(desc);
}

void *ath12k_hal_srng_dst_peek(struct ath12k_base *ab, struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	if (srng->u.dst_ring.tp != srng->u.dst_ring.cached_hp)
		return (srng->ring_base_vaddr + srng->u.dst_ring.tp);

	return NULL;
}
EXPORT_SYMBOL(ath12k_hal_srng_dst_peek);

void *__ath12k_hal_srng_dst_peek(struct hal_srng *srng)
{
	if (srng->u.dst_ring.tp != srng->u.dst_ring.cached_hp)
		return (srng->ring_base_vaddr + srng->u.dst_ring.tp);

	return NULL;
}
EXPORT_SYMBOL(__ath12k_hal_srng_dst_peek);

void *ath12k_hal_srng_dst_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng)
{
	void *desc;

	lockdep_assert_held(&srng->lock);

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.dst_ring.tp;

	srng->u.dst_ring.tp = (srng->u.dst_ring.tp + srng->entry_size);

        /* wrap around to start of ring*/
        if (srng->u.dst_ring.tp == srng->ring_size)
                srng->u.dst_ring.tp = 0;

#ifndef CONFIG_IO_COHERENCY
	if (srng->flags & HAL_SRNG_FLAGS_CACHED)
		dma_sync_single_for_cpu(ab->dev, virt_to_phys(desc),
					(srng->entry_size * sizeof(u32)),
					DMA_FROM_DEVICE);
#endif
	return desc;
}
EXPORT_SYMBOL(ath12k_hal_srng_dst_get_next_entry);

void *__ath12k_hal_srng_dst_get_next_cached_entry(struct hal_srng *srng,
						  u32 *old_tp)
{
	void *desc;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.dst_ring.tp;

	srng->u.dst_ring.tp = (srng->u.dst_ring.tp + srng->entry_size);

        /* wrap around to start of ring*/
        if (srng->u.dst_ring.tp == srng->ring_size)
                srng->u.dst_ring.tp = 0;

	if (old_tp)
		*old_tp = srng->u.dst_ring.tp;

	prefetch(srng->ring_base_vaddr + srng->u.dst_ring.tp);
	return desc;
}
EXPORT_SYMBOL(__ath12k_hal_srng_dst_get_next_cached_entry);

void *ath12k_hal_srng_dst_get_next_cached_entry(struct ath12k_base *ab,
						struct hal_srng *srng,
						u32 *old_tp)
{
	lockdep_assert_held(&srng->lock);

	return __ath12k_hal_srng_dst_get_next_cached_entry(srng, old_tp);

}
EXPORT_SYMBOL(ath12k_hal_srng_dst_get_next_cached_entry);

int __ath12k_hal_srng_dst_num_free(struct hal_srng *srng, bool sync_hw_ptr)
{
	u32 tp, hp;

	tp = srng->u.dst_ring.tp;

	if (sync_hw_ptr) {
		hp = *srng->u.dst_ring.hp_addr;
		srng->u.dst_ring.cached_hp = hp;
	} else {
		hp = srng->u.dst_ring.cached_hp;
	}

	if (hp >= tp)
		return (hp - tp) / srng->entry_size;
	else
		return (srng->ring_size - tp + hp) / srng->entry_size;
}
EXPORT_SYMBOL(__ath12k_hal_srng_dst_num_free);

int ath12k_hal_srng_dst_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr)
{
	lockdep_assert_held(&srng->lock);

	return __ath12k_hal_srng_dst_num_free(srng, sync_hw_ptr);
}
EXPORT_SYMBOL(ath12k_hal_srng_dst_num_free);

void ath12k_hal_srng_dst_invalidate_entry(struct ath12k_dp *dp,
					  struct hal_srng *srng, int entries)
{
	u32 *desc, tp, hp;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED))
	        return;

	tp = srng->u.dst_ring.tp;
	hp = srng->u.dst_ring.cached_hp;

	desc = srng->ring_base_vaddr + tp;
	if (hp > tp) {
		dma_sync_single_for_cpu(dp->dev, virt_to_phys(desc),
					entries * srng->entry_size * sizeof(u32),
					DMA_FROM_DEVICE);
	} else {
		entries = srng->ring_size - tp;
		dma_sync_single_for_cpu(dp->dev, virt_to_phys(desc),
					entries * sizeof(u32),
					DMA_FROM_DEVICE);
		entries = hp;
		dma_sync_single_for_cpu(dp->dev, virt_to_phys(srng->ring_base_vaddr),
					entries * sizeof(u32),
					DMA_FROM_DEVICE);
	}
}
EXPORT_SYMBOL(ath12k_hal_srng_dst_invalidate_entry);

/* Returns number of available entries in src ring */
int ath12k_hal_srng_src_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr)
{
	u32 tp, hp;

	lockdep_assert_held(&srng->lock);

	hp = srng->u.src_ring.hp;

	if (sync_hw_ptr) {
		tp = *srng->u.src_ring.tp_addr;
		srng->u.src_ring.cached_tp = tp;
	} else {
		tp = srng->u.src_ring.cached_tp;
	}

	if (tp > hp)
		return ((tp - hp) / srng->entry_size) - 1;
	else
		return ((srng->ring_size - hp + tp) / srng->entry_size) - 1;
}
EXPORT_SYMBOL(ath12k_hal_srng_src_num_free);

void *ath12k_hal_srng_src_next_peek(struct ath12k_base *ab,
				    struct hal_srng *srng)
{
	void *desc;
	u32 next_hp;

	lockdep_assert_held(&srng->lock);

	next_hp = (srng->u.src_ring.hp + srng->entry_size) % srng->ring_size;

	if (next_hp == srng->u.src_ring.cached_tp)
		return NULL;

	desc = srng->ring_base_vaddr + next_hp;

	return desc;
}
EXPORT_SYMBOL(ath12k_hal_srng_src_next_peek);

void *ath12k_hal_srng_src_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng)
{
	void *desc;
	u32 next_hp;

	lockdep_assert_held(&srng->lock);

	/* TODO: Using % is expensive, but we have to do this since size of some
	 * SRNG rings is not power of 2 (due to descriptor sizes). Need to see
	 * if separate function is defined for rings having power of 2 ring size
	 * (TCL2SW, REO2SW, SW2RXDMA and CE rings) so that we can avoid the
	 * overhead of % by using mask (with &).
	 */
	next_hp = (srng->u.src_ring.hp + srng->entry_size) % srng->ring_size;

	if (next_hp == srng->u.src_ring.cached_tp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = next_hp;

	/* TODO: Reap functionality is not used by all rings. If particular
	 * ring does not use reap functionality, we need not update reap_hp
	 * with next_hp pointer. Need to make sure a separate function is used
	 * before doing any optimization by removing below code updating
	 * reap_hp.
	 */
	srng->u.src_ring.reap_hp = next_hp;

	return desc;
}
EXPORT_SYMBOL(ath12k_hal_srng_src_get_next_entry);

void *ath12k_hal_srng_src_peek(struct ath12k_base *ab, struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	if (((srng->u.src_ring.hp + srng->entry_size) % srng->ring_size) ==
	    srng->u.src_ring.cached_tp)
		return NULL;

	return srng->ring_base_vaddr + srng->u.src_ring.hp;
}
EXPORT_SYMBOL(ath12k_hal_srng_src_peek);

void *ath12k_hal_srng_src_reap_next(struct ath12k_base *ab,
				    struct hal_srng *srng)
{
	void *desc;
	u32 next_reap_hp;

	lockdep_assert_held(&srng->lock);

	next_reap_hp = (srng->u.src_ring.reap_hp + srng->entry_size) %
		       srng->ring_size;

	if (next_reap_hp == srng->u.src_ring.cached_tp)
		return NULL;

	desc = srng->ring_base_vaddr + next_reap_hp;
	srng->u.src_ring.reap_hp = next_reap_hp;

	return desc;
}

void *ath12k_hal_srng_src_get_next_reaped(struct ath12k_base *ab,
					  struct hal_srng *srng)
{
	void *desc;

	lockdep_assert_held(&srng->lock);

	if (srng->u.src_ring.hp == srng->u.src_ring.reap_hp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = (srng->u.src_ring.hp + srng->entry_size) %
			      srng->ring_size;

	return desc;
}

u32 __ath12k_hal_srng_access_begin(struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		srng->u.src_ring.cached_tp =
			*(volatile u32 *)srng->u.src_ring.tp_addr;
		return srng->u.src_ring.hp;
	} else {
		srng->u.dst_ring.cached_hp = *srng->u.dst_ring.hp_addr;
		return srng->u.dst_ring.tp;
	}
}
EXPORT_SYMBOL(__ath12k_hal_srng_access_begin);

u32 ath12k_hal_srng_access_begin(struct ath12k_base *ab, struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	return __ath12k_hal_srng_access_begin(srng);
}
EXPORT_SYMBOL(ath12k_hal_srng_access_begin);

u32 ath12k_hal_srng_access_begin_no_lock(struct hal_srng *srng)
{
	return __ath12k_hal_srng_access_begin(srng);
}
EXPORT_SYMBOL(ath12k_hal_srng_access_begin_no_lock);

void __ath12k_hal_srng_update_tp(struct hal_srng *srng, u32 new_tp)
{
	srng->u.dst_ring.tp = new_tp;
}
EXPORT_SYMBOL(__ath12k_hal_srng_update_tp);

void ath12k_hal_srng_update_tp(struct hal_srng *srng, u32 new_tp)
{
	lockdep_assert_held(&srng->lock);

	__ath12k_hal_srng_update_tp(srng, new_tp);
}
EXPORT_SYMBOL(ath12k_hal_srng_update_tp);

/* Update cached ring head/tail pointers to HW. ath12k_hal_srng_access_begin()
 * should have been called before this.
 */
void __ath12k_hal_srng_access_end(struct ath12k_base *ab, struct hal_srng *srng)
{
	/* TODO: See if we need a write memory barrier here */
	if (srng->flags & HAL_SRNG_FLAGS_LMAC_RING) {
		/* For LMAC rings, ring pointer updates are done through FW and
		 * hence written to a shared memory location that is read by FW
		 */
		if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
			srng->u.src_ring.last_tp =
				*(volatile u32 *)srng->u.src_ring.tp_addr;
			*srng->u.src_ring.hp_addr = srng->u.src_ring.hp;
		} else {
			srng->u.dst_ring.last_hp = *srng->u.dst_ring.hp_addr;
			*srng->u.dst_ring.tp_addr = srng->u.dst_ring.tp;
		}
	} else {
		if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
			srng->u.src_ring.last_tp =
				*(volatile u32 *)srng->u.src_ring.tp_addr;
			ath12k_hif_write32(ab,
					   (unsigned long)srng->u.src_ring.hp_addr -
					   (unsigned long)ab->mem,
					   srng->u.src_ring.hp);
		} else {
			srng->u.dst_ring.last_hp = *srng->u.dst_ring.hp_addr;
			ath12k_hif_write32(ab,
					   (unsigned long)srng->u.dst_ring.tp_addr -
					   (unsigned long)ab->mem,
					   srng->u.dst_ring.tp);
		}
	}

	srng->timestamp = jiffies;
}
EXPORT_SYMBOL(__ath12k_hal_srng_access_end);

void ath12k_hal_srng_access_end(struct ath12k_base *ab, struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	__ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_hal_srng_access_end);

void ath12k_hal_srng_access_end_no_lock(struct ath12k_base *ab,
					struct hal_srng *srng)
{
	 __ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_hal_srng_access_end_no_lock);

void ath12k_hal_setup_link_idle_list(struct ath12k_base *ab,
				     struct wbm_idle_scatter_list *sbuf,
				     u32 nsbufs, u32 tot_link_desc,
				     u32 end_offset)
{
	ab->hal.hal_ops->setup_link_idle_list(ab, (struct hal_wbm_idle_scatter_list *)sbuf, nsbufs,
					      tot_link_desc, end_offset);
}

static bool hal_tx_ppe2tcl_ring_halt_get(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	return (regval &
			1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT);
}

static void hal_tx_ppe2tcl_ring_halt_set(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval |= (1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT);

	/* Enable ring halt for the ppe2tcl ring */
	ath12k_hif_write32(ab, cmn_reg_addr, regval);
}

static void hal_tx_ppe2tcl_ring_halt_reset(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval &= ~(1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT);

	/* Disable ring halt for the ppe2tcl ring */
	ath12k_hif_write32(ab, cmn_reg_addr, regval);
}

static bool hal_tx_ppe2tcl_ring_halt_done(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;

	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval &= (1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_STAT_SHFT);

	return !!regval;
}

int ath12k_hal_srng_setup_idx(struct ath12k_base *ab, enum hal_ring_type type,
			      int ring_num, int mac_id,
			      struct hal_srng_params *params, u32 restore_idx)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_srng_config *srng_config = &ab->hal.srng_config[type];
	struct hal_srng *srng;
	int ring_id;
	u32 idx;
	int i, retry_count = 0;

	ring_id = ath12k_hal_srng_get_ring_id(hal, type, ring_num, mac_id);
	if (ring_id < 0)
		return ring_id;

	srng = &hal->srng_list[ring_id];

	srng->ring_id = ring_id;
	srng->ring_dir = srng_config->ring_dir;
	srng->ring_base_paddr = params->ring_base_paddr;
	srng->ring_base_vaddr = params->ring_base_vaddr;
	srng->entry_size = srng_config->entry_size;
	srng->num_entries = params->num_entries;
	srng->ring_size = srng->entry_size * srng->num_entries;
	srng->intr_batch_cntr_thres_entries =
				params->intr_batch_cntr_thres_entries;
	srng->intr_timer_thres_us = params->intr_timer_thres_us;
	srng->flags = params->flags;
	srng->msi_addr = params->msi_addr;
	srng->msi2_addr = params->msi2_addr;
	srng->msi_data = params->msi_data;
	srng->msi2_data = params->msi2_data;
	srng->initialized = 1;
	spin_lock_init(&srng->lock);
	lockdep_set_class(&srng->lock, &srng->lock_key);

	for (i = 0; i < HAL_SRNG_NUM_REG_GRP; i++) {
		srng->hwreg_base[i] = srng_config->reg_start[i] +
				      (ring_num * srng_config->reg_size[i]);
	}

	memset(srng->ring_base_vaddr, 0,
	       (srng->entry_size * srng->num_entries) << 2);

	if (srng->flags & HAL_SRNG_FLAGS_CACHED) {
		dma_sync_single_for_cpu(ab->dev, virt_to_phys(srng->ring_base_vaddr),
					(srng->entry_size * srng->num_entries * sizeof(u32)),
					DMA_FROM_DEVICE);
	}

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		srng->u.src_ring.hp = 0;
		srng->u.src_ring.cached_tp = 0;
		srng->u.src_ring.reap_hp = srng->ring_size - srng->entry_size;
		srng->u.src_ring.tp_addr = (void *)(hal->rdp.vaddr + ring_id);
		srng->u.src_ring.low_threshold = params->low_threshold *
						 srng->entry_size;

		if (srng->u.src_ring.tp_addr)
                        *srng->u.src_ring.tp_addr = 0;

		if (srng_config->mac_type == ATH12K_HAL_SRNG_UMAC) {
			ath12k_hal_set_umac_srng_ptr_addr(ab, srng, type, ring_num);
		} else {
			idx = ring_id - HAL_SRNG_RING_ID_DMAC_CMN_ID_START;
			srng->u.src_ring.hp_addr = (void *)(hal->wrp.vaddr +
						   idx);
			if (srng->u.src_ring.hp_addr)
                                *srng->u.src_ring.hp_addr = 0;

			srng->flags |= HAL_SRNG_FLAGS_LMAC_RING;
		}
	} else {
		/* During initialization loop count in all the descriptors
		 * will be set to zero, and HW will set it to 1 on completing
		 * descriptor update in first loop, and increments it by 1 on
		 * subsequent loops (loop count wraps around after reaching
		 * 0xffff). The 'loop_cnt' in SW ring state is the expected
		 * loop count in descriptors updated by HW (to be processed
		 * by SW).
		 */
		srng->u.dst_ring.loop_cnt = 1;
		srng->u.dst_ring.tp = 0;
		srng->u.dst_ring.cached_hp = 0;
		srng->u.dst_ring.hp_addr = (void *)(hal->rdp.vaddr + ring_id);

		if (srng->u.dst_ring.hp_addr)
                        *srng->u.dst_ring.hp_addr = 0;

		if (srng_config->mac_type == ATH12K_HAL_SRNG_UMAC) {
			ath12k_hal_set_umac_srng_ptr_addr(ab, srng, type, ring_num);
		} else {
			/* For PMAC & DMAC rings, tail pointer updates will be done
			 * through FW by writing to a shared memory location
			 */
			idx = ring_id - HAL_SRNG_RING_ID_DMAC_CMN_ID_START;
			srng->u.dst_ring.tp_addr = (void *)(hal->wrp.vaddr +
						   idx);
			if (srng->u.dst_ring.tp_addr)
                                *srng->u.dst_ring.tp_addr = 0;

			srng->flags |= HAL_SRNG_FLAGS_LMAC_RING;
		}
	}

	if (srng_config->mac_type != ATH12K_HAL_SRNG_UMAC)
		return ring_id;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (restore_idx) {
		/* During UMAC reset Tx ring halt is set
		 * by Wi-Fi FW during pre-reset stage.
		 * Hence skip halting the rings again
		 */
		if (ath12k_dp_umac_reset_in_progress(ab)) {
			if (!(hal_tx_ppe2tcl_ring_halt_get(ab))) {
				ath12k_warn(ab, "TX ring halt not set\n");
				WARN_ON(1);
			}
			ath12k_hal_srng_hw_init(ab, srng, restore_idx);
		} else {
			hal_tx_ppe2tcl_ring_halt_set(ab);
			do {
				ath12k_warn(ab, "Waiting for ring reset, retried count: %d\n",
					    retry_count);
				mdelay(RING_HALT_TIMEOUT);
				retry_count++;
			} while (!(hal_tx_ppe2tcl_ring_halt_done(ab)) &&
				 (retry_count < RNG_HALT_STAT_RETRY_COUNT));

			if (retry_count >= RNG_HALT_STAT_RETRY_COUNT)
				ath12k_err(ab, "Ring halt is failed, retried count: %d\n",
					   retry_count);

			ath12k_hal_srng_hw_init(ab, srng, restore_idx);
			hal_tx_ppe2tcl_ring_halt_reset(ab);
		}
	} else {
		ath12k_hal_srng_hw_init(ab, srng, 0);
	}
#else
	ath12k_hal_srng_hw_init(ab, srng, 0);
#endif

	if (type == HAL_CE_DST) {
		srng->u.dst_ring.max_buffer_length = params->max_buffer_len;
		ath12k_hal_ce_dst_setup(ab, srng, ring_num);
	}

	return ring_id;
}

void ath12k_hal_srng_shadow_config(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	int ring_type, ring_num;

	/* update all the non-CE srngs. */
	for (ring_type = 0; ring_type < HAL_MAX_RING_TYPES; ring_type++) {
		struct hal_srng_config *srng_config = &hal->srng_config[ring_type];

		if (ring_type == HAL_CE_SRC ||
		    ring_type == HAL_CE_DST ||
			ring_type == HAL_CE_DST_STATUS)
			continue;

		if (srng_config->mac_type == ATH12K_HAL_SRNG_DMAC ||
		    srng_config->mac_type == ATH12K_HAL_SRNG_PMAC)
			continue;

		for (ring_num = 0; ring_num < srng_config->max_rings; ring_num++)
			ath12k_hal_srng_update_shadow_config(ab, ring_type, ring_num);
	}
}

void ath12k_hal_reo_config_reo2ppe_dest_info(struct ath12k_base *ab)
{
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 val = HAL_REO1_REO2PPE_DST_VAL;

	ath12k_hif_write32(ab, reo_base + HAL_REO1_REO2PPE_DST_INFO,
			   val);
}

void ath12k_hal_srng_get_shadow_config(struct ath12k_base *ab,
				       u32 **cfg, u32 *len)
{
	struct ath12k_hal *hal = &ab->hal;

	*len = hal->num_shadow_reg_configured;
	*cfg = hal->shadow_reg_addr;
}

void ath12k_hal_srng_shadow_update_hp_tp(struct ath12k_base *ab,
					 struct hal_srng *srng)
{
	lockdep_assert_held(&srng->lock);

	/* check whether the ring is empty. Update the shadow
	 * HP only when then ring isn't' empty.
	 */
	if (srng->ring_dir == HAL_SRNG_DIR_SRC &&
	    *srng->u.src_ring.tp_addr != srng->u.src_ring.hp)
		ath12k_hal_srng_access_end(ab, srng);
}

static void ath12k_hal_srng_destroy_config(struct ath12k_hal *hal)
{
	kfree(hal->srng_config);
	hal->srng_config = NULL;
}

static void ath12k_hal_register_srng_lock_keys(struct ath12k_hal *hal)
{
	u32 ring_id;

	for (ring_id = 0; ring_id < HAL_SRNG_RING_ID_MAX; ring_id++)
		lockdep_register_key(&hal->srng_list[ring_id].lock_key);
}

static void ath12k_hal_unregister_srng_lock_keys(struct ath12k_hal *hal)
{
	u32 ring_id;

	for (ring_id = 0; ring_id < HAL_SRNG_RING_ID_MAX; ring_id++)
		lockdep_unregister_key(&hal->srng_list[ring_id].lock_key);
}

int ath12k_hal_srng_init(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	int ret;

	memset(hal, 0, sizeof(*hal));

	ret = ab->hw_params->hal_ops->hal_init(hal, ab->hw_params->hw_rev);
	if (ret)
		goto err_hal;

	ret = ab->hw_params->hal_ops->create_srng_config(hal);
	if (ret)
		goto err_hal;

	hal->dev = ab->dev;

	ret = ath12k_hal_alloc_cont_rdp(hal);
	if (ret)
		goto err_hal;

	ret = ath12k_hal_alloc_cont_wrp(hal);
	if (ret)
		goto err_free_cont_rdp;

	ath12k_hal_register_srng_lock_keys(hal);

	ath12k_hal_mon_ops_init(hal, ab->hw_params->hw_rev);

	return 0;

err_free_cont_rdp:
	ath12k_hal_free_cont_rdp(hal);

err_hal:
	return ret;
}

void ath12k_hal_srng_deinit(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;

	ath12k_hal_unregister_srng_lock_keys(hal);
	ath12k_hal_free_cont_rdp(hal);
	ath12k_hal_free_cont_wrp(hal);
	ath12k_hal_srng_destroy_config(hal);
}

void ath12k_hal_dump_srng_stats(struct ath12k_base *ab)
{
	struct hal_srng *srng;
	struct ath12k_ext_irq_grp *irq_grp;
	struct ath12k_ce_pipe *ce_pipe;
	int i;

	ath12k_err(ab, "Last interrupt received for each CE:\n");
	for (i = 0; i < ab->hw_params->ce_count; i++) {
		ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		ath12k_err(ab,"CE_id %d pipe_num %d %ums before ce_manual_poll_count %d "
				"ce_last_manual_tasklet_schedule_ts %ums before\n",
			   i, ce_pipe->pipe_num,
			   jiffies_to_msecs(jiffies - ce_pipe->timestamp),
			   ce_pipe->ce_manual_poll_count,
			   jiffies_to_msecs(jiffies - ce_pipe->last_ce_manual_poll_ts));
	}

	ath12k_err(ab, "\nLast interrupt received for each group:\n");
	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		irq_grp = &ab->ext_irq_grp[i];
		ath12k_err(ab, "group_id %d %ums before\n",
			   irq_grp->grp_id,
			   jiffies_to_msecs(jiffies - irq_grp->timestamp));
	}

	for (i = 0; i < HAL_SRNG_RING_ID_MAX; i++) {
		srng = &ab->hal.srng_list[i];

		if (!srng->initialized)
			continue;

		if (srng->ring_dir == HAL_SRNG_DIR_SRC)
			ath12k_err(ab,
				   "src srng id %u hp %u, reap_hp %u, cur tp %u, cached tp %u last tp %u napi processed before %ums\n",
				   srng->ring_id, srng->u.src_ring.hp,
				   srng->u.src_ring.reap_hp,
				   *srng->u.src_ring.tp_addr, srng->u.src_ring.cached_tp,
				   srng->u.src_ring.last_tp,
				   jiffies_to_msecs(jiffies - srng->timestamp));
		else if (srng->ring_dir == HAL_SRNG_DIR_DST)
			ath12k_err(ab,
				   "dst srng id %u tp %u, cur hp %u, cached hp %u last hp %u napi processed before %ums\n",
				   srng->ring_id, srng->u.dst_ring.tp,
				   *srng->u.dst_ring.hp_addr,
				   srng->u.dst_ring.cached_hp,
				   srng->u.dst_ring.last_hp,
				   jiffies_to_msecs(jiffies - srng->timestamp));
	}
}

void ath12k_hal_vdev_mcast_ctrl_set(struct ath12k_base *ab, u32 vdev_id,
				    u8 mcast_ctrl_val)
{
	u32 reg_addr, val, reg_val;
	u8 reg_idx, index_in_reg;

	reg_idx = HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id);
	index_in_reg = HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id);

	reg_addr = HAL_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_ADDR(reg_idx);
	val = ath12k_hif_read32(ab, reg_addr);

	val &= (~(HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK <<
		  (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg)));

	reg_val = val |
		  ((HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK & mcast_ctrl_val) <<
		   (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg));

	ath12k_hif_write32(ab, reg_addr, reg_val);
}

void ath12k_hal_srng_hw_disable(struct ath12k_base *ab,
				struct hal_srng *srng)
{
	ab->hal.hal_ops->srng_hw_disable(ab, srng);
}

void ath12k_hal_reset_rx_reo_tid_q(struct ath12k_hal *hal,
				   void *qdesc,
				   u32 ba_window_size, u8 tid)
{
	hal->hal_ops->reset_rx_reo_tid_q(qdesc, ba_window_size, tid);
}

void ath12k_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
					       u32 ppeds_idx_map_val)
{
	u32 reg_addr;

	reg_addr = HAL_TCL_PPE_INDEX_MAPPING_TABLE_n_ADDR(HAL_SEQ_WCSS_UMAC_TCL_REG, idx);

	ath12k_hif_write32(ab, reg_addr, ppeds_idx_map_val);
}

static
void ath12k_hal_get_hw_hptp(struct ath12k_base *ab, enum hal_ring_type type,
			    struct hal_srng *srng, uint32_t *hp, uint32_t *tp)
{
	ab->hal.hal_ops->get_hw_hptp(ab, type, srng, hp, tp);
}

static inline
uint32_t ath12k_hal_get_ring_usage(struct ath12k_hal *hal,
		struct hal_srng *srng, enum hal_ring_type ring_type,
		uint32_t *hp, uint32_t *tp)
{
	u32 num_avail, num_valid = 0;
        u32 ring_usage;

        if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
                if (*tp > *hp)
                        num_avail =  ((*tp - *hp) / srng->entry_size);
                else
                        num_avail = ((srng->ring_size - *hp + *tp) /
                                     srng->entry_size);
                if (ring_type == HAL_WBM_IDLE_LINK)
                        num_valid = num_avail;
                else
                        num_valid = srng->num_entries - num_avail;
        } else {
                if (*hp >= *tp)
                        num_valid = ((*hp - *tp) / srng->entry_size);
                else
                        num_valid = ((srng->ring_size - *tp + *hp) /
                                     srng->entry_size);
        }
        ring_usage = (100 * num_valid) / srng->num_entries;
        return ring_usage;
}

static inline
void ath12k_hal_get_sw_hptp(struct hal_srng *srng, uint32_t *hp, uint32_t *tp)
{
	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
                *hp = srng->u.src_ring.hp;
                *tp = *srng->u.src_ring.tp_addr;
        } else {
                *tp = srng->u.dst_ring.tp;
                *hp = *srng->u.dst_ring.hp_addr;
        }
}

ssize_t ath12k_hal_dump_ring_stats(struct ath12k_base *ab, enum hal_ring_type type,
				int ring_id, char *buf, int size)
{

	struct ath12k_hal *hal = &ab->hal;
	struct hal_srng *srng;
	u32 tp, hp, ring_usage;
	int hw_hp = -1, hw_tp = -1;
	const char *ring_name;
	int len = 0;

	srng = &hal->srng_list[ring_id];

	spin_lock_bh(&srng->lock);
	if (srng && srng->initialized) {
		ring_name = hal->srng_config[type].name;

		ath12k_hal_get_sw_hptp(srng, &hp, &tp);
		ring_usage = ath12k_hal_get_ring_usage(hal, srng, type, &hp, &tp);
		len += scnprintf(buf + len, size - len,
				 "%-20s %-20s %10d %10d %12u %12u\n",
				 ring_name, "SW ", hp, tp, ring_usage,
				 jiffies_to_msecs(jiffies - srng->timestamp));

		ath12k_hal_get_hw_hptp(ab, type, srng, &hw_hp, &hw_tp);
		ring_usage = 0;

                if (hw_hp >= 0 && hw_tp >= 0)
			ring_usage = ath12k_hal_get_ring_usage(hal, srng, type,
					&hw_hp, &hw_tp);
		len += scnprintf(buf + len, size - len,
				 "%-20s %-20s %10d %10d %12u %12u\n",
				 ring_name, "HW ", hw_hp, hw_tp, ring_usage,
				 jiffies_to_msecs(jiffies - srng->timestamp));
	}
	spin_unlock_bh(&srng->lock);
	return len;
}

ssize_t ath12k_debugfs_hal_dump_srng_stats(struct ath12k_base *ab, char *buf, int size)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_ext_irq_grp *irq_grp;
	struct ath12k_ce_pipe *ce_pipe;
	int len =0 ;
	u32 i, pdev;

	len += scnprintf(buf + len, size - len, "Last interrupt received for each CE:\n");
	for (i = 0; i < ab->hw_params->ce_count; i++) {
		ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
				continue;

		spin_lock_bh(&ab->ce.ce_lock);
		len += scnprintf(buf + len, size - len,
				"CE_id %d pipe_num %d %ums before ce_manual_poll_count %d ce_last_manual_tasklet_schedule_ts %ums before\n",
				i, ce_pipe->pipe_num,
				jiffies_to_msecs(jiffies - ce_pipe->timestamp),
				ce_pipe->ce_manual_poll_count,
				jiffies_to_msecs(jiffies - ce_pipe->last_ce_manual_poll_ts));
		spin_unlock_bh(&ab->ce.ce_lock);
	}

	len += scnprintf(buf + len, size - len, "\nLast interrupt received for each group:\n");
	len += scnprintf(buf + len, size - len, "%5s %20s\n", "group_id", "delay in ms");
	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		irq_grp = &ab->ext_irq_grp[i];
		len += scnprintf(buf + len, size - len, "%-20d  %-20u\n",
				 irq_grp->grp_id,
				 jiffies_to_msecs(jiffies - irq_grp->timestamp));
	}

	len += scnprintf(buf + len, size - len, "%-20s %-20s %10s %10s %12s %12s\n",
			 "Ring Name", "SW/HW", "Head",
			 "Tail", "Ring Usage", "napi processed before ms");

	/*umac rings*/
	len += ath12k_hal_dump_ring_stats(ab, HAL_WBM_IDLE_LINK,
			dp->wbm_idle_ring.ring_id,
			buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_REO_EXCEPTION,
			dp->reo_except_ring.ring_id,
			buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_REO_REINJECT,
			dp->reo_reinject_ring.ring_id,
                        buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_REO_CMD,
			dp->reo_cmd_ring.ring_id,
                        buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_REO_STATUS,
			dp->reo_status_ring.ring_id,
                        buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_WBM2SW_RELEASE,
			dp->rx_rel_ring.ring_id,
			buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_SW2WBM_RELEASE,
			dp->wbm_desc_rel_ring.ring_id,
                        buf + len, size - len);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_REO_DST,
				dp->reo_dst_ring[i].ring_id,
				buf + len, size - len);

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_TCL_DATA,
				dp->tx_ring[i].tcl_data_ring.ring_id,
				buf + len, size - len);

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_WBM2SW_RELEASE,
                               dp->tx_ring[i].tcl_comp_ring.ring_id,
			       buf + len, size - len);

	/*lmac rings*/
	len += ath12k_hal_dump_ring_stats(ab, HAL_RXDMA_BUF,
			dp->rx_refill_buf_ring.refill_buf_ring.ring_id,
                        buf + len, size - len);

	for (pdev = 0; pdev < MAX_RADIOS; pdev++) {
		rcu_read_lock();
		dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev);
		if (!dp_pdev) {
			rcu_read_unlock();
			continue;
		}
		dp_mon_pdev = dp_pdev->dp_mon_pdev;
		for (i = 0; i < MAX_RXDMA_PER_PDEV; i++) {
			len += ath12k_hal_dump_ring_stats(ab, HAL_RXDMA_MONITOR_DST,
				dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id,
				buf + len, size - len);
		}
		rcu_read_unlock();
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_RXDMA_DST,
			dp->rxdma_err_dst_ring[i].ring_id,
                        buf + len, size - len);

	return len;
}
