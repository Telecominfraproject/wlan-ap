// Copyright (c) 2007-2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
//Qualcomm Technologies Proprietary and Confidential.

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

              Test Application for Diag Interface

GENERAL DESCRIPTION
  Contains main implementation of Diagnostic Services Test Application.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS
  

Copyright (c) 2007-2012 Qualcomm Technologies, Inc.
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
10/01/08   SJ     Created
===========================================================================*/

#include "event.h"
#include "msg.h"
#include "log.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "string.h"
#include "malloc.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/klog.h>

#define NUM_BYTES 100
/* Global Data */
char buffer_temp[1000];
int buffer_temp_end = 0;
char prev_delimiter = '6';

/* static data */
pthread_t fd_testapp_thread_hdl;			/* Diag task thread handle */

/*=============================================================================*/
/* Local Function declarations */
/*=============================================================================*/
void print_string(void);
void scan_buffer(char *buffer,int start);
typedef boolean (*ptr_Diag_LSM_Init)(byte* pIEnv);
typedef boolean (*ptr_Diag_LSM_DeInit)(void);
/*=============================================================================*/

/* Main Function. This initializes Diag_LSM, calls the tested APIs and exits. */
int main(void)
{
	boolean bInit_Success = FALSE;
	char *buffer;
	int count_bytes;

	buffer = (char*)malloc(sizeof(char) * NUM_BYTES);
	if (!buffer) {
		printf("TestApp_MultiThread: Could not allocate memory\n");
		return -1;
	}

	bInit_Success = Diag_LSM_Init(NULL);

	if(!bInit_Success)
	{
		printf("TestApp_MultiThread: Diag_LSM_Init() failed.");
		free(buffer);
		return -1;
	}
	printf("TestApp_MultiThread: Diag_LSM_Init succeeded.\n");

	do
	{
		memset(buffer, 0, sizeof(char) * NUM_BYTES);
		count_bytes = klogctl(2, buffer, NUM_BYTES);
		scan_buffer(buffer, 0);
		if(count_bytes < NUM_BYTES)
		print_string();
	} while(1);

	/* Now find the DeInit function and call it. Clean up before exiting */
	Diag_LSM_DeInit();

	free(buffer);

	return 0;
}

void scan_buffer(char *buffer, int start)
{
	int i,j;
	int found = 0;

	for(i=start;i<NUM_BYTES;i++)
	{
		if(buffer[i] == '<' && (buffer[i+1] >= 49 || buffer[i+1] <= 56) && buffer[i+2] == '>')
		{
			found = 1;
			for(j=start;j<i;j++)
				buffer_temp[buffer_temp_end + j-start] = buffer[j];

			print_string();
			buffer_temp_end = 0;
			start = i+3;
			prev_delimiter = buffer[i+1];
			break;
		}
	}

	if(found == 1)
	{
		scan_buffer(buffer, start);
	}
	else
	{
		for(i = start; i < NUM_BYTES; i++)
		{
			buffer_temp[i-start] = buffer[i];
		}
		buffer_temp_end = NUM_BYTES - start;
	}
}

void print_string(void)
{
	switch(prev_delimiter)
	{
	case '0':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_HIGH,"%s",buffer_temp);
		break;

	case '1':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_HIGH,"%s",buffer_temp);
		break;

	case '2':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_HIGH,"%s",buffer_temp);
		break;

	case '3':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_MED,"%s",buffer_temp);
		break;

	case '4':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_MED,"%s",buffer_temp);
		break;

	case '5':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_LOW,"%s",buffer_temp);
		break;

	case '6':
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_LOW,"%s",buffer_temp);
		break;

	default:
		MSG_SPRINTF_1(MSG_SSID_LK , MSG_LEGACY_LOW,"%s",buffer_temp);
	}

	memset(buffer_temp, 0, sizeof(char) * 1000);
}
