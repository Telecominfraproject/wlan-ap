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
#define CPU_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/%s"

#define PATH_MAX 256
#define BUF_MAX 32
#define PHY0 0
#define PHY1 1
#define NUM_VALUES 4

int load_config_file=0;

//#define ULOG_INFO(fmt, ...) ulog(LOG_INFO, fmt, ## __VA_ARGS__)

typedef unsigned char	u8;
typedef unsigned long	u32;

u8 w2g_threshold_level=0;
u8 w5g_threshold_level=0;
u8 w2g_cur_temper=0;
u8 w5g_cur_temper=0;
u32 level_cpu_freq=1008000;

/* default value of wifi thresholds*/
#ifdef PLATFORM_RAP630C_311G
u8 level_2g_high[4]={105, 110, 115, 120};
u8 level_2g_low[4]={0, 105, 110, 115};
u8 level_2g_mitigation[4]={0, 35, 50, 70};
u8 level_5g_high[4]={105, 110, 115, 120};
u8 level_5g_low[4]={0, 105, 110, 115};
u8 level_5g_mitigation[4]={0, 20, 30, 50};
u32 level_cpu_frequency[4]={1008000, 800000, 800000, 800000};
#endif

#ifdef PLATFORM_RAP630W_311G
u8 level_2g_high[4]={105, 110, 115, 120};
u8 level_2g_low[4]={0, 105, 110, 115};
u8 level_2g_mitigation[4]={0, 20, 50, 70};
u8 level_5g_high[4]={105, 110, 115, 120};
u8 level_5g_low[4]={0, 105, 110, 115};
u8 level_5g_mitigatione[4]={0, 20, 50, 70};
u32 level_cpu_frequency[4]={1008000, 800000, 800000, 800000};
#endif

static char *config_file = NULL;

typedef struct {
    int thresholds_high[NUM_VALUES];
    int thresholds_low[NUM_VALUES];
    int mitigation[NUM_VALUES];
    int cpu_freq[NUM_VALUES];
} WifiConfig;

WifiConfig wifi2g = {0}, wifi5g = {0};

static void set_cpu_freq (int freq) {
	FILE * fp;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, CPU_FREQ_PATH, "scaling_governor");

	fp = fopen(filename, "w");
	if (!fp) {
		ULOG_ERR("open scaling_governor error\n");
	}
	fprintf(fp, "%s", "userspace");
	fclose(fp);

	snprintf(filename, PATH_MAX, CPU_FREQ_PATH, "scaling_setspeed");

	fp = fopen(filename, "w");
	if (!fp) {
		ULOG_ERR("open scaling_setspeed error\n");
	}
	fprintf(fp, "%d", freq);
	fclose(fp);
}

void parse_line(const char* line, const char* key, int* array) {
	char label[64];
	int values[NUM_VALUES];
	int i;

	if (sscanf(line, "%s %d %d %d %d %d", label, &values[0], &values[1], &values[2], &values[3], &values[4]) == 6) {
		for (i = 0; i < NUM_VALUES; ++i) {
			array[i] = values[i];
		}
	}
}

int load_config() {
	FILE * fp = fopen(config_file, "r");
	if (!fp) {
		ULOG_ERR("opne config file error\n");
		return 1;
	}

	WifiConfig* current_config = NULL;
	char line[256];

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "[wifi2g]")) {
			current_config = &wifi2g;
		} else if (strstr(line, "[wifi5g]")) {
			current_config = &wifi5g;
		} else if (current_config) {
			if (strstr(line, "thresholds_high")) {
				parse_line(line, "thresholds_high", current_config->thresholds_high);
			} else if (strstr(line, "thresholds_low")) {
				parse_line(line, "thresholds_low", current_config->thresholds_low);
			} else if (strstr(line, "mitigation")) {
				parse_line(line, "mitigation", current_config->mitigation);
			} else if (strstr(line, "CPU_freq")) {
				parse_line(line, "CPU_freq", current_config->cpu_freq);
			}
		}
	}

	fclose(fp);
	set_cpu_freq(wifi5g.cpu_freq[0]);

	return 0;
}

int load_default_config(){
	int i=0;

	set_cpu_freq(1008000);
	for (i = 0; i < NUM_VALUES; i++) {
		wifi2g.thresholds_high[i]=level_2g_high[i];
		wifi2g.thresholds_low[i]=level_2g_low[i];
		wifi2g.mitigation[i]=level_2g_mitigation[i];
		wifi2g.cpu_freq[i]=level_cpu_frequency[i];
		wifi5g.thresholds_high[i]=level_5g_high[i];
		wifi5g.thresholds_low[i]=level_5g_low[i];
		wifi5g.mitigation[i]=level_5g_mitigation[i];
		wifi5g.cpu_freq[i]=level_cpu_frequency[i];
	}
	return 0;

}

