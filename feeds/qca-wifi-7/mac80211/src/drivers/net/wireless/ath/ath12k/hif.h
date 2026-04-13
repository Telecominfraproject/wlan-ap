/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_HIF_H
#define ATH12K_HIF_H

#include "core.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "pci.h"
#endif

struct ath12k_hif_ops {
	u32 (*read32)(struct ath12k_base *ab, u32 address);
	void (*write32)(struct ath12k_base *ab, u32 address, u32 data);
	u32 (*cmem_read32)(struct ath12k_base *sc, u32 address);
	void (*cmem_write32)(struct ath12k_base *sc, u32 address, u32 data);
	u32 (*pmm_read32)(struct ath12k_base *sc, u32 address);
	void (*irq_enable)(struct ath12k_base *ab);
	void (*irq_disable)(struct ath12k_base *ab);
	int (*start)(struct ath12k_base *ab);
	void (*stop)(struct ath12k_base *ab);
	int (*power_up)(struct ath12k_base *ab);
	void (*power_down)(struct ath12k_base *ab, bool is_suspend);
	int (*suspend)(struct ath12k_base *ab);
	int (*resume)(struct ath12k_base *ab);
	int (*map_service_to_pipe)(struct ath12k_base *ab, u16 service_id,
				   u8 *ul_pipe, u8 *dl_pipe);
	int (*get_user_msi_vector)(struct ath12k_base *ab, char *user_name,
				   int *num_vectors, u32 *user_base_data,
				   u32 *base_vector);
	void (*get_msi_address)(struct ath12k_base *ab, u32 *msi_addr_lo,
				u32 *msi_addr_hi);
	void (*ce_irq_enable)(struct ath12k_base *ab);
	void (*ce_irq_disable)(struct ath12k_base *ab);
	void (*get_ce_msi_idx)(struct ath12k_base *ab, u32 ce_id, u32 *msi_idx);
	int (*panic_handler)(struct ath12k_base *ab);
	void (*coredump_download)(struct ath12k_base *ab);
	int (*ext_irq_setup)(struct ath12k_base *ab,
			     int (*handler)(struct ath12k_dp *dp,
					    struct ath12k_ext_irq_grp *irq_grp,
					    int budget),
			     struct ath12k_dp *dp);
	void (*ext_irq_cleanup)(struct ath12k_base *ab);
	int (*set_qrtr_endpoint_id)(struct ath12k_base *ab);
	void (*config_static_window)(struct ath12k_base *ab);
	int (*get_msi_irq)(struct ath12k_base *ab, unsigned int vector);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	int (*ppeds_register_interrupts)(struct ath12k_base *ab, int type, int vector,
					 int ring_num);
	void (*ppeds_free_interrupts)(struct ath12k_base *ab);
	void (*ppeds_irq_enable)(struct ath12k_base *ab, enum ppeds_irq_type type);
	void (*ppeds_irq_disable)(struct ath12k_base *ab, enum ppeds_irq_type type);
#endif
	void (*dp_umac_intr_line_reset)(struct ath12k_base *ab);
	int (*dp_umac_reset_irq_config)(struct ath12k_base *ab);
	void (*dp_umac_reset_enable_irq)(struct ath12k_base *ab);
	void (*dp_umac_reset_free_irq)(struct ath12k_base *ab);
};

static inline int ath12k_hif_map_service_to_pipe(struct ath12k_base *ab, u16 service_id,
						 u8 *ul_pipe, u8 *dl_pipe)
{
	return ab->hif.ops->map_service_to_pipe(ab, service_id,
						ul_pipe, dl_pipe);
}

static inline int ath12k_hif_get_user_msi_vector(struct ath12k_base *ab,
						 char *user_name,
						 int *num_vectors,
						 u32 *user_base_data,
						 u32 *base_vector)
{
	if (!ab->hif.ops->get_user_msi_vector)
		return -EOPNOTSUPP;

	return ab->hif.ops->get_user_msi_vector(ab, user_name, num_vectors,
						user_base_data,
						base_vector);
}

static inline void ath12k_hif_get_msi_address(struct ath12k_base *ab,
					      u32 *msi_addr_lo,
					      u32 *msi_addr_hi)
{
	if (!ab->hif.ops->get_msi_address)
		return;

	ab->hif.ops->get_msi_address(ab, msi_addr_lo, msi_addr_hi);
}

static inline void ath12k_hif_get_ce_msi_idx(struct ath12k_base *ab, u32 ce_id,
					     u32 *msi_data_idx)
{
	if (ab->hif.ops->get_ce_msi_idx)
		ab->hif.ops->get_ce_msi_idx(ab, ce_id, msi_data_idx);
	else
		*msi_data_idx = ce_id;
}

static inline void ath12k_hif_ce_irq_enable(struct ath12k_base *ab)
{
	if (ab->hif.ops->ce_irq_enable)
		ab->hif.ops->ce_irq_enable(ab);
}

static inline void ath12k_hif_ce_irq_disable(struct ath12k_base *ab)
{
	if (ab->hif.ops->ce_irq_disable)
		ab->hif.ops->ce_irq_disable(ab);
}

static inline void ath12k_hif_irq_enable(struct ath12k_base *ab)
{
	ab->hif.ops->irq_enable(ab);
}

static inline void ath12k_hif_irq_disable(struct ath12k_base *ab)
{
	ab->hif.ops->irq_disable(ab);
}

static inline int ath12k_hif_suspend(struct ath12k_base *ab)
{
	if (ab->hif.ops->suspend)
		return ab->hif.ops->suspend(ab);

	return 0;
}

