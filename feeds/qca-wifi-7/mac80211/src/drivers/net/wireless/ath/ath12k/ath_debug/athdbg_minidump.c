// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
#include <linux/minidump_tlv.h>
#include "athdbg_minidump.h"

#define minidump_crash_type (MINIDUMP_CRASH_TYPE_HOST | MINIDUMP_CRASH_TYPE_FW)
enum athdbg_minidump_status minidump_state = ENABLE_MINIDUMP;

const char *ath12k_dump_list[] = {
	"ath12k",
	"ath12k_base",
	"ath12k_dp",
	"ath12k_hw_group",
	"ath12k_hw",
	"ath12k_link_vif",
	"ath12k_ce_stats",
	"ieee80211_hw",
	"dp_rx_fst",
	"hal_rx_fst",
	"ath12k_reg_info",
	"ieee80211_regdomain",
};

struct list_head athdbg_minidump_list = LIST_HEAD_INIT(athdbg_minidump_list);

struct minidump_file_handler athdbg_debugfs_handlers[] = {
	{ "collect", 0644, ATHDBG_REQ_COLLECT_MINI_DUMP },
	{ "show_all_alloc_struct", 0644, ATHDBG_REQ_SHOW_ALL_STRUCT },
	{ "add_to_dump_list", 0644, ATHDBG_REQ_ADD_DUMP_LIST },
	{ "show_dump_list", 0644, ATHDBG_REQ_SHOW_DUMP_LIST },
	{ "disable_minidump", 0644, ATHDBG_REQ_DISABLE_MINIDUMP },
	{  NULL, 0, 0 },
};

struct athdbg_minidump_info *find_dump_node(const char *struct_name)
{
	struct athdbg_minidump_info *minidump_node;

	if (list_empty(&athdbg_minidump_list))
		return NULL;

	list_for_each_entry(minidump_node, &athdbg_minidump_list, dump_list) {
		if (minidump_node->struct_name) {
			if (strcmp(struct_name, minidump_node->struct_name) == 0)
				return minidump_node;
		}
	}
	return NULL;
}

void athdbg_iterate_minidump_list(void)
{
	struct athdbg_minidump_info *minidump_node;

	list_for_each_entry(minidump_node, &athdbg_minidump_list, dump_list) {
		if (minidump_node)
			athmem_find_and_print_minidump_entry(minidump_node->struct_name);
	}
}

void athdbg_create_minidump_struct_list(void)
{
	struct athdbg_minidump_info *minidump_node;
	size_t num_structs = sizeof(ath12k_dump_list) / sizeof(ath12k_dump_list[0]);
	int i;

	for (i = 0; i < num_structs; i++) {
		minidump_node = kzalloc(sizeof(*minidump_node), GFP_ATOMIC);
		if (minidump_node) {
			minidump_node->struct_name = ath12k_dump_list[i];

			INIT_LIST_HEAD(&minidump_node->dump_list);
			list_add_tail(&minidump_node->dump_list,
				      &athdbg_minidump_list);
		}
	}
}

void athdbg_clear_minidump_struct_list(void)
{
	struct athdbg_minidump_info *minidump_node, *tmp_node;

	if (list_empty(&athdbg_minidump_list))
		return;

	list_for_each_entry_safe(minidump_node, tmp_node, &athdbg_minidump_list,
				 dump_list) {
		list_del(&minidump_node->dump_list);
		kfree(minidump_node);
	}
}

static void athdbg_add_to_minidump_struct_list(const char *struct_name)
{
	struct athdbg_minidump_info *minidump_node;

	minidump_node = find_dump_node(struct_name);
	if (!minidump_node) {
		minidump_node = kzalloc(sizeof(*minidump_node), GFP_ATOMIC);
		if (minidump_node) {
			minidump_node->struct_name = struct_name;

			INIT_LIST_HEAD(&minidump_node->dump_list);
			list_add_tail(&minidump_node->dump_list,
				      &athdbg_minidump_list);
		}
	}
}

void athdbg_clear_minidump_info(void)
{
	athmem_clear_minidump_and_rb_tree();
	athdbg_clear_minidump_struct_list();
}
EXPORT_SYMBOL(athdbg_clear_minidump_info);

void athdbg_minidump_log(void *start_addr, size_t size, const char *struct_name,
			 const char *module_name)
{
	int ret = 0;

	if (struct_name && strlen(struct_name) > 0) {
		ath_minidump_update_alloc(start_addr, size, struct_name,
					  module_name);
		if (minidump_state == ENABLE_MINIDUMP &&
		    find_dump_node(struct_name)) {
			pr_debug("%s, addr %lx, sname %s, mname %s, size %zx\n",
				__func__, (const uintptr_t)start_addr,
				struct_name, module_name,
				size);

			athdbg_add_to_minidump_struct_list(struct_name);
			ret = minidump_add_segments((const uintptr_t)start_addr, size,
						   QCA_WDT_LOG_DUMP_TYPE_MOD,
						   struct_name,
						   minidump_crash_type,
						   module_name);
			if (ret)
				pr_err("ERR Cannot log minidump %d\n", ret);
		}
	}
}

