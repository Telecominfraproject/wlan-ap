/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

	      Sample Packet Response Test Application on Diag Interface

GENERAL DESCRIPTION
  Contains main implementation of PktRsp Test app. on Apps processor using
  Diagnostic Services.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2007-2011, 2016 Qualcomm Technologies, Inc.
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
3/11/2009  Shalabh Jain and Hiren Bhinde  Created
4/2/2009   Shalabh Jain and Yash Kharia   Adding ftmTest app to the mainline
===========================================================================*/

#include "msg.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "diagpkt.h"
#include "string.h"
#include <unistd.h>

/* Subsystem command codes for the test app  */
#define DIAG_TEST_APP_MT_NO_SUBSYS    143
#define DIAG_SUBSYS_TEST_CLIENT_MT     11
#define DIAG_TEST_APP_F_75            0x0000
#define DIAG_TEST_APP_F_75_test       0x0003

/*===========================================================================*/
/* Local Function declarations */
/*===========================================================================*/

void *dummy_func_no_subsys(void *req_pkt,
				   uint16 pkt_len);

void *dummy_func_75(void *req_pkt,
			    uint16 pkt_len);

void sample_parse_request(void *req_pkt, uint16 pkt_len, void *rsp);

/*===========================================================================*/
/* User tables for this client(ftmtest app) */
/*===========================================================================*/
static const diagpkt_user_table_entry_type test_tbl_1[] =
{ /* subsys cmd low, subsys cmd code high, call back function */
	{DIAG_TEST_APP_MT_NO_SUBSYS, DIAG_TEST_APP_MT_NO_SUBSYS,
		 dummy_func_no_subsys},
};

static const diagpkt_user_table_entry_type test_tbl_2[] =
{ /* susbsys_cmd_code lo = 0 , susbsys_cmd_code hi = 0, call back function */
	{DIAG_TEST_APP_F_75, DIAG_TEST_APP_F_75, dummy_func_75},
  /* susbsys_cmd_code lo = 3 , susbsys_cmd_code hi = 3, call back function */
	{DIAG_TEST_APP_F_75_test, DIAG_TEST_APP_F_75_test, dummy_func_75},
};

/*===========================================================================*/
/* Main Function. This initializes Diag_LSM, calls the tested APIs and exits. */
/*===========================================================================*/
int main(void)
{
	boolean bInit_Success = FALSE;

	printf("\n\t\t=====================");
	printf("\n\t\tStarting FTM Test App");
	printf("\n\t\t=====================");
#ifdef DIAG_DEBUG
	printf("\n Calling LSM init \n");
#endif
	/* Calling LSM init  */
	bInit_Success = Diag_LSM_Init(NULL);

	if (!bInit_Success) {
		printf("FTM Test App: Diag_LSM_Init() failed.");
		return -1;
	}

#ifdef DIAG_DEBUG
	printf("FTM Test App: Diag_LSM_Init succeeded. \n");
#endif
	/* Registering diag packet with no subsystem id.
	 * This is so that an empty request to the app. gets a response back
	 * and we can ensure that the diag is working as well as the app. is
	 * responding subsys id = 255, table = test_tb1_1 ....
	 * To execute on QXDM :: "send_data 143 0 0 0 0 0"
	 */
	DIAGPKT_DISPATCH_TABLE_REGISTER(DIAGPKT_NO_SUBSYS_ID, test_tbl_1);

	/* Registering diag packet with no subsystem id. This is so
	 * that an empty request to the app. gets a response back
	 * and we can ensure that the diag is working as well as the app. is
	 * responding subsys id = 11, table = test_tbl_2,
	 * To execute on QXDM :: "send_data 75 11 0 0 0 0 0 0"
		OR
	 * To execute on QXDM :: "send_data 75 11 3 0 0 0 0 0"
	 */
	DIAGPKT_DISPATCH_TABLE_REGISTER(DIAG_SUBSYS_TEST_CLIENT_MT,
					  test_tbl_2);

	/* Adding Sleep of 5 sec so that mask is updated */
	 sleep(5);
	do {
		MSG_1(MSG_SSID_DIAG, MSG_LVL_HIGH,
		      "Hello world from FTM Test App.%d", 270938);
		sleep(1);
	} while (1);

	/* Now find the DeInit function and call it.
	 * Clean up before exiting
	 */
	Diag_LSM_DeInit();

	return 0;
}


/*===========================================================================*/
/* dummy registered functions */
/*===========================================================================*/

void *dummy_func_no_subsys(void *req_pkt, uint16 pkt_len)
{
	void *rsp = NULL;
#ifdef DIAG_DEBUG
	printf("\n ##### FTM Test App: : Inside dummy_func_no_subsys #####\n");
#endif
	/* Allocate the same length as the request. */
	rsp = diagpkt_alloc(DIAG_TEST_APP_MT_NO_SUBSYS, pkt_len);

	if (rsp != NULL) {
		memcpy((void *) rsp, (void *) req_pkt, pkt_len);
#ifdef DIAG_DEBUG
		printf("FTM Test APP: diagpkt_alloc succeeded");
#endif
	} else {
		printf("FTM Test APP: diagpkt_subsys_alloc failed");
	}

	return rsp;
}


void *dummy_func_75(void *req_pkt, uint16 pkt_len)
{
	void *rsp = NULL;
#ifdef DIAG_DEBUG
	printf("\n FTM Test App: Inside dummy_func_75 \n");
#endif
	/* Allocate the same length as the request. */
	rsp = diagpkt_subsys_alloc(DIAG_TEST_APP_MT_NO_SUBSYS,
				    DIAG_TEST_APP_F_75, 20);
/* The request sent in from QXDM is parsed here. The response to each kind of
request can be customized. To demonstrate this, we look for codes 1, 2, 3, 4 in
the request (in the same order). For example, send_data 75 11 3 0 1 2 3 4 0 0 0.
Here a specific response string will be sent back. Any other request is simply
echoed back.This is demonstrated in sample_parse_request function
*/
	if (rsp != NULL) {
		sample_parse_request(req_pkt, pkt_len, rsp);
#ifdef DIAG_DEBUG
		printf("FTM Test App: diagpkt_subsys_alloc succeeded \n");
#endif
	} else
		printf("FTM Test APP: diagpkt_subsys_alloc failed");


	return rsp;
}

/*===========================================================================*/
/* dummy parse request function*/
/*===========================================================================*/

void sample_parse_request(void *req_pkt, uint16 pkt_len, void *rsp)
{
	unsigned char *temp = (unsigned char *)req_pkt + 4;
	int code1, code2, code3, code4;
	char *rsp_string = "FTM response";

	code1 = (int)(*(char *)temp);
	temp++;
	code2 = (int)(*(char *)temp);
	temp++;
	code3 = (int)(*(char *)temp);
	temp++;
	code4 = (int)(*(char *)temp);

	if (code1 == 1 && code2 == 2 && code3 == 3 && code4 == 4) {
		memcpy((void *) rsp, (void *) req_pkt, 4);
		memcpy((void *) ((unsigned char *)rsp+4), (void *) rsp_string,
			 strlen(rsp_string));
	} else
		memcpy((void *) (rsp), (void *) req_pkt, pkt_len);

}
