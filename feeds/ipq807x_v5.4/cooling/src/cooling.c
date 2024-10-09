/*===========================================================================

  cooling.c

  DESCRIPTION 
  Thermal monitor and mitigation implementation functions.

===========================================================================*/
#include <stdio.h>		/* Standard input/output definitions */
#include <stdlib.h>
#include <string.h>		/* String function definitions */
#include <unistd.h>		/* UNIX standard function definitions */
#include <fcntl.h>		/* File control definitions */
#include <errno.h>		/* Error number definitions */
#include <getopt.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <libubox/ustream.h>
#include <libubox/uloop.h>
#include <libubox/list.h>
#include <libubox/ulog.h>
#include <libubus.h>


#define CUR_STATE_PATH "/sys/devices/virtual/thermal/cooling_device%i/cur_state"
#define TEMPER_PATH "/sys/devices/virtual/thermal/thermal_zone%i/temp"

#define PATH_MAX 256
#define BUF_MAX 8
#define THERSHOLD_MAX 20
#define PHY0 0
#define PHY1 1

typedef unsigned char	u8;

u8 w2g_threshold_level=0;
u8 w5g_threshold_level=0;
u8 w2g_cur_temper=0;
u8 w5g_cur_temper=0;


#ifdef PLATFORM_RAP630C_311G
u8 level_2g_lo[4]={0, 105, 110, 115};
u8 level_2g_hi[4]={105, 110, 115, 120};
u8 level_2g_limit[4]={0, 35, 50, 70};
u8 level_5g_lo[4]={0, 105, 110, 115};
u8 level_5g_hi[4]={105, 110, 115, 120};
u8 level_5g_limit[4]={0, 20, 30, 50};
#endif

#ifdef PLATFORM_RAP630W_311G
u8 level_2g_lo[4]={0, 105, 110, 115};
u8 level_2g_hi[4]={105, 110, 115, 120};
u8 level_2g_limit[4]={0, 20, 50, 70};
u8 level_5g_lo[4]={0, 105, 110, 115};
u8 level_5g_hi[4]={105, 110, 115, 120};
u8 level_5g_limit[4]={0, 20, 50, 70};
#endif

static char *config_file = NULL;
char temp[4][BUF_MAX];

#define ULOG_DBG(fmt, ...) ulog(LOG_DEBUG, fmt, ## __VA_ARGS__)

static void write_cur_state (char *filename, int state) {
	FILE * fp;

	ULOG_DBG("write_cur_state filename=[%s] [%d]\n", filename, state);
	fp = fopen(filename, "w");
	if (!fp){
		ULOG_ERR("some kind of error write cur_state\n");
	}
	fprintf(fp, "%d", state);
	fclose(fp);
}

static void read_cur_state (char *filename, char *buffer) {
	FILE * fp;

	fp = fopen(filename, "r");
	if (!fp){
		ULOG_ERR("some kind of error write cur_state\n");
	}
	if (0 == fread(buffer, sizeof(char), 3, fp)) {
		ULOG_ERR("some kind of error read value\n");
	}
	fclose(fp);
}

static void wifi_get_temperature() {
	char filename[PATH_MAX];
	FILE * fp;
	int i = 0;
	char buffer[BUF_MAX];

	/* get current phy cooling state*/
	for (i=0 ; i <= 1; i++ ) {
		memset(buffer, 0, BUF_MAX);
		snprintf(filename, PATH_MAX, CUR_STATE_PATH, i);
		read_cur_state(filename, buffer);
		ULOG_DBG("read from Phy%i cur_state is %s\n", i, buffer);
	}

	for (i=0 ; i <= 3; i++ ) {
		snprintf(filename, PATH_MAX, TEMPER_PATH, i);
		fp = fopen(filename, "r");
		if (!fp) {
			ULOG_ERR("some kind of error open value\n");
		}
		memset(temp[i], 0, BUF_MAX);
		if (0 == fread(temp[i], sizeof(char), 3, fp)) {
			ULOG_ERR("some kind of error read value\n");
		}
		fclose(fp);
		ULOG_DBG("thermal_zone%i cur_temp is %s\n", i, temp[i]);
	}

	w2g_cur_temper=atoi(temp[0]);
	w5g_cur_temper=atoi(temp[3]);
}

static void wifi_set_cooling() {
	char filename[PATH_MAX];
	int level;

	for (level=0 ; level<=3 ; level++) {
		if (w2g_cur_temper >= level_2g_lo[level] && w2g_cur_temper < level_2g_hi[level]) {
			ULOG_DBG("2G at level %d , %d degree\n" ,level, w2g_cur_temper);
			if (w2g_threshold_level != level) {
				ULOG_DBG("setting 2G reduce %d percent\n" ,level_2g_limit[level]);
				snprintf(filename, PATH_MAX, CUR_STATE_PATH, PHY0);
				write_cur_state(filename, level_2g_limit[level]);
				w2g_threshold_level = level;
			}
		}
		if (w5g_cur_temper >= level_5g_lo[level] && w5g_cur_temper < level_5g_hi[level]) {
			ULOG_DBG("5G at level %d , %d degree\n" ,level, w5g_cur_temper);
			if (w5g_threshold_level != level) {
				ULOG_DBG("setting 5G reduce %d percent\n" ,level_5g_limit[level]);
				snprintf(filename, PATH_MAX, CUR_STATE_PATH, PHY1);
				write_cur_state(filename, level_5g_limit[level]);
				w5g_threshold_level = level;
			}
		}
	}
}

static void cooling_init() {
	char filename[256];
	int i;

	for (i=0 ; i <= 1; i++) {
		snprintf(filename, PATH_MAX, CUR_STATE_PATH, i);
		write_cur_state(filename, 0);
	}
}

void print_usage(void)
{
	printf("\nWifi-cooling daemon usage\n");
	printf("Optional arguments:\n");
	printf("  -c <file>        config file\n");
	printf("  -d               debug output\n");
	printf("  -h               this usage screen\n");
}

static void state_timeout_cb(struct uloop_timeout *t) 
{
	wifi_get_temperature();
	wifi_set_cooling();
	uloop_timeout_set(t, 30 * 1000);		// interval 30 seconds
}

int main(int argc, char *argv[]) 
{
	int ch;
	struct uloop_timeout state_timeout = {
		.cb = state_timeout_cb,
	};

	setpriority(PRIO_PROCESS, getpid(), -20);

	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "cooling");
	ulog_threshold(LOG_INFO);

	while ((ch = getopt(argc, argv, "c:dh")) != -1) {
		switch (ch) {
		case 'c':
			printf("wifi-cooling load configuration file %s\n", optarg);
			config_file = optarg;
			break;
		case 'd':
			printf("wifi-cooling ulog_threshold set to debug level\n");
			ulog_threshold(LOG_DEBUG);
			break;
		case 'h':
		default:
			print_usage();
			return 0;
		}

	}

	cooling_init();
	uloop_init();
	uloop_timeout_set(&state_timeout, 1000);
	uloop_run();
	uloop_done();

	return 0;
}