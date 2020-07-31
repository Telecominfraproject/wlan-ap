/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __NETIFD_H_
#define __NETIFD_H_

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "target.h"

#include <linux/if.h>
#include <libubox/blobmsg.h>
#include "utils.h"

#define SCHEMA_FIND_KEY(x, key)    __find_key(	 \
	(char *)x##_keys, sizeof(*(x ## _keys)), \
	(char *)x, sizeof(*(x)), x##_len, key)

static inline const char * __find_key(char *keyv, size_t keysz, char *datav, size_t datasz, int vlen, const char *key)
{
	int ii;

	for (ii = 0; ii < vlen; ii++) {
		if (strcmp(keyv, key) == 0)
			return datav;
		keyv += keysz;
		datav += datasz;
	}

	return NULL;
}

struct iface_info {
	char name[IFNAMSIZ];
	int vid;
};

extern int l3_device_split(char *l3_device, struct iface_info *info);

extern struct blob_buf b;
extern ovsdb_table_t table_Wifi_Inet_Config;
extern struct uci_context *uci;

extern void wifi_inet_config_init(void);
extern void wifi_inet_state_init(void);
extern void wifi_inet_state_set(struct blob_attr *msg);
extern void wifi_inet_master_set(struct blob_attr *msg);

extern int netifd_ubus_init(struct ev_loop *loop);

extern void dhcp_add(char *net, const char *lease_time, const char *start, const char *limit);
extern void dhcp_del(char *net);
extern void dhcp_get_state(struct schema_Wifi_Inet_State *state);
extern void dhcp_get_config(struct schema_Wifi_Inet_Config *conf);
extern void dhcp_lease(const char *method, struct blob_attr *msg);

extern void firewall_add_zone(char *net, int nat);
extern void firewall_del_zone(char *net);
extern void firewall_get_state(struct schema_Wifi_Inet_State *state);
extern void firewall_get_config(struct schema_Wifi_Inet_Config *conf);

#endif