static inline int ath12k_hif_resume(struct ath12k_base *ab)
{
	if (ab->hif.ops->resume)
		return ab->hif.ops->resume(ab);

	return 0;
}

static inline int ath12k_hif_start(struct ath12k_base *ab)
{
	return ab->hif.ops->start(ab);
}

static inline void ath12k_hif_stop(struct ath12k_base *ab)
{
	ab->hif.ops->stop(ab);
}

static inline u32 ath12k_hif_read32(struct ath12k_base *ab, u32 address)
{
	return ab->hif.ops->read32(ab, address);
}

static inline u32 ath12k_hif_pmm_read32(struct ath12k_base *ab, u32 offset)
{
	return ab->hif.ops->pmm_read32(ab, offset);
}

static inline u32 ath12k_hif_cmem_read32(struct ath12k_base *ab, u32 address)
{
	return ab->hif.ops->cmem_read32(ab, address);
}

static inline void ath12k_hif_cmem_write32(struct ath12k_base *ab, u32 address,
                                      u32 data)
{
        ab->hif.ops->cmem_write32(ab, address, data);
}

static inline void ath12k_hif_write32(struct ath12k_base *ab, u32 address,
				      u32 data)
{
	ab->hif.ops->write32(ab, address, data);
}

static inline int ath12k_hif_power_up(struct ath12k_base *ab)
{
	if (!ab->hif.ops->power_up)
		return -EOPNOTSUPP;

	return ab->hif.ops->power_up(ab);
}

static inline void ath12k_hif_power_down(struct ath12k_base *ab, bool is_suspend)
{
	if (!ab->hif.ops->power_down)
		return;

	ab->hif.ops->power_down(ab, is_suspend);
}

static inline int ath12k_hif_panic_handler(struct ath12k_base *ab)
{
	if (!ab->hif.ops->panic_handler)
		return NOTIFY_DONE;

	return ab->hif.ops->panic_handler(ab);
}

static inline void ath12k_hif_coredump_download(struct ath12k_base *ab)
{
	if (ab->hif.ops->coredump_download)
		ab->hif.ops->coredump_download(ab);
}

static inline int ath12k_hif_set_qrtr_endpoint_id(struct ath12k_base *ab)
{
	if (!ab->hif.ops->set_qrtr_endpoint_id)
		return -EOPNOTSUPP;
	else
		return ab->hif.ops->set_qrtr_endpoint_id(ab);
}

static inline void ath12k_hif_config_static_window(struct ath12k_base *ab)
{
	if (!ab->hif.ops->config_static_window)
		return;

	ab->hif.ops->config_static_window(ab);
}

static inline int ath12k_hif_get_msi_irq(struct ath12k_base *ab, unsigned int vector)
{
	if (!ab->hif.ops->get_msi_irq)
		return -EOPNOTSUPP;

	return ab->hif.ops->get_msi_irq(ab, vector);
}

static inline
int ath12k_hif_ext_irq_setup(struct ath12k_base *ab,
			     int (*irq_handler)(struct ath12k_dp *dp,
						struct ath12k_ext_irq_grp *irq_grp,
						int budget),
			     struct ath12k_dp *dp)
{
	if (!ab->hif.ops->ext_irq_setup)
		return -EOPNOTSUPP;

	return ab->hif.ops->ext_irq_setup(ab, irq_handler, dp);
}

static inline void ath12k_hif_ext_irq_cleanup(struct ath12k_base *ab)
{
	if (!ab->hif.ops->ext_irq_cleanup)
		return;

	ab->hif.ops->ext_irq_cleanup(ab);
}

static inline void ath12k_hif_dp_umac_intr_line_reset(struct ath12k_base *ab)
{
        if (ab->hif.ops->dp_umac_intr_line_reset)
                ab->hif.ops->dp_umac_intr_line_reset(ab);
}

static inline int ath12k_hif_dp_umac_reset_irq_config(struct ath12k_base *ab)
{
        if (ab->hif.ops->dp_umac_reset_irq_config)
                return ab->hif.ops->dp_umac_reset_irq_config(ab);

        return 0;
}

static inline void ath12k_hif_dp_umac_reset_enable_irq(struct ath12k_base *ab)
{
        if (ab->hif.ops->dp_umac_reset_enable_irq)
                return ab->hif.ops->dp_umac_reset_enable_irq(ab);

        return;
}

static inline void ath12k_hif_dp_umac_reset_free_irq(struct ath12k_base *ab)
{
        if (ab->hif.ops->dp_umac_reset_free_irq)
                return ab->hif.ops->dp_umac_reset_free_irq(ab);

        return;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static inline int ath12k_hif_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
						       int ring_num)
{
	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (ab->hif.ops->ppeds_register_interrupts)
		return ab->hif.ops->ppeds_register_interrupts(ab, type, vector,
							      ring_num);
	return 0;
}

static inline void ath12k_hif_ppeds_free_interrupts(struct ath12k_base *ab)
{
	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ab->hif.ops->ppeds_register_interrupts)
		ab->hif.ops->ppeds_free_interrupts(ab);
}

static inline void ath12k_hif_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ab->hif.ops->ppeds_irq_enable)
		ab->hif.ops->ppeds_irq_enable(ab, type);
}

static inline void ath12k_hif_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ab->hif.ops->ppeds_irq_disable)
		ab->hif.ops->ppeds_irq_disable(ab, type);
}
#else
static inline int ath12k_hif_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
						       int ring_num)
{
	return 0;
}

static inline void ath12k_hif_ppeds_free_interrupts(struct ath12k_base *ab)
{
}

static inline void ath12k_hif_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
}

static inline void ath12k_hif_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
}
#endif
#endif /* ATH12K_HIF_H */
