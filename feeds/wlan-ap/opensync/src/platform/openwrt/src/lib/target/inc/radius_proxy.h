/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RADIUS_PROXY_H_INCLUDED
#define RADIUS_PROXY_H_INCLUDED

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

extern ovsdb_table_t table_Radius_Proxy_Config;

void callback_Radius_Proxy_Config(ovsdb_update_monitor_t *mon,
		struct schema_Radius_Proxy_Config *old, struct schema_Radius_Proxy_Config *conf);
void radius_proxy_fixup(void);

#endif /* RADIUS_PROXY_H_INCLUDED */

