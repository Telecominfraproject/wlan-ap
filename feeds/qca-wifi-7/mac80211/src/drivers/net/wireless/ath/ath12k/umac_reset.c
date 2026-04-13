/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "core.h"
#include "coredump.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "hif.h"
#include "dp.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif

int ath12k_htt_umac_reset_msg_send(struct ath12k_base *ab,
				   struct ath12k_htt_umac_reset_setup_cmd_params *params)
{
	struct sk_buff *skb;
	struct htt_dp_umac_reset_setup_req_cmd *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct htt_dp_umac_reset_setup_req_cmd *)skb->data;
	cmd->msg_info = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP,
					HTT_H2T_MSG_TYPE_SET);
	cmd->msg_info |= u32_encode_bits(0, HTT_H2T_MSG_METHOD);
	cmd->msg_info |= u32_encode_bits(0, HTT_T2H_MSG_METHOD);
	cmd->msi_data = params->msi_data;
	cmd->msg_shared_mem.size = sizeof(struct htt_h2t_paddr_size);
	cmd->msg_shared_mem.addr_lo = params->addr_lo;
	cmd->msg_shared_mem.addr_hi = params->addr_hi;


	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);

	if (ret) {
		ath12k_warn(ab, "DP UMAC INIT msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "DP UMAC INIT msg sent from host\n");
	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_htt_umac_reset_send_start_pre_reset_cmd(struct ath12k_base *ab, int is_initiator,
						   int is_target_recovery)
{
	struct sk_buff *skb;
	struct h2t_umac_hang_recovery_start_pre_reset *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct h2t_umac_hang_recovery_start_pre_reset*)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET,
				   HTT_H2T_UMAC_RESET_MSG_TYPE);
	cmd->hdr |= u32_encode_bits(is_initiator, HTT_H2T_UMAC_RESET_IS_INITIATOR_SET);
	cmd->hdr |= u32_encode_bits(!is_target_recovery, HTT_H2T_UMAC_RESET_IS_TARGET_RECOVERY_SET);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
                ath12k_warn(ab, "failed to send htt umac reset pre reset start: %d\n",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_get_umac_reset_intr_offset(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_NUM_MAX; i++) {
		if (ab->hw_params->ring_mask->umac_dp_reset[i])
			return i;
	}
	return 0;
}

int ath12k_htt_umac_reset_setup_cmd(struct ath12k_base *ab)
{
	int msi_data_count;
	struct ath12k_htt_umac_reset_setup_cmd_params params = {};
	u32 msi_data_start, msi_irq_start;
	int ret, intr_ctxt;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

	intr_ctxt = ath12k_get_umac_reset_intr_offset(ab);

	if (!ab->hw_params->umac_reset_ipc) {
		ret = ath12k_hif_get_user_msi_vector(ab, "DP",
						     &msi_data_count, &msi_data_start,
						     &msi_irq_start);
		if (ret) {
			ath12k_err(ab, "Failed to fill msi_data\n");
			return ret;
		}

		params.msi_data = (intr_ctxt % msi_data_count) + msi_data_start;
	} else {
		params.msi_data = ab->hw_params->umac_reset_ipc;
	}

	params.addr_lo = umac_reset->shmem_paddr_aligned & HAL_ADDR_LSB_REG_MASK;
	params.addr_hi = (u64)umac_reset->shmem_paddr_aligned >> HAL_ADDR_MSB_REG_SHIFT;

	return ath12k_htt_umac_reset_msg_send(ab, &params);
}

int ath12k_dp_umac_reset_init(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;
	int alloc_size, ret;

	if (!ab->hw_params->support_umac_reset)
		return 0;

	umac_reset = &ab->dp_umac_reset;
	umac_reset->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;

	alloc_size = sizeof(struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t) +
			    ATH12K_DP_UMAC_RESET_SHMEM_ALIGN - 1;

	umac_reset->shmem_vaddr_unaligned = dma_alloc_coherent(ab->dev,
                                                     alloc_size,
                                                     &umac_reset->shmem_paddr_unaligned,
                                                     GFP_KERNEL);
	if (!umac_reset->shmem_vaddr_unaligned) {
		ath12k_warn(ab, "Failed to allocate memory with size:%u\n", alloc_size);
		return -ENOMEM;
	}

	umac_reset->shmem_vaddr_aligned =
		PTR_ALIGN(umac_reset->shmem_vaddr_unaligned, ATH12K_DP_UMAC_RESET_SHMEM_ALIGN);
	umac_reset->shmem_paddr_aligned =
		umac_reset->shmem_paddr_unaligned + ((unsigned long)umac_reset->shmem_vaddr_aligned -
				(unsigned long)umac_reset->shmem_vaddr_unaligned);
	umac_reset->shmem_size = alloc_size;
	umac_reset->shmem_vaddr_aligned->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;
	umac_reset->intr_offset = ath12k_get_umac_reset_intr_offset(ab);
	memset(&umac_reset->ts, 0, sizeof(struct ath12k_umac_reset_ts));

	ret = ath12k_hif_dp_umac_reset_irq_config(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to register interrupt for UMAC RECOVERY\n");
		goto shmem_free;
	}

	ret = ath12k_htt_umac_reset_setup_cmd(ab);
	if (ret) {
		ath12k_warn(ab, "Unable to setup UMAC RECOVERY\n");
		goto free_irq;
	}

	ath12k_hif_dp_umac_reset_enable_irq(ab);
	return 0;

free_irq:
	ath12k_hif_dp_umac_reset_free_irq(ab);
shmem_free:
	dma_free_coherent(ab->dev,
			  umac_reset->shmem_size,
			  umac_reset->shmem_vaddr_unaligned,
			  umac_reset->shmem_paddr_unaligned);
	umac_reset->shmem_vaddr_unaligned = NULL;
	return ret;
}

void ath12k_umac_reset_pre_reset_validation(struct ath12k_base *ab, bool *is_initiator,
					    bool *is_target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	*is_initiator = *is_target_recovery = false;

	if (!ag)
		return;

	mlo_umac_reset = &ag->mlo_umac_reset;

	spin_lock_bh(&mlo_umac_reset->lock);
	if (mlo_umac_reset->initiator_chip == ab->device_id)
		*is_initiator = true;
	if (mlo_umac_reset->umac_reset_info & BIT(1))
		*is_target_recovery = true;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

void ath12k_umac_reset_completion(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (!mlo_umac_reset)
		return;

	if (!(mlo_umac_reset->umac_reset_info & BIT(0)))
		return;

	spin_lock_bh(&mlo_umac_reset->lock);
	mlo_umac_reset->umac_reset_info = 0;
	mlo_umac_reset->initiator_chip = 0;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

void ath12k_umac_reset_send_htt(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr_aligned;
	bool is_initiator, is_target_recovery;
	int ret;

	shmem_vaddr_aligned = umac_reset->shmem_vaddr_aligned;

	switch(tx_event) {
	case ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE:
		ath12k_umac_reset_pre_reset_validation(ab, &is_initiator,
						       &is_target_recovery);
		ret = ath12k_htt_umac_reset_send_start_pre_reset_cmd(ab,
								     is_initiator,
								     is_target_recovery);
		ab->dp_umac_reset.ts.trigger_done = jiffies_to_msecs(jiffies);
		if (ret)
			ath12k_warn(ab, "Unable to send umac trigger\n");
		break;
	case ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_PRE_RESET_DONE_SET);
		ab->dp_umac_reset.ts.pre_reset_done = jiffies_to_msecs(jiffies);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_START_DONE_SET);
		ab->dp_umac_reset.ts.post_reset_done = jiffies_to_msecs(jiffies);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
				ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_COMPLETE_DONE);
		ab->dp_umac_reset.ts.post_reset_complete_done = jiffies_to_msecs(jiffies);
		break;
        }

	if (tx_event == ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE) {
		ath12k_umac_reset_completion(ab);
		ath12k_info(ab, "MLO UMAC Recovery completed\n");
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Time taken for trigger_start:%llums "
			   "trigger_done: %llums pre_reset:%llums post_reset:%llums "
			   "post_reset_complete:%llums",
			   ab->dp_umac_reset.ts.trigger_start, ab->dp_umac_reset.ts.trigger_done,
			   ab->dp_umac_reset.ts.pre_reset_done - ab->dp_umac_reset.ts.pre_reset_start,
			   ab->dp_umac_reset.ts.post_reset_done - ab->dp_umac_reset.ts.post_reset_start,
			   ab->dp_umac_reset.ts.post_reset_complete_done - ab->dp_umac_reset.ts.post_reset_complete_start);
		memset(&umac_reset->ts, 0, sizeof(struct ath12k_umac_reset_ts));
	}

	return;
}

