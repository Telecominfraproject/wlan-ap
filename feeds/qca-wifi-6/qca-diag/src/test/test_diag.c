/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
Copyright (c) 2007-2015, 2016 by Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.


              Test Application for Diag Interface

GENERAL DESCRIPTION
  Contains main implementation of Diagnostic Services Test Application.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS
  

Copyright (c) 2007-2015, 2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header: 

when       who    what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ     Changes for CBSP2.0
03/26/08   JV     Added calls to test packet request/response
12/2/07    jv     Added test calls for log APIs
11/19/07   mad    Created      
===========================================================================*/

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <ctype.h>
#include "errno.h"
#include "event.h"
#include "msg.h"
#include "log.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "diagdiag.h"
#include "diag_lsm.h"
#include "../src/ts_linux.h"
#include <limits.h>

#define DIAG_CMD_APPS_LOOPBACK_TEST	0x0028
#define DIAG_CMD_APPS_STRESS_TEST	0x0006
#define DIAG_CMD_APPS_DELAYED_RSP_TEST	0x0215

#define TIMESTAMP_LEN			8

#define DIAG_DELAYED_RSP_CNT_BASE	0x8000
#define DIAG_DELAYED_RSP_CNT_MAX	0xFFF

#define MAX_NUM_TASKS			50

#define PRECISION	1000000
#define INT_PART(a) (int)a
#define DEC_PART(a) ((a - (int)a) * PRECISION)

typedef PACK(struct) {
	log_hdr_type hdr;
	int iteration;
	int task_priority;
	int req_iterations;
	uint32 payload[125];
} diag_stress_test_log_t;

typedef PACK(struct) {
	int iteration;
	int task_priority;
	int req_iterations;
} diag_stress_test_event_t;

typedef PACK(struct) {
	uint8 test_type;
	uint8 priority_type;
	uint16 priority;
	uint32 num_iterations;
	uint32 sleep_time;
	uint32 iterations_sleep;
} diag_stress_test_task_info;

typedef PACK(struct) {
	uint8 cmd_code;
	uint8 subsys_cmd_code;
	uint16 subsys_id;
	uint32 num_tasks;
} diag_stress_test_req_t;

typedef struct {
	uint8 active;
	pthread_t thread_hdl;
	diag_stress_test_task_info task_info;
} diag_stress_test_hdl_t;

typedef PACK(struct) {
	uint8 cmd_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint16 count;
} diag_delayed_test_req_t;

typedef PACK(struct) {
	uint8 command_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint32 status;
	uint16 delayed_rsp_id;
	uint16 rsp_cnt;
} diagpkt_subsys_hdr_type_v2;

typedef PACK(struct) {
	diagpkt_subsys_hdr_type_v2 hdr;
	char my_rsp[20];
} diag_delayed_test_rsp_t;

typedef PACK(struct) {
	diagpkt_subsys_header_type hdr;
	unsigned long timestamp;
} diag_loopback_rsp_t;

static uint8 verbose;
static int max_num_tasks;
static diag_stress_test_hdl_t *test_info;
static pthread_mutex_t test_hdl_lock;

static diag_stress_test_hdl_t *diag_stress_test_get_hdl()
{
	int i;
	diag_stress_test_hdl_t *test_hdl = NULL;

	pthread_mutex_lock(&test_hdl_lock);
	for (i = 0; i < max_num_tasks; i++) {
		if (test_info[i].active)
			continue;
		test_info[i].active = 1;
		test_hdl = &test_info[i];
		break;
	}
	pthread_mutex_unlock(&test_hdl_lock);

	return test_hdl;
}

