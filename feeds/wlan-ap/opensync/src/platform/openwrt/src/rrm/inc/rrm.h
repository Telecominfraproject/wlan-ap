/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RRM_H_INCLUDED
#define RRM_H_INCLUDED

#include <stdbool.h>
#include <jansson.h>
#include <ev.h>
#include <sys/time.h>
#include <syslog.h>

#include "log.h"

#include "ds.h"
#include "ds_tree.h"

#include "os_nif.h"

#include "target.h"
#include "dppline.h"

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "target.h"

#include "utils.h"
#include <libubox/list.h>
#include <evsched.h>


typedef struct
{
	struct schema_Wifi_Radio_State  schema;
	bool                            init;
	radio_entry_t                   config;
	ds_tree_node_t                  node;
} rrm_radio_state_t;

typedef struct
{
	struct schema_Wifi_VIF_State    schema;
	ds_tree_node_t                  node;
} rrm_vif_state_t;

typedef struct
{
	// Cached data
	radio_type_t freq_band;
	uint32_t backup_channel;
	uint32_t cell_size;
	int32_t probe_resp_threshold;
	int32_t client_disconnect_threshold;
	uint32_t snr_percentage_drop;
	uint32_t min_load;
	uint32_t basic_rate;

	// Internal state data
	int32_t noise_lwm;
} rrm_entry_t;

typedef struct
{
	struct schema_Wifi_RRM_Config   schema;
	rrm_entry_t                     rrm_data;
	ds_tree_node_t                  node;
} rrm_config_t;

int rrm_setup_monitor(void);
void rrm_channel_init(void);
int rrm_ubus_init(struct ev_loop *loop);
int ubus_get_noise(const char *if_name, uint32_t *noise);
int ubus_set_channel_switch(const char *if_name, uint32_t frequency);
void set_rrm_parameters(rrm_entry_t *rrm_data);
ds_tree_t* rrm_get_rrm_config_list(void);
ds_tree_t* rrm_get_radio_list(void);
ds_tree_t* rrm_get_vif_list(void);

#endif /* RRM_H_INCLUDED */
