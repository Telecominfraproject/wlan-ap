/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __TELEMETRY_AGENT_H__
#define __TELEMETRY_AGENT_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/relay.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include "telemetry_agent_wifi_driver_if.h"
#include "telemetry_agent_app_if.h"

#define STATUS_SUCCESS 0
#define STATUS_FAIL -1
/* Maximum buffer size that is requied to transfer stats between
agent and APP */
#define EMESH_MAX_SUB_BUFFERS 10
#define MAX_SUB_BUFFERS 10
#define STATS_FREQUECY  1000 /*in milli seconds */

#define MAX_PDEV_LINKS_DB 3
#define MAX_SOCS_DB 5

#define MAC_ADDR_EQ(a1, a2)   (memcmp(a1, a2, 6) == 0)

#define TA_PRINT_DEBUG 0x1
#define TA_PRINT_INFO  0x2
#define TA_PRINT_ERROR 0x4
#define TA_PRINT_MASK(mask)  (g_agent_obj.debug_mask & mask)

#define ta_print_info(args...) do { \
    if (TA_PRINT_MASK(TA_PRINT_INFO)) {  \
        printk(args);    \
    }                    \
} while (0)

#define ta_print_debug(args...) do { \
    if (TA_PRINT_MASK(TA_PRINT_DEBUG)) {  \
        printk(args);    \
    }                    \
} while (0)

#define ta_print_error(args...) do { \
    if (TA_PRINT_MASK(TA_PRINT_ERROR)) {  \
        printk(args);    \
    }                    \
} while (0)

struct agent_peer_db {
	struct list_head node;
	void *peer_obj_ptr;
	void *pdev_obj_ptr;
	void *psoc_obj_ptr;
	uint8_t peer_mac_addr[6];
	uint32_t tx_mpdu_retried;
	uint32_t tx_mpdu_total;
	uint32_t rx_mpdu_retried;
	uint32_t rx_mpdu_total;
	uint8_t chan_bw;
	uint16_t eff_chan_bw;
	uint32_t tx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	uint32_t rx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
};

struct agent_pdev_db {
	void *pdev_obj_ptr;
	void *psoc_obj_ptr;
	uint8_t pdev_id;
	uint32_t tx_mpdu_failed[WLAN_AC_MAX];
	uint32_t tx_mpdu_total[WLAN_AC_MAX];
	int num_peers;
	spinlock_t peer_db_lock;
	struct list_head peer_db_list;
};

struct agent_soc_db {
	void *psoc_obj_ptr;
	uint8_t soc_id;
	uint8_t num_pdevs;
	struct agent_pdev_db pdev_db[MAX_PDEV_LINKS_DB];
};

struct agent_telemtry_db {
	uint8_t num_socs;
	struct agent_soc_db psoc_db[MAX_SOCS_DB];
};

struct telemetry_agent_object {
	/* RelayFS */
	uint32_t num_subbufs;
	uint32_t subbuf_size;
	/* relay(fs) channel */
	struct rchan *rfs_channel;
	/* relay(fs) telemetry channel */
	struct rchan *rfs_emesh_channel;
	struct dentry *dir_ptr;
	struct dentry *dir_emesh_ptr;
	/* delyed work */
	struct delayed_work stats_work_init;
	/* delyed work periodic*/
	struct delayed_work stats_work_periodic;
	/* delayed telemetry work periodic*/
	struct delayed_work emesh_stats_work_periodic;
	/* pdev and peer objects */
	struct agent_telemtry_db agent_db;
	uint32_t debug_mask;
};


extern int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);
extern int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);
void telemetry_agent_stats_work(struct work_struct *work);
int telemetry_agent_register_umac_ops(void);
int telemetry_agent_init_relayfs(struct telemetry_agent_object *agent_obj);
#endif /* __TELEMETRY_AGENT_H__ */