void athdbg_collect_reference_segments(struct ath12k_base *ab)
{
	int i = 0, j = 0;
	struct ath12k_hw_group *ag;
	struct ath12k *ar = NULL;
	struct ath12k_hw *ah = NULL;
	struct ieee80211_hw *hw = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ieee80211_vif *vif;
	u32 vdev_bitmap, bit_pos;

	if (!ab || !ab->ag)
		return;

	ag = ab->ag;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		athdbg_minidump_log(ah, sizeof(struct ath12k_hw),
				    "ath12k_hw",
				    "ath12k");
		for (j = 0; j < ah->num_radio; j++) {
			ar = &ah->radio[j];
			if (!ar)
				continue;

			athdbg_minidump_log(ar, sizeof(struct ath12k),
					    "ath12k",
					    "ath12k");

			vdev_bitmap = ar->allocated_vdev_map;

			ab = ar->ab;
			if (!ab)
				continue;

			for (bit_pos = 0; bit_pos < 32; bit_pos++) {
				if (!(vdev_bitmap & BIT(bit_pos)))
					continue;
				if (athdbg_base && athdbg_base->dbg_to_ath_ops)
					arvif = athdbg_base->dbg_to_ath_ops->get_link_vif_from_vdev_id(ab, bit_pos);

				if (!arvif)
					continue;

				athdbg_minidump_log(arvif,
						    sizeof(struct ath12k_link_vif),
						    "ath12k_link_vif",
						    "ath12k");

				ahvif = arvif->ahvif;
				if (!ahvif)
					continue;

				athdbg_minidump_log(ahvif,
						    sizeof(struct ath12k_vif),
						    "ath12k_vif",
						    "ath12k");

				vif = ahvif->vif;
				if (!vif)
					continue;

				athdbg_minidump_log(vif,
						    sizeof(struct ieee80211_vif),
						    "ieee80211_vif",
						    "ath12k");
			}
			hw = ah->hw;
			if (!hw)
				continue;

			athdbg_minidump_log(hw, sizeof(struct ieee80211_hw),
					    "ieee80211_hw",
					    "ath12k");
		}
	}
}
EXPORT_SYMBOL(athdbg_collect_reference_segments);

void athdbg_free_reference_segments(struct ath12k_base *ab)
{
	int i = 0, j = 0;
	struct ath12k_hw_group *ag;
	struct ath12k *ar = NULL;
	struct ath12k_hw *ah = NULL;
	struct ieee80211_hw *hw = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ieee80211_vif *vif;
	u32 vdev_bitmap, bit_pos;
	const struct athdbg_to_ath12k_ops *ops;

	if (!ab || !ab->ag)
		return;

	ag = ab->ag;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		athmem_free_entry_in_minidump(ah);

		for (j = 0; j < ah->num_radio; j++) {
			ar = &ah->radio[j];
			if (!ar)
				continue;

			athmem_free_entry_in_minidump(ar);

			vdev_bitmap = ar->allocated_vdev_map;

			ab = ar->ab;
			if (!ab)
				continue;

			for (bit_pos = 0; bit_pos < 32; bit_pos++) {
				if (!(vdev_bitmap & BIT(bit_pos)))
					continue;
				if (athdbg_base &&
				   athdbg_base->dbg_to_ath_ops) {
					ops = athdbg_base->dbg_to_ath_ops;
					arvif = ops->get_link_vif_from_vdev_id(
						ab, bit_pos);
				}

				if (!arvif)
					continue;

				athmem_free_entry_in_minidump(arvif);

				ahvif = arvif->ahvif;
				if (!ahvif)
					continue;

				athmem_free_entry_in_minidump(ahvif);

				vif = ahvif->vif;
				if (!vif)
					continue;

				athmem_free_entry_in_minidump(vif);
			}
			hw = ah->hw;
			if (!hw)
				continue;

			athmem_free_entry_in_minidump(hw);
		}
	}
}
EXPORT_SYMBOL(athdbg_free_reference_segments);

void athdbg_do_dump_minidump(struct ath12k_base *ab)
{

	if (!ab || minidump_state != ENABLE_MINIDUMP)
		return;

	do_dump_minidump(minidump_crash_type);
	athdbg_free_reference_segments(ab);
}
EXPORT_SYMBOL(athdbg_do_dump_minidump);

void athdbg_collect_minidump(struct athdbg_request *dbg_req,
			     struct ath12k_base *ab)
{
	int val = 0;

