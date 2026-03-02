/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

              Legacy Service Mapping layer implementation for Events

GENERAL DESCRIPTION
  Contains main implementation of Legacy Service Mapping layer for Diagnostic
  Event Services.

EXTERNALIZED FUNCTIONS
  event_report
  event_report_payload

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2007-2011, 2014-2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
10/01/08   SJ     Changes for CBSP2.0
05/01/08   JV     Added support to update the copy of event_mask in this process
                  during initialization and also on mask change
11/12/07   mad    Created

===========================================================================*/


/* ==========================================================================
   Include Files
========================================================================== */
#include "./../include/event.h"
#include "../include/diag_lsm.h"
#include "diag_lsmi.h" /* for declaration of diag Handle */
#include "diagsvc_malloc.h"
#include "event_defs.h"
#include "diagdiag.h"
#include "eventi.h"
#include "diag_lsm_event_i.h"
#include "diag_shared_i.h" /* for definition of diag_data struct. */

#include "ts_linux.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include "errno.h"
#include "stdio.h"
#include <memory.h>
//#include <sys/time.h>
//#include <time.h>

/*Local Function declarations*/
static byte *event_alloc (event_id_enum_type id, uint8 payload_length, int* pAlloc_Len);

/*this keeps track of number of failures to IDiagPkt_Send().
This will currently be used only internally.*/
static unsigned int gEvent_commit_to_cs_fail = 0;

int gnDiag_LSM_Event_Initialized = 0;

#define DCI_EVENT_MASK_SIZE		512
unsigned char dci_cumulative_event_mask[DCI_EVENT_MASK_SIZE];
int num_dci_clients_event;


//unsigned char* event_mask = NULL;

/*===========================================================================

FUNCTION event_update_mask

DESCRIPTION
  Update event mask structure as per data from tool

DEPENDENCIES
   None

RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void event_update_mask(unsigned char* ptr, int len)
{
	if (!ptr || len <= 0 || !gnDiag_LSM_Event_Initialized)
		return;

	if (len > EVENT_MASK_SIZE)
		len = EVENT_MASK_SIZE;

	memcpy(event_mask, ptr, len);
}

/*===========================================================================

FUNCTION event_update_dci_mask

DESCRIPTION
  Update cumulative dci event mask as per data passed from dci in diag kernel

DEPENDENCIES
   None

RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void event_update_dci_mask(unsigned char* ptr, int len)
{
	if (!ptr || !gnDiag_LSM_Event_Initialized || len < (int)sizeof(int))
		return;

	num_dci_clients_event = *(int *)ptr;
	ptr += sizeof(int);
	len -= sizeof(int);
	if ( len > DCI_EVENT_MASK_SIZE)
		len = DCI_EVENT_MASK_SIZE;

	/* Populating dci event mask table */
	memcpy(dci_cumulative_event_mask, ptr, len);
}

/* Externalized functions */
/*===========================================================================

FUNCTION EVENT_REPORT

DESCRIPTION
  Report an event. Published Diag API.

DEPENDENCIES
   Diag Event service must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void event_report (event_id_enum_type Event_Id)
{
   if(diag_fd != -1)
   {
      byte *pEvent;
      int Alloc_Len = 0;
      pEvent = event_alloc (Event_Id, 0, &Alloc_Len);
      if(pEvent)
      {
	     int NumberOfBytesWritten = 0;
		 NumberOfBytesWritten = write(diag_fd, (const void*) pEvent, Alloc_Len);
		 if(NumberOfBytesWritten != 0)
	     {
			DIAG_LOGE("Diag_LSM_Event: Write failed in %s, bytes written: %d, error: %d\n",
				   __func__, NumberOfBytesWritten, errno);
            gEvent_commit_to_cs_fail++;
         }

         DiagSvc_Free(pEvent, GEN_SVC_ID);
      }

   }
   return;
}/* event_report */

/*===========================================================================

FUNCTION EVENT_REPORT_PAYLOAD

DESCRIPTION
  Report an event with payload data.

DEPENDENCIES
  Diag Event service must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void
event_report_payload (event_id_enum_type Event_Id, uint8 Length, void *pPayload)
{
   if(diag_fd != -1)
   {
      byte *pEvent = NULL;
      int Alloc_Len = 0;
      if (Length > 0 && pPayload)
      {
         pEvent = event_alloc (Event_Id, Length, &Alloc_Len);
         if (pEvent)
         {
            struct event_store_type* temp = (struct event_store_type*) (pEvent + FPOS(diag_data, rest_of_data));
			if(Length <= 2)
					memcpy (&temp->payload, pPayload, Length);// Dont need the length field if payload <= 2 bytes
			else
					memcpy (EVENT_LARGE_PAYLOAD(temp->payload.payload), pPayload, Length);
		    int NumberOfBytesWritten = 0;
			NumberOfBytesWritten = write(diag_fd, (const void*) pEvent, Alloc_Len);
            if(NumberOfBytesWritten != 0)
            {
				DIAG_LOGE("Diag_LSM_Event: Write failed in %s, bytes written: %d, error: %d\n",
				   __func__, NumberOfBytesWritten, errno);
				gEvent_commit_to_cs_fail++;
            }
            DiagSvc_Free(pEvent, GEN_SVC_ID);
         }

      }
      else
      {
         event_report (Event_Id);
      }
   }
   return;
}/* event_report_payload */

