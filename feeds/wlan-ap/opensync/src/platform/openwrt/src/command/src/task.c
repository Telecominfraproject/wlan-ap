/* SPDX-License-Identifier: BSD-3-Clause */

#include <net/if.h>

#include "command.h"

static struct list_head tasks = LIST_HEAD_INIT(tasks);
static ovsdb_table_t table_Command_Config;
static ovsdb_table_t table_Command_State;
static int task_running;
char serial[64];

static struct cmd_handler {
	char *cmd;
	int (*cb)(struct task *task);
} cmd_handler[] = {
	{
		.cmd = "tcpdump",
		.cb = cmd_handler_tcpdump,
	}, {
		.cmd = "tcpdump-wifi",
		.cb = cmd_handler_tcpdump_wifi,
	}, {
		.cmd = "crashlog",
		.cb = cmd_handler_crashlog,
	}, {
		.cmd = "startPortForwardingSession",
		.cb = cmd_handler_port_forwarding,
	},
};

static void task_next(void);

bool ovsdb_table_upsert_simple_typed_f(ovsdb_table_t *table,
				       char *column, void *value, ovsdb_col_t col_type,
				       void *record, bool update_uuid, char *filter[])
{
	return ovsdb_table_upsert_where_f(table,
					  ovsdb_where_simple_typed(column, value, col_type),
					  record, update_uuid, filter);
}

bool ovsdb_table_upsert_simple_typed(ovsdb_table_t *table,
			       char *column, void *value, ovsdb_col_t col_type,
			       void *record, bool update_uuid)
{
	return ovsdb_table_upsert_simple_typed_f(table, column, value, col_type, record, update_uuid, NULL);
}

void task_status(struct task *task, int status, char *result)
{
	struct schema_Command_State state;
	int done = 0;

	memset(&state, 0, sizeof(state));

	switch (status) {
	case TASK_WAITING:
		SCHEMA_SET_STR(state.state, "waiting");
		break;
	case TASK_PENDING:
		SCHEMA_SET_STR(state.state, "pending");
		break;
	case TASK_RUNNING:
		SCHEMA_SET_STR(state.state, "running");
		break;
	case TASK_COMPLETE:
		SCHEMA_SET_STR(state.state, "complete");
		done = 1;
		break;
	case TASK_FAILED:
		SCHEMA_SET_STR(state.state, "failed");
		done = 1;
		break;
	default:
		LOG(ERR, "Invalid task state");
		return;
	}

	memcpy(&state.cmd_uuid, &task->conf._uuid, sizeof(task->conf._uuid));
	state.cmd_uuid_exists = true;
	state.cmd_uuid_present = true;
	SCHEMA_SET_INT(state.timestamp, task->conf.timestamp);
	SCHEMA_SET_STR(state.command, task->conf.command);
	if (result) {
		STRSCPY(state.result_keys[0], "error");
	        STRSCPY(state.result[0], result);
		state.result_len = 1;
	}

	if (!ovsdb_table_upsert_simple_typed(&table_Command_State, SCHEMA_COLUMN(Command_State, cmd_uuid),
					     &state.cmd_uuid, OCLM_UUID, &state, NULL))
		LOG(ERR, "failed to update task status");

	if (!done)
		return;

	list_del(&task->list);
	free(task);
	task_running = 0;
	task_next();
}

static void task_run(void *arg)
{
	struct task *task = (struct task *)arg;

	task->pid = task->handler->cb(task);
	if (task->pid <= 0)
		task_status(task, TASK_FAILED, "failed to start task");
	else
		task_status(task, TASK_RUNNING, NULL);
}

static void task_next(void)
{
	struct task *task;

	if (task_running)
		return;

	if (list_empty(&tasks)) {
		LOGN("all tasks complete");
		return;
	}
	task = list_first_entry(&tasks, struct task, list);

	task_running = 1;

	if (task->conf.delay) {
		task_status(task, TASK_PENDING, NULL);
		evsched_task(&task_run, task, EVSCHED_SEC(task->conf.delay));
	} else
		task_run(task);

}

static int command_conf_add(struct schema_Command_Config *conf)
{
	unsigned int i;

	if (conf->command_exists && conf->timestamp_exists)
		for (i = 0; i < ARRAY_SIZE(cmd_handler); i++) {
			struct task *task;

			if (strcmp(cmd_handler[i].cmd, conf->command))
				continue;
			task = malloc(sizeof(*task));
			if (!task) {
				LOG(ERR, "failed to allocate command task");
				return -1;
			}
			memset(task, 0, sizeof(*task));
			memcpy(&task->conf, conf, sizeof(*conf));
			task->handler = &cmd_handler[i];
			if (task->conf.duration < 10)
				task->conf.duration = 10;
			list_add_tail(&task->list, &tasks);
			if (!list_empty(&tasks))
				task_status(task, TASK_WAITING, NULL);
			task_next();
			return 0;
		}

	return -1;
}

static void command_conf_del(struct schema_Command_Config *conf)
{
}

static void callback_Command_Config(ovsdb_update_monitor_t *mon,
			       struct schema_Command_Config *old_rec,
			       struct schema_Command_Config *iconf)
{
	switch (mon->mon_type) {
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		command_conf_add(iconf);
		break;
	case OVSDB_UPDATE_DEL:
		command_conf_del(old_rec);
		break;
	default:
		LOG(ERR, "Invalid Command_Config mon_type(%d)", mon->mon_type);
	}
}

void task_init(void)
{
	LOGI("Initializing command manager");

	OVSDB_TABLE_INIT(Command_Config, timestamp);
	OVSDB_TABLE_MONITOR(Command_Config, false);
	OVSDB_TABLE_INIT(Command_State, timestamp);

	target_serial_get(serial, sizeof(serial));

	return;
}