	if (!dbg_req)
		goto exit;
	if (minidump_state != ENABLE_MINIDUMP)
		goto exit;
	if (kstrtou32(dbg_req->input_buf, 0, &val))
		goto exit;
	if (val) {
		athdbg_collect_reference_segments(ab);
		athdbg_do_dump_minidump(ab);
	}
exit:
	kfree(dbg_req->input_buf);
	return;
}
EXPORT_SYMBOL(athdbg_collect_minidump);

static void athdbg_show_all_minidump_struct(struct athdbg_request *dbg_req)
{
	int val = 0;

	if (!dbg_req)
		goto exit;
	if (minidump_state != ENABLE_MINIDUMP)
		goto exit;
	if (kstrtou32(dbg_req->input_buf, 0, &val))
		goto exit;
	if (val)
		athmem_print_all_allocated_list();

exit:
	kfree(dbg_req->input_buf);
	return;
}

static void athdbg_add_struct_to_minidump(struct athdbg_request *dbg_req)
{
	if (!dbg_req)
		goto exit;
	if (minidump_state != ENABLE_MINIDUMP)
		goto exit;

	if (strlen(dbg_req->input_buf) > 0)
		athmem_find_and_add_entry_in_minidump((const char *)dbg_req->input_buf);
	else
		pr_err("Invalid input\n");
exit:
	kfree(dbg_req->input_buf);
	return;
}

static void athdbg_show_minidump_entries(struct athdbg_request *dbg_req)
{
	int val = 0;

	if (!dbg_req)
		goto exit;
	if (minidump_state != ENABLE_MINIDUMP)
		goto exit;
	if (kstrtou32(dbg_req->input_buf, 0, &val))
		goto exit;

	if (val)
		athmem_print_minidump_list();

exit:
	kfree(dbg_req->input_buf);
	return;
}

void athdbg_remove_minidump_segment(void *start_addr)
{
	if (minidump_state == ENABLE_MINIDUMP)
		minidump_remove_segments((const uintptr_t)start_addr);
}

static void athdbg_disable_minidump(struct athdbg_request *dbg_req)
{
	int val = 0;

	if (!dbg_req)
		goto exit;
	if (kstrtou32(dbg_req->input_buf, 0, &val))
		goto exit;

	switch (val) {
	case DISABLE_MINIDUMP:
		minidump_state = DISABLE_MINIDUMP;
		athdbg_clear_minidump_info();
		break;
	case ENABLE_MINIDUMP:
		minidump_state = ENABLE_MINIDUMP;
		athdbg_create_minidump_struct_list();
		break;
	default:
		break;
	}
exit:
	kfree(dbg_req->input_buf);
	return;
}

enum athdbg_minidump_request get_minidump_req(const char *filename)
{
	int i = 0;

	while (athdbg_debugfs_handlers[i].filename != NULL) {
		if (strcmp(athdbg_debugfs_handlers[i].filename, filename) == 0)
			return athdbg_debugfs_handlers[i].minidump_req;
		i++;
	}
	return ATHDBG_REQ_INVALID;
}

void athdbg_add_to_minidump_log(void *start_addr, size_t size,
				const char *struct_name,
				const char *module_name)
{
	int ret = 0;

	if (minidump_state == ENABLE_MINIDUMP && strlen(struct_name) > 0) {
		pr_debug("%s, ptr addr %p, structure name %s, module name %s, size %zu\n",
			__func__, start_addr, struct_name, module_name, size);

		athdbg_add_to_minidump_struct_list(struct_name);
		ret = minidump_add_segments((const uintptr_t)start_addr, size,
					   QCA_WDT_LOG_DUMP_TYPE_MOD,
					   struct_name,
					   minidump_crash_type,
					   module_name);
		if (ret)
			pr_err("ERR Cannot log minidump %d\n", ret);
	}
}

void athdbg_process_minidump_request(struct ath12k_base *ab,
				     struct athdbg_request *dbg_req)
{
	if (!ab || !dbg_req)
		return;

	switch (dbg_req->data) {
	case ATHDBG_REQ_COLLECT_MINI_DUMP:
		athdbg_collect_minidump(dbg_req, ab);
		break;
	case ATHDBG_REQ_SHOW_ALL_STRUCT:
		athdbg_show_all_minidump_struct(dbg_req);
		break;
	case ATHDBG_REQ_ADD_DUMP_LIST:
		athdbg_add_struct_to_minidump(dbg_req);
		break;
	case ATHDBG_REQ_SHOW_DUMP_LIST:
		athdbg_show_minidump_entries(dbg_req);
		break;
	case ATHDBG_REQ_DISABLE_MINIDUMP:
		athdbg_disable_minidump(dbg_req);
		break;
	case ATHDBG_REQ_INVALID:
		pr_err("Unknown Request");
		break;
	}
}
#endif