static void write_cur_state (const char *filename, int state) {
	FILE * fp;

	fp = fopen(filename, "w");
	if (!fp){
		ULOG_ERR("opne %s file error\n",filename);
	}
	fprintf(fp, "%d", state);
	fclose(fp);
}

int read_cur_state(const char *filename, char *buf, size_t buffer) {
	FILE *fp;
	
	fp = fopen(filename, "r");
	if (!fp) {
		ULOG_ERR("opne %s file error\n",filename);
		return -1;
	}
	if (!fgets(buf, buffer, fp)) {
		ULOG_ERR("Failed to read %s file\n", filename);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}


static void wifi_get_temperature() {
	char filename[PATH_MAX];
	char buffer[BUF_MAX];
	int i = 0;

//	ULOG_INFO("=================================\n");

	/* read cpuinfo_cur_freq*/
	snprintf(filename, PATH_MAX, CPU_FREQ_PATH, "cpuinfo_cur_freq");

	memset(buffer, 0, BUF_MAX);
	read_cur_state(filename, buffer, sizeof(buffer));
//	ULOG_INFO("CPU current frequency: %s\n", buffer);

	/* get current phy cooling state*/
	for (i=0; i <= 1; i++) {
		memset(buffer, 0, BUF_MAX);
		snprintf(filename, PATH_MAX, CUR_STATE_PATH, i);
		read_cur_state(filename, buffer, sizeof(buffer));
//		ULOG_INFO("Phy%i cur_state is: %s\n", i, buffer);
	}

	for (i=0; i <= 3; i++) {
		memset(buffer, 0, BUF_MAX);
		snprintf(filename, PATH_MAX, TEMPER_PATH, i);
		read_cur_state(filename, buffer, sizeof(buffer));
//		ULOG_INFO("thermal_zone%i cur_temp is: %s\n", i, buffer);

		if (i == 0)
			w2g_cur_temper=atoi(buffer);
		else if (i == 3)
			w5g_cur_temper=atoi(buffer);
	}

	if (w5g_cur_temper >= 120)
	{
		ULOG_ERR("!! Temperature is over %d degree, system will reboot\n", w5g_cur_temper);
		sync();
		if ( -1 != system("reboot &") ){
			printf("sysyem reboot...\n");
		}
	}
}

static void wifi_set_cooling() {
	char filename[PATH_MAX];
	int level;

	for (level = 0; level <= 3; level++) {
		if (w2g_cur_temper >= wifi2g.thresholds_low[level] && w2g_cur_temper < wifi2g.thresholds_high[level]) {
//			ULOG_INFO("2G at level %d , %d degree\n" ,level, w2g_cur_temper);
			if (w2g_threshold_level != level) {
//				ULOG_INFO("setting 2G reduce %d percent\n" ,wifi2g.mitigation[level]);
				snprintf(filename, PATH_MAX, CUR_STATE_PATH, PHY0);
				write_cur_state(filename, wifi2g.mitigation[level]);
				w2g_threshold_level = level;
			}
		}
		if (w5g_cur_temper >= wifi5g.thresholds_low[level] && w5g_cur_temper < wifi5g.thresholds_high[level]) {
//			ULOG_INFO("5G at level %d , %d degree\n" ,level, w5g_cur_temper);
			if (w5g_threshold_level != level) {
//				ULOG_INFO("setting 5G reduce %d percent\n" ,wifi5g.mitigation[level]);
				snprintf(filename, PATH_MAX, CUR_STATE_PATH, PHY1);
				write_cur_state(filename, wifi5g.mitigation[level]);
				w5g_threshold_level = level;
				set_cpu_freq(wifi5g.cpu_freq[level]);
			}
		}
	}

}


static void cooling_init() {
	char filename[PATH_MAX];
	int i,result=0;

	for (i=0 ; i <= 1; i++) {
		snprintf(filename, PATH_MAX, CUR_STATE_PATH, i);
		write_cur_state(filename, 0);
	}

	if(load_config_file)
		result = load_config();

	if (result == 1 || load_config_file == 0)
		load_default_config();
}

void print_usage(void)
{
	printf("\nWifi-cooling daemon usage\n");
	printf("Optional arguments:\n");
	printf("  -c <file>        setting with config file\n");
	printf("  -d               default setting\n");
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
	ulog_threshold(LOG_ERR);
//	ulog_threshold(LOG_INFO);

 	while ((ch = getopt(argc, argv, "c:dh")) != -1) {
		switch (ch) {
		case 'c':
			printf("wifi-cooling load configuration file %s\n", optarg);
			config_file = optarg;
			load_config_file=1;
			break;
		case 'd':
			printf("wifi-cooling set to default value\n");
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
