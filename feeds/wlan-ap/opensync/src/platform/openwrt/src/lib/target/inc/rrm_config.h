/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RRM_CONFIG_H_INCLUDED
#define RRM_CONFIG_H_INCLUDED

#include <stdbool.h>
#include <jansson.h>
#include <ev.h>
#include <sys/time.h>
#include <syslog.h>

#include "log.h"
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

extern ovsdb_table_t table_Wifi_RRM_Config;

void rrm_config_vif(struct blob_buf *b, struct blob_buf *del, 
		const char * freq_band, const char * if_name);
int rrm_get_backup_channel(const char * freq_band);
bool rrm_config_txpower(const char *rname, unsigned int txpower);

void callback_Wifi_RRM_Config(ovsdb_update_monitor_t *mon,
		struct schema_Wifi_RRM_Config *old, struct schema_Wifi_RRM_Config *conf);

#endif /* RRM_CONFIG_H_INCLUDED */