/*==========================================================================

FUNCTION event_alloc

DESCRIPTION
  This routine allocates an event item from the process heap and fills in
  the following information:

  Event ID
  Time stamp
  Payload length field

  //TODO :This routine also detects dropped events and handles the reporting of
  //dropped events.

RETURN VALUE
  A pointer to the allocated  event is returned.
  NULL if the event cannot be allocated.
  The memory should be freed by the calling function, using DiagSvc_Free().
  pAlloc_Len is an output value, indicating the number of bytes allocated.

===========================================================================*/
static byte *
event_alloc (event_id_enum_type id, uint8 payload_length, int* pAlloc_Len)
{
   byte *pEvent = NULL;
   int alloc_len = 0, header_length;
   boolean mask_set = 0;
   boolean dci_mask_set = 0;

   if(!gnDiag_LSM_Event_Initialized)
	   return NULL;

 /*    int idx = 0;
	   printf("In event_alloc: event_mask = %x \n",(unsigned int)event_mask);
  if(event_mask)
    {
       printf("Attempting to Read event_mask \n");
       for (idx = 0; idx < EVENT_MASK_SIZE && idx < 5; idx++)
       {
	     printf("event_mask[%d]=%d\n",idx,event_mask[idx]);
         // if(!gbRemote)
           //  event_mask[idx]=10;
       }
    }
  */
   /* Verify that the event id is in the right range and that the
    corresponding bit is turned on in the event mask. */
// removed masking for Phase I

	if (id <= EVENT_LAST_ID) {
		mask_set = EVENT_MASK_BIT_SET (id);
		if (num_dci_clients_event > 0) {
			dci_mask_set  = (dci_cumulative_event_mask[(id)/8] &
							(1 << ((id) & 0x07)));
		}
	}
	if (!mask_set && !dci_mask_set)
	{
		//printf("event_alloc: mask check returned FALSE \n");
		mask_set = 0;
		return NULL;
	}

// alloc_len = FPOS (event_store_type, payload.payload) + payload_length;
	// Prototyping Diag 1.5 WM7: Adding a uint32 so the diag driver can identify this as an event.

  alloc_len =  FPOS(diag_data, rest_of_data) + FPOS (struct event_store_type, payload.payload) + payload_length ;
	  pEvent = (byte *) DiagSvc_Malloc(alloc_len, GEN_SVC_ID);
      if (pEvent)
   {
      struct event_store_type* temp = NULL;
      diag_data* pdiag_data = (diag_data*) pEvent;
      //Prototyping Diag 1.5 WM7:Fill in the fact that this is an event.
	pdiag_data->diag_data_type = 0;
	if (mask_set)
		pdiag_data->diag_data_type |= DIAG_DATA_TYPE_EVENT;
	if (dci_mask_set)
		pdiag_data->diag_data_type |= DIAG_DATA_TYPE_DCI_EVENT;


      //Prototyping Diag 1.5 WM7:Advance the pointer to point to the event_store_type part
      temp = (struct event_store_type*) (pEvent + FPOS(diag_data, rest_of_data));

      if(pAlloc_Len)
      {
		  *pAlloc_Len = alloc_len;
      }

	  ts_get_lohi(&(temp->ts_lo), &(temp->ts_hi));
	  temp->cmd_code = 96;
	  temp->event_id.event_id_field.id = id;
	  temp->event_id.event_id_field.time_trunc_flag = 0;
      header_length = sizeof(temp->event_id) + sizeof(temp->ts_lo) + sizeof(temp->ts_hi);

	  if(payload_length <= 2)
	  {
		  alloc_len--;
		  if(pAlloc_Len)
		  {
			*pAlloc_Len = alloc_len;
		  }
		  else
		  {
			  DIAG_LOGE("event_alloc: Error, null pointer "
				"encountered for returning allocation "
				"length\n");
		  }
		  temp->length = header_length + payload_length;
	  }
	  else
	  {
			  // Add the payload length field only if payload more than 2 bytes
			  temp->payload.length = payload_length;
			  //adding the payload length field
			  temp->length = header_length + sizeof(temp->payload.length) + payload_length;
	  }

	  switch(payload_length)
	  {
		case 0:
		temp->event_id.event_id_field.payload_len = 0x0;
		break;

		case 1:
		temp->event_id.event_id_field.payload_len = 0x1;
		break;

		case 2:
		temp->event_id.event_id_field.payload_len = 0x2;
		break;

		default:
		temp->event_id.event_id_field.payload_len = 0x3;
	  }
   }

   return pEvent;
} /* event_alloc */

 /*===========================================================================

FUNCTION Diag_LSM_Event_Init

DESCRIPTION
  Initializes the event service

RETURN VALUE
  boolean indicating success

SIDE EFFECTS
  None.
===========================================================================*/
boolean Diag_LSM_Event_Init(void)
{
	boolean status = TRUE;
	if(!gnDiag_LSM_Event_Initialized)
	{
		num_dci_clients_event = 0;
		memset(dci_cumulative_event_mask, 0, DCI_EVENT_MASK_SIZE);
		gnDiag_LSM_Event_Initialized = TRUE;
	}
	return status;

} /* Diag_LSM_Event_Init */

/*===========================================================================

FUNCTION Diag_LSM_Event_DeInit

DESCRIPTION
  Deinitializes the event service


SIDE EFFECTS
  None.
===========================================================================*/
void Diag_LSM_Event_DeInit(void)
{
	gnDiag_LSM_Event_Initialized = FALSE;

} /* Diag_LSM_Event_Init */

