#ifndef __COMMAND_H_
#define __COMMAND_H_

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

#include "utils.h"

extern void task_init(void);
extern int ubus_get_l3_device(const char *net, char *ifname);
extern int command_ubus_init(struct ev_loop *loop);

#endif
