/* SPDX-License-Identifier: BSD-3-Clause */

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
#include <libubox/list.h>
#include <evsched.h>

enum {
	TASK_WAITING,
	TASK_PENDING,
	TASK_RUNNING,
	TASK_COMPLETE,
	TASK_FAILED,
};

struct cmd_handler;
struct task {
	struct cmd_handler *handler;
	struct schema_Command_Config conf;
	struct list_head list;
	pid_t pid;
	ev_child child;
	const char *arg;
};

extern char serial[64];
extern time_t crash_timestamp;
extern pid_t cmd_handler_tcpdump(struct task *task);
extern pid_t cmd_handler_tcpdump_wifi(struct task *task);
extern void task_status(struct task *task, int status, char *result);
extern void task_init(void);
extern int ubus_get_l3_device(const char *net, char *ifname);
extern int command_ubus_init(struct ev_loop *loop);
extern void node_config_init(void);
extern void webserver_init(void);
extern void crashlog_init(void);
extern pid_t cmd_handler_crashlog(struct task *task);
extern pid_t cmd_handler_port_forwarding(struct task *task);
extern int port_forwarding(char *ipAddress, char *port);

#endif