static void *diag_stress_test_thread(void* param)
{
	diag_stress_test_hdl_t *test_hdl = NULL;
	diag_stress_test_task_info *task_info = NULL;
	diag_stress_test_log_t *log = NULL;
	diag_stress_test_event_t event_payload;
	int i;
	uint32 payload_stress_test_complete = 0;

	if (!param) {
		printf("test_diag: Invalid params in %s\n", __func__);
		return 0;
	}

	test_hdl = (diag_stress_test_hdl_t *)param;
	task_info = &test_hdl->task_info;

	for (i = 1; i <= (int)task_info->num_iterations; i++) {
		if (task_info->iterations_sleep > 0 && (i % task_info->iterations_sleep == 0))
			sleep(task_info->sleep_time * 0.001);

		switch (task_info->test_type) {
		case DIAGDIAG_STRESS_TEST_MSG:
			MSG(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test MSG with no arg\n");
			break;
		case DIAGDIAG_STRESS_TEST_MSG_1:
			MSG_1(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_1 Iter %d\n", i);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_2:
			MSG_2(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_2 Iter %d uid %d\n", i, 1);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_3:
			MSG_3(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_3 Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_4:
			MSG_4(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_4 Iter %d uid %d num_iter %d procid %d\n", i, 1, 2, 3);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_5:
			MSG_5(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_5 Iter %d uid %d num_iter %d procid %d %d\n", i, 1, 2, 3, 4);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_6:
			MSG_6(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_6 Iter %d uid %d num_iter %d procid %d %d %d\n", i, 1, 2, 3, 4, 5);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_LOW:
			MSG_LOW("MSG_LOW Iter %d uid %d num_iter %d \n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_MED:
			MSG_MED("MSG_MED Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_HIGH:
			MSG_HIGH("MSG_HIGH Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_ERROR:
			MSG_ERROR("MSG_ERROR Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_FATAL:
			MSG_FATAL("MSG_FATAL Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_LOG:
			log = (diag_stress_test_log_t *)log_alloc(LOG_DIAG_STRESS_TEST_C,
								  sizeof(diag_stress_test_log_t));
			if (log) {
				log->iteration = i;
				log->req_iterations = task_info->num_iterations;
				log_commit(log);
			}
			break;
		case DIAGDIAG_STRESS_TEST_EVENT_NO_PAYLOAD:
			event_report(EVENT_DIAG_STRESS_TEST_NO_PAYLOAD);
			break;
		case DIAGDIAG_STRESS_TEST_EVENT_WITH_PAYLOAD:
			event_payload.iteration = i;
			event_payload.req_iterations = task_info->num_iterations;
			event_report_payload(EVENT_DIAG_STRESS_TEST_WITH_PAYLOAD, sizeof(event_payload), &event_payload);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG:
			QSR_MSG(300000001, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG with no arg\n");
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_1:
			QSR_MSG_1(300000002, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_1 Iter %d\n", i);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_2:
			QSR_MSG_2(300000003, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_2 Iter %d uid %d\n", i, 1);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_3:
			QSR_MSG_3(300000004, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_3 Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_4:
			QSR_MSG_4(300000005, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_4 Iter %d uid %d num_iter %d procid %d\n", i, 1, 2, 3);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_5:
			QSR_MSG_5(300000006, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_5 Iter %d uid %d num_iter %d procid %d %d\n", i, 1, 2, 3, 4);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_6:
			QSR_MSG_6(300000007, MSG_SSID_DIAG, MSG_LEGACY_HIGH, "Test QSR_MSG_6 Iter %d uid %d num_iter %d procid %d %d %d\n", i, 1, 2, 3, 4, 5);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_LOW:
			QSR_MSG_LOW(300000008, "Test QSR_MSG_LOW Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_MED:
			QSR_MSG_MED(300000009, "Test QSR_MSG_MED Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_HIGH:
			QSR_MSG_HIGH(300000011, "Test QSR_MSG_HIGH Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_ERROR:
			QSR_MSG_ERROR(300000012, "Test QSR_MSG_ERROR Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_QSR_MSG_FATAL:
			QSR_MSG_FATAL(300000013, "Test QSR_MSG_FATAL Iter %d uid %d num_iter %d\n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_SPRINTF_1:
			MSG_SPRINTF_1(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_SPRINTF_1 Iter %d\n", i);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_SPRINTF_2:
			MSG_SPRINTF_2(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_SPRINTF_2 Iter %d uid %d\n", i, 1);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_SPRINTF_3:
			MSG_SPRINTF_3(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_SPRINTF_3 Iter %d uid %d num_iter %d \n", i, 1, 2);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_SPRINTF_4:
			MSG_SPRINTF_4(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_SPRINTF_4 Iter %d uid %d num_iter %d %d \n", i, 1, 2, 3);
			break;
		case DIAGDIAG_STRESS_TEST_MSG_SPRINTF_5:
			MSG_SPRINTF_5(MSG_SSID_DIAG, MSG_LEGACY_HIGH, "MSG_SPRINTF_5 Iter %d uid %d num_iter %d %d.%d \n", i, 1, 2, INT_PART(1.234), DEC_PART(1.234));
			break;
		}
	}
	printf("\n");
	sleep(1);
	event_report_payload(EVENT_DIAG_STRESS_TEST_COMPLETED, sizeof(uint32), &payload_stress_test_complete);
	test_hdl->active = 0;
	return 0;
}

static void *diagdiag_apps_stress_test(void *req_pkt, uint16 pkt_len)
{
	void *rsp = NULL;
	int i;
	uint8 *request = (uint8 *)req_pkt;
	diag_stress_test_req_t *test_info = NULL;
	diag_stress_test_task_info *task_info = NULL;
	diag_stress_test_hdl_t *test_hdl = NULL;

	test_info = (diag_stress_test_req_t *)(request);
	request += sizeof(diag_stress_test_req_t);

	printf("test_diag: Received a stress test command for %d tests\n", (int)test_info->num_tasks);

	for (i = 1; i <= (int)test_info->num_tasks; i++) {
		task_info = (diag_stress_test_task_info *)request;
		test_hdl = diag_stress_test_get_hdl();
		if (!test_hdl) {
			printf("test_diag: Unable to get test handle, i: %d. Try after sometime\n", i);
			goto err_rsp;
		}
		memcpy(&test_hdl->task_info, task_info, sizeof(diag_stress_test_task_info));
		pthread_create(&test_hdl->thread_hdl, NULL, diag_stress_test_thread, test_hdl);
		if (test_hdl->thread_hdl == 0) {
			printf("test_diag: Unable to create thread handle, i: %d\n", i);
			test_hdl->active = 0;
			goto err_rsp;
		} else {
			test_hdl->active = 1;
		}

		if (verbose) {
			printf(" Task No:                  %d\n", i);
			printf(" Test type:                %d\n", (int)task_info->test_type);
			printf(" Priority type:            %d\n", (int)task_info->priority_type);
			printf(" Priority:                 %d\n", (int)task_info->priority);
			printf(" Number of iterations:     %d\n", (int)task_info->num_iterations);
			printf(" Sleep Duration:           %d\n", (int)task_info->sleep_time);
			printf(" Iteration before sleep:   %d\n", (int)task_info->iterations_sleep);
		}
		request += sizeof(diag_stress_test_task_info);
	}

	rsp = diagpkt_subsys_alloc(DIAG_SUBSYS_DIAG_SERV, DIAG_CMD_APPS_STRESS_TEST, pkt_len);
	if (rsp != NULL) {
		printf("test_diag: Allocated a response of size: %d\n", pkt_len);
		memcpy ((void *)rsp, (void *)req_pkt, pkt_len);
	}

	printf("\n");
	return rsp;

err_rsp:
	return diagpkt_err_rsp(DIAG_SUBSYS_CMD_F, req_pkt, pkt_len);
}

static void *diagdiag_loopback_test(void *req_pkt, uint16 pkt_len)
{
	uint16 actual_len = pkt_len + TIMESTAMP_LEN + 1;
	unsigned char *rsp = NULL;
	unsigned char *req = (unsigned char *)req_pkt;
	uint16 write_len = 0;
	uint16 read_len = 0;

	if (!req_pkt || pkt_len == 0) {
		printf("test_diag: In %s, invalid pointer %p or length: %d\n",
		       __func__, req_pkt, pkt_len);
		return NULL;
	}

	printf("test_diag: Received a loopback request of size: %d\n", pkt_len);
	rsp = (unsigned char *)diagpkt_subsys_alloc(DIAG_SUBSYS_DIAG_SERV,
						    DIAG_CMD_APPS_LOOPBACK_TEST,
						    actual_len);
	if (rsp != NULL) {
		printf("test_diag: Allocated a response of size: %d\n", actual_len);
		memcpy(rsp, req, sizeof(diagpkt_subsys_header_type));
		write_len += sizeof(diagpkt_subsys_header_type);
		read_len += sizeof(diagpkt_subsys_header_type);
		ts_get((void *)(rsp + write_len));
		write_len += sizeof(unsigned long);
		memcpy(rsp + write_len, req + read_len, pkt_len - read_len);
	}

	printf("\n");
	return rsp;
}

static void *diagdiag_apps_delayed_rsp_test(void *req_pkt, uint16 pkt_len)
{
	void *rsp = NULL;
	diag_delayed_test_rsp_t *my_rsp = NULL;
	diagpkt_subsys_delayed_rsp_id_type delay_rsp_id = 0;
	char first_resp[] = "First response.";
	char del_rsp[] = "Delayed response.";
	uint16 count;
	int i;
	(void) pkt_len;

	if (!req_pkt)
		return NULL;
	count = (*(diag_delayed_test_req_t *)req_pkt).count;
	if (count > DIAG_DELAYED_RSP_CNT_MAX) {
		printf("test_diag: Count is above limit. Truncating to %d\n", DIAG_DELAYED_RSP_CNT_MAX);
		count = DIAG_DELAYED_RSP_CNT_MAX;
	}

	/* Allocate the length of response. */
	rsp = diagpkt_subsys_alloc_v2(DIAG_SUBSYS_DIAG_SERV, DIAG_CMD_APPS_DELAYED_RSP_TEST,
				      sizeof(diag_delayed_test_rsp_t));
	if (!rsp) {
		printf("test_diag: In %s, Unable to allocate response, err: %d\n", __func__, errno);
		return NULL;
	}

	/*
	 * Get the delayed_rsp_id that was allocated by diag to
	 * use for the delayed response we're going to send next.
	 * This id is unique in the system.
	 */
	delay_rsp_id = diagpkt_subsys_get_delayed_rsp_id(rsp);

	/* Frame and commit the immediate response */
	my_rsp = (diag_delayed_test_rsp_t *)rsp;
	memcpy(my_rsp->my_rsp, first_resp, strlen(first_resp));
	diagpkt_commit(rsp);
	printf("test_diag: Successfully committed immediate response\n");

	if (count == 0) {
		printf("\n");
		return NULL;
	}

	/* Allocate and Commit (count - 1) delayed responses  */
	for (i = 1; i < (int)count; i++) {
		/* Create the delayed response */
		rsp = diagpkt_subsys_alloc_v2_delay(DIAG_SUBSYS_DIAG_SERV, DIAG_CMD_APPS_DELAYED_RSP_TEST,
						    delay_rsp_id, sizeof(diag_delayed_test_rsp_t));
		if (!rsp) {
			printf("test_diag: In %s, Unable to create delayed response %d, err: %d\n", __func__, i, errno);
			continue;
		}

		/* Set the response count. Please see the ICD to set this field */
		diagpkt_subsys_set_rsp_cnt(rsp, DIAG_DELAYED_RSP_CNT_BASE + i);
		my_rsp = (diag_delayed_test_rsp_t *) rsp;
		memcpy(my_rsp->my_rsp, del_rsp, strlen(del_rsp));
		diagpkt_delay_commit(rsp);
		if (verbose)
			printf("test_diag: Successfully committed delayed response, %d\n", i);
	}

	/* Commit the last delayed response */
	rsp = diagpkt_subsys_alloc_v2_delay(DIAG_SUBSYS_DIAG_SERV, DIAG_CMD_APPS_DELAYED_RSP_TEST,
					    delay_rsp_id, sizeof(diag_delayed_test_rsp_t));
	if (!rsp) {
		printf("test_diag: In %s, Unable to create delayed response %d, err: %d\n", __func__, count, errno);
		return NULL;
	}

	/* Set the response count. Please see the ICD to set this field */
	diagpkt_subsys_set_rsp_cnt(rsp, count - 1);
	my_rsp = (diag_delayed_test_rsp_t *)rsp;
	memcpy(my_rsp->my_rsp, del_rsp, strlen(del_rsp));
	diagpkt_delay_commit(rsp);
	printf("test_diag: Successfully committed last delayed response\n\n");

	return NULL;
}

static const diagpkt_user_table_entry_type diag_test_tbl[] = {
	{DIAG_CMD_APPS_STRESS_TEST, DIAG_CMD_APPS_STRESS_TEST, diagdiag_apps_stress_test},
	{DIAG_CMD_APPS_LOOPBACK_TEST, DIAG_CMD_APPS_LOOPBACK_TEST, diagdiag_loopback_test}
};

static const diagpkt_user_table_entry_type diag_test_tbl_delay[] = {
	{DIAG_CMD_APPS_DELAYED_RSP_TEST, DIAG_CMD_APPS_DELAYED_RSP_TEST, diagdiag_apps_delayed_rsp_test}
};

static void usage(char *progname)
{
	printf("\n");
	printf("  Usage for %s:\n\n", progname);
	printf("  -t, --threads: maximum number of stress test tasks to support\n");
	printf("  -d, --verbose: prints stress test task information\n");
	printf("  -h, --help:    help\n");
	printf("  e.g. test_diag -v -t 5 can support upto 5 stress test threads at the same time\n\n");
	exit(0);
}

static void parse_args(int argc, char **argv)
{
	int command, temp_size;
	struct option longopts[] = {
			{"threads",	1,	NULL,	't'},
			{"verbose",	0,	NULL,	'v'},
			{"help",	0,	NULL,	'h'},
	};

	if (argc == 1)
		return;

	while ((command = getopt_long(argc, argv, "t:vh", longopts, NULL)) != -1) {
		switch (command) {
		case 't':
			max_num_tasks = atoi(optarg);
			if (max_num_tasks <= 0 || max_num_tasks >= MAX_NUM_TASKS) {
				printf("test_diag: Invalid number of tasks, %d\n", max_num_tasks);
				exit(0);
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	unsigned int i = 0;
	boolean status = FALSE;
	verbose = 0;
	max_num_tasks = 1;
	pthread_mutex_init(&test_hdl_lock, NULL);
	parse_args(argc, argv);

	test_info = malloc(max_num_tasks * sizeof(diag_stress_test_hdl_t));
	if (!test_info) {
		printf("test_diag: Unable to allocate memory for stress test handle\n");
		exit(0);
	}
	memset(test_info, 0, sizeof(max_num_tasks * sizeof(diag_stress_test_hdl_t)));

	status = Diag_LSM_Init(NULL);
	if (!status) {
		printf("test_diag: Diag_LSM_Init failed, error: %d\n", errno);
		free(test_info);
		return -1;
	}
	printf("test_diag: Diag_LSM_Init succeeded\n\n");

	/* Register Apps Test Commands */
	DIAGPKT_DISPATCH_TABLE_REGISTER(DIAG_SUBSYS_DIAG_SERV, diag_test_tbl);
	/* Register Delayed Response Test Command */
	DIAGPKT_DISPATCH_TABLE_REGISTER_V2_DELAY(DIAG_SUBSYS_CMD_VER_2_F,
						 DIAG_SUBSYS_DIAG_SERV,
						 diag_test_tbl_delay);

	while (i < UINT_MAX)
		sleep(5);

	Diag_LSM_DeInit();
	free(test_info);

	return 0;
}
