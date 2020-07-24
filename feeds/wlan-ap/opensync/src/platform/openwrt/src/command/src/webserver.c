#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "command.h"

static ovsdb_table_t table_Manager;
static int done;

static void stop_http(void *arg)
{
	LOGN("Stopping webserver");
	system("/etc/init.d/uhttpd stop");
	system("/etc/init.d/uhttpd disable");
}

static void callback_Manager(ovsdb_update_monitor_t *mon,
			     struct schema_Manager *old,
			     struct schema_Manager *conf)
{
	if (done || mon->mon_type == OVSDB_UPDATE_DEL)
		return;

	done = 1;
	LOGN("CM connected, stopping webserver in 5 minutes");
	unlink("/etc/config/uhttpd");
	evsched_task(&stop_http, NULL, EVSCHED_SEC(5 * 60));
}

void webserver_init(void)
{
	struct stat uhttpd = {};

	if (stat("/etc/config/uhttpd", &uhttpd) < 0)
		return;

	OVSDB_TABLE_INIT_NO_KEY(Manager);
	OVSDB_TABLE_MONITOR(Manager, false);
}