int ath12k_umac_reset_notify_target(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_base *partner_ab;
	struct ath12k_hw_group *ag = ab->ag;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags))
			continue;

		ath12k_umac_reset_send_htt(partner_ab, tx_event);
	}

	return 0;
}

int ath12k_umac_reset_initiate_recovery(struct ath12k_base *ab,
					bool target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (!mlo_umac_reset)
		return -EOPNOTSUPP;

	spin_lock_bh(&mlo_umac_reset->lock);

	if (mlo_umac_reset->umac_reset_info &
	    ATH12K_IS_UMAC_RESET_IN_PROGRESS) {
		spin_unlock_bh(&mlo_umac_reset->lock);
		ath12k_warn(ab, "UMAC RECOVERY IS IN PROGRESS\n");
		WARN_ON(1);
		return -ECANCELED;
	}
	mlo_umac_reset->umac_reset_info = BIT(0); /* UMAC recovery is in progress */
	if (target_recovery)
		mlo_umac_reset->umac_reset_info |= BIT(1); /* Target recovery */
	atomic_set(&mlo_umac_reset->response_chip, 0);
	mlo_umac_reset->initiator_chip = ab->device_id;
	spin_unlock_bh(&mlo_umac_reset->lock);
	return 0;
}

void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
						   enum dp_umac_reset_tx_cmd tx_event)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (atomic_read(&mlo_umac_reset->response_chip) >= ab->ag->num_started) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "response chip:%d num_started:%d sending notify\n",
			   atomic_read(&mlo_umac_reset->response_chip), ab->ag->num_started);
		ath12k_umac_reset_notify_target(ab, tx_event);
		atomic_set(&mlo_umac_reset->response_chip, 0);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "response_chip:%d num_started:%d not matching.. hold on notify\n",
			   atomic_read(&mlo_umac_reset->response_chip), ab->ag->num_started);
	}
	return;
}

