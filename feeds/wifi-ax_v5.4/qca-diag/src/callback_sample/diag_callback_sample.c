/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

		Sample Application for Diag Callback Interface

GENERAL DESCRIPTION
  Contains sample implementation of Diagnostic Callback APIs.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2012-2014 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "malloc.h"
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include "errno.h"
#include "msg.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "diagpkt.h"
#include "diag_lsmi.h"
#include "diag_shared_i.h"

#define REQ_LOOPBACK_LEN	7
#define REQ_STRESSTEST_LEN	24

/* Callback for the receiving Diag data */
int process_diag_data(unsigned char *ptr, int len, void *context_data)
{
	int i;
	if (!ptr) {
		return 0;
	}

	if (context_data) {
		if (*(int *)context_data == MSM) {
			DIAG_LOGE("diag_callback_sample: Received data of len %d from MSM", len);
		} else if (*(int *)context_data == MDM) {
			DIAG_LOGE("diag_callback_sample: Received data of len %d from MDM", len);
		} else {
			DIAG_LOGE("diag_callback_sample: Received data of len %d from unknown proc %d", len, *(int *)context_data);
		}
	}

	for (i = 0; i < len; i++) {
		if (i % 8 == 0) {
			DIAG_LOGE("\n  ");
		}
		DIAG_LOGE("%02x ", ptr[i]);
	}
	DIAG_LOGE("\n");

	return 0;
}

/* Helper function to check if MDM is supported */
static uint8 is_mdm_supported()
{
	uint16 remote_mask = 0;
	uint8 err = 0;
	err = diag_has_remote_device(&remote_mask);
	if (err != 1) {
		DIAG_LOGE("diag_callback_sample: Unable to check for MDM support, err: %d\n", errno);
		return 0;
	}
	return (remote_mask & MDM);
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	int err = 0;
	int data_primary = MSM;
	int data_remote = MDM;
	uint8 mdm_support = 0;
	boolean status = FALSE;
	unsigned char req_modem_loopback[REQ_LOOPBACK_LEN] =
		{ 75, 18, 41, 0, 1, 2, 3 };
	unsigned char req_modem_msg_stress_test[REQ_STRESSTEST_LEN] =
		{ 75, 18, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char req_adsp_log_stress_test[REQ_STRESSTEST_LEN] =
		{ 75, 18, 7, 0, 1, 0, 0, 0, 16, 1, 1, 0, 0, 1, 0, 0, 5, 0, 0, 0, 10, 0, 0, 0 };

	status = Diag_LSM_Init(NULL);
	if (!status) {
		DIAG_LOGE("diag_callback_sample: Diag LSM Init failed, exiting... err: %d\n", errno);
		exit(0);
	}

	/* Register the callback function for receiving data from MSM */
	diag_register_callback(process_diag_data, &data_primary);

	/* Check if MDM is supported, If yes, register for MDM callback too */
	mdm_support = is_mdm_supported();
	if (mdm_support)
		diag_register_remote_callback(process_diag_data, MDM, &data_remote);

	/* Switch to Callback mode to receive Diag data in this application */
	diag_switch_logging(CALLBACK_MODE, NULL);

	/*
	 * You can now send requests to the processors. The response, and any
	 * log, event or F3s will be sent via the callback function.
	 */
	DIAG_LOGE("diag_callback_sample: Sending Modem loopback request to MSM\n");
	err = diag_callback_send_data(MSM, req_modem_loopback, REQ_LOOPBACK_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send Modem loopback request to MSM\n");
	} else {
		sleep(2);
	}

	DIAG_LOGE("diag_callback_sample: Sending Modem Message Stress Test to MSM\n");
	err = diag_callback_send_data(MSM, req_modem_msg_stress_test, REQ_STRESSTEST_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send Modem Message Stress Test to MSM\n");
	} else {
		sleep(30);
	}

	DIAG_LOGE("diag_callback_sample: Sending ADSP Log Stress Test to MSM\n");
	err = diag_callback_send_data(MSM, req_adsp_log_stress_test, REQ_STRESSTEST_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send ADSP Log Stress Test to MSM\n");
	} else {
		sleep(30);
	}

	if (!mdm_support)
		goto finish;

	DIAG_LOGE("diag_callback_sample: Sending Modem loopback request to MDM\n");
	/* If MDM is supported, send the requests to MDM ASIC as well */
	err = diag_callback_send_data(MDM, req_modem_loopback, REQ_LOOPBACK_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send Modem loopback request to MDM\n");
	} else {
		sleep(2);
	}

	DIAG_LOGE("diag_callback_sample: Sending Modem Message Stress Test to MDM\n");
	err = diag_callback_send_data(MDM, req_modem_msg_stress_test, REQ_STRESSTEST_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send Modem Message Stress Test to MDM\n");
	} else {
		sleep(30);
	}

	DIAG_LOGE("diag_callback_sample: Sending ADSP Log Stress Test to MDM\n");
	err = diag_callback_send_data(MDM, req_adsp_log_stress_test, REQ_STRESSTEST_LEN);
	if (err) {
		DIAG_LOGE("diag_callback_sample: Unable to send ADSP Log Stress Test to MDM\n");
	} else {
		sleep(30);
	}

finish:
	/*
	 * When you are done using the Callback Mode, it is highly recommended
	 * that you switch back to USB Mode.
	 */
	diag_switch_logging(USB_MODE, NULL);

	/* Release the handle to Diag*/
	status = Diag_LSM_DeInit();
	if (!status) {
		DIAG_LOGE("diag_callback_sample: Unable to close handle to diag driver, err: %d\n", errno);
		exit(0);
	}

	return 0;
}
