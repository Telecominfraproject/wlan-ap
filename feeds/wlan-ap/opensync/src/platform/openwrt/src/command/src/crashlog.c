/* SPDX-License-Identifier: BSD-3-Clause */

#include "command.h"

static ovsdb_table_t table_Alarms;
time_t crash_timestamp = 0;

void crashlog_init(void)
{
	struct schema_Alarms alarm = {};
	struct stat s = {};

	if (stat("/sys/kernel/debug/crashlog", &s) < 0)
		return;

	crash_timestamp = time(NULL);
	OVSDB_TABLE_INIT_NO_KEY(Alarms);

	SCHEMA_SET_STR(alarm.add_info, "reboot");
	SCHEMA_SET_STR(alarm.code, "oops");
	SCHEMA_SET_STR(alarm.source, "kernel");
	SCHEMA_SET_INT(alarm.timestamp, crash_timestamp);
	if (!ovsdb_table_upsert(&table_Alarms, &alarm, false))
		LOG(ERR, "crashlog: failed to insert alarm");

	return;
}