void ath12k_umac_reset_notify_pre_reset_done(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;

	dp = ath12k_ab_to_dp(ab);

	if (dp->service_rings_running)
		return;

	ath12k_umac_reset_notify_target_sync_and_send(ab,
						      ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE);
	ab->dp_umac_reset.umac_pre_reset_in_prog = false;
}
EXPORT_SYMBOL(ath12k_umac_reset_notify_pre_reset_done);

void ath12k_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	set_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags);
	ath12k_hif_irq_disable(ab);
	atomic_inc(&mlo_umac_reset->response_chip);
	ab->dp_umac_reset.umac_pre_reset_in_prog = true;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ath12k_dp_ppeds_service_enable_disable(ab, true);
		ath12k_dp_ppeds_interrupt_stop(ab);
		ath12k_dp_ppeds_stop(ab);
		ath12k_dp_ppeds_service_enable_disable(ab, false);
	}
#endif
	ath12k_umac_reset_notify_pre_reset_done(ab);

 /*
  * Memset the wbm link desc pool to 0 at this point, so that by the time
  * FW responds with post_reset_start, we would have finished the memset.
  * This will save a few milliseconds.
  */

	ath12k_dp_clear_link_desc_pool(dp);
	return;
}

void ath12k_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	dp->service_rings_running = 0;
	atomic_inc(&mlo_umac_reset->response_chip);
	ath12k_hif_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ath12k_dp_ppeds_start(ab);
		ath12k_dp_ppeds_interrupt_start(ab);
	}
#endif
	clear_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags);
	ath12k_umac_reset_notify_target_sync_and_send(ab, ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE);
	return;
}

void ath12k_dp_umac_reset_action(struct ath12k_base *ab,
				 enum dp_umac_reset_recover_action rx_event)
{
	int ret;
	bool target_recovery = false;

	switch(rx_event) {
	case ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC:
		target_recovery = true;
		fallthrough;
	case ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY:
		if (!target_recovery && ab->is_reset)
			return;

		ret = ath12k_umac_reset_initiate_recovery(ab, target_recovery);
		if (!ret) {
			ab->dp_umac_reset.ts.trigger_start = jiffies_to_msecs(jiffies);
			ath12k_umac_reset_notify_target(ab, ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE);
		}
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE:
		ab->dp_umac_reset.ts.post_reset_complete_start = jiffies_to_msecs(jiffies);
		ath12k_umac_reset_handle_post_reset_complete(ab);
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_START:
		ab->dp_umac_reset.ts.post_reset_start = jiffies_to_msecs(jiffies);
		ath12k_umac_reset_handle_post_reset_start(ab);
		break;
	case ATH12K_UMAC_RESET_DO_PRE_RESET:
		ab->dp_umac_reset.ts.pre_reset_start = jiffies_to_msecs(jiffies);
		ath12k_umac_reset_handle_pre_reset(ab);
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Unknown UMAC RESET event received\n");
		break;
	}
	return;
}

irqreturn_t ath12k_umac_reset_interrupt_handler(int irq, void *arg)
{
	struct ath12k_base *ab = arg;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

	disable_irq_nosync(umac_reset->irq_num);
	tasklet_schedule(&umac_reset->intr_tq);
	return IRQ_HANDLED;
}

void ath12k_dp_umac_reset_handle(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr;
	int rx_event, num_event = 0;
	u32 t2h_msg;

	shmem_vaddr = umac_reset->shmem_vaddr_aligned;
	if (!shmem_vaddr) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address NULL\n");
		return;
	}

	if (shmem_vaddr->magic_num != umac_reset->magic_num) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address is invalid shmem:0x%x u:0x%x\n",
			   shmem_vaddr->magic_num, umac_reset->magic_num);
		return;
	}

	t2h_msg = shmem_vaddr->t2h_msg;
	shmem_vaddr->t2h_msg = 0;

	rx_event = ATH12K_UMAC_RESET_RX_EVENT_NONE;

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET)) {
		rx_event |= ATH12K_UMAC_RESET_DO_PRE_RESET;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_START;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE;
		num_event++;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Deduced rx event:%d num:%d\n", rx_event, num_event);

	if (num_event > 1) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Multiple event notified in single msg\n");
		WARN_ON_ONCE(1);
		return;
	}

	ath12k_dp_umac_reset_action(ab, rx_event);
	return;
}

void ath12k_umac_reset_tasklet_handler(struct tasklet_struct *umac_cntxt)
{
	struct ath12k_dp_umac_reset *umac_reset = from_tasklet(umac_reset, umac_cntxt, intr_tq);
	struct ath12k_base *ab = container_of(umac_reset, struct ath12k_base, dp_umac_reset);

	ath12k_hif_dp_umac_intr_line_reset(ab);
	ath12k_dp_umac_reset_handle(ab);
	enable_irq(umac_reset->irq_num);
}

void ath12k_dp_umac_reset_deinit(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;

	if (!ab->hw_params->support_umac_reset)
		return;

	umac_reset = &ab->dp_umac_reset;

	if (!umac_reset)
		return;

	ath12k_hif_dp_umac_reset_free_irq(ab);

	if (umac_reset->shmem_vaddr_unaligned) {
		dma_free_coherent(ab->dev,
				  umac_reset->shmem_size,
				  umac_reset->shmem_vaddr_unaligned,
				  umac_reset->shmem_paddr_unaligned);
		umac_reset->shmem_vaddr_unaligned = NULL;

	}
}
