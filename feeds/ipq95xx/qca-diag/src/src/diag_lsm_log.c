/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

              Diag Interface

GENERAL DESCRIPTION
   Contains main implementation of Diagnostic Log Services.

EXTERNALIZED FUNCTIONS
   log_set_code
   log_set_length
   log_set_timestamp
   log_submit
   log_free
   log_get_length
   log_get_code
   log_status
   log_alloc
   log_commit
   log_shorten

INITIALIZATION AND SEQUENCING REQUIREMENTS


Copyright (c) 2007-2011, 2014-2015, 2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

                              Edit History

$Header:

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ      Changes for CBSP2.0
05/01/08   JV      Added support to update the copy of log_mask in this process
                   during initialization and also on mask change
02/11/08   JV      Changed the comparison of IDiagPkt_Send()'s return value from
                   AEE_FAILURE to AEE_SUCCESS
01/16/08   JV      Created stubs for log_on_demand_register and log_on_demand_unregister
                   as we do not have function pointer support for Diag 1.5A
11/20/07   JV      Created

===========================================================================*/


/* ==========================================================================
   Include Files
========================================================================== */
#include "stdio.h"
#include "comdef.h"
#include "diagi.h"
#include "diagpkt.h"
#include "diagdiag.h"
#include "log.h"
#include "msg.h"
#include "log_codes.h"
#include "diagsvc_malloc.h"
#include "diag_lsm_log_i.h"
#include "diag_lsmi.h" /* for declaration of IDiagPkt handle, gpiDiagPkt */

#include <string.h>
#include "diag_shared_i.h" /* for definition of diag_data struct, and diag datatypes. */
#include "ts_linux.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include "errno.h"
#include <assert.h>
#include "diaglogi.h"
#include "../include/diag_lsm.h"

static int log_inited;
static int log_commit_to_cs_fail;
static int num_dci_clients_log;
static byte *log_mask;
static byte *dci_cumulative_log_mask;
static uint32 log_status_mask (log_code_type code);
static boolean log_search_mask (unsigned int log_type, unsigned int id, unsigned int item);
static boolean log_mask_enabled (const byte *xx_mask_ptr, unsigned int xx_id,
                                 unsigned int xx_item);
static boolean log_dci_mask_enabled(const byte *xx_mask_ptr, unsigned int xx_id,
					unsigned int xx_item);

/* External Function Implementations */
/*===========================================================================

FUNCTION log_update_mask

DESCRIPTION
  Update log masks as per data passed by tool

DEPENDENCIES:
  None

RETURN VALUE
  None

SIDE EFFECTS
  None
===========================================================================*/

void log_update_mask(unsigned char* ptr, int len)
{
	int i;
	int read_len = 0;
	int copy_len = 0;
	diag_log_mask_update_t *src = NULL;
	diag_log_mask_t *dest = NULL;

	if (!ptr || len <= (int)sizeof(diag_log_mask_update_t) || !log_inited)
		return;

	dest = (diag_log_mask_t *)(log_mask);

	for (i = 0; i < MAX_EQUIP_ID && read_len < len; i++, dest++) {
		src = (diag_log_mask_update_t *)(ptr + read_len);
		read_len += sizeof(diag_log_mask_update_t);
		dest->equip_id = src->equip_id;
		dest->num_items = src->num_items;
		copy_len = LOG_ITEMS_TO_SIZE(dest->num_items);
		if (copy_len > MAX_ITEMS_PER_EQUIP_ID)
			copy_len = MAX_ITEMS_PER_EQUIP_ID;
		memcpy(dest->mask, ptr + read_len, copy_len);
		read_len += LOG_ITEMS_TO_SIZE(dest->num_items);
	}
}

/*===========================================================================

FUNCTION log_update_dci_mask

DESCRIPTION
  Update cumulative dci log masks as per data passed from dci in diag kernel

DEPENDENCIES:
  None

RETURN VALUE
  None

SIDE EFFECTS
  None
===========================================================================*/

void log_update_dci_mask(unsigned char* ptr, int len)
{
	if (!ptr || !log_inited || len < (int)sizeof(int))
		return;

	num_dci_clients_log = *(int *)ptr;
	ptr += sizeof(int);
	len -= sizeof(int);

	if ( len > DCI_LOG_MASK_SIZE)
		len = DCI_LOG_MASK_SIZE;

	memcpy(dci_cumulative_log_mask, ptr, len);
}


/*===========================================================================

FUNCTION LOG_ALLOC

DESCRIPTION
  This function allocates a buffer of size 'length' for logging data.  The
  specified length is the length of the entire log, including the log
  header.  This operation is inteneded only for logs that do not require
  data accumulation.

  !!! The header is filled in automatically by this routine.

DEPENDENCIES:
   Diag log service must be initialized.
   log_commit() or log_free() must be called ASAP after this call.

RETURN VALUE
  A pointer to the allocated buffer is returned on success.
  If the log code is disabled or there is not enough space, NULL is returned.

SIDE EFFECTS
  Since this allocation is made from a shared resource pool, log_commit()
  or log_free() must be called as soon as possible and in a timely fashion.
  This allocation system has no garbage collection.  Calling this routine
  places the log buffer in a FIFO queue.  If you hold the pointer for a
  significant period of time, the diag task will be blocked waiting for
  you to call log_commit().

  If you need to log accumulated data, store the accumulated data in your
  own memory space and use log_submit() to log the data.
===========================================================================*/
void *log_alloc (
   log_code_type code,
   unsigned int length
)
{
   diag_log_rsp_type *plog_pkt_ptr; /* Pointer to packet being created */
   log_header_type *phdr_ptr = NULL;
   void *return_ptr = NULL;
   uint32 enabled_mask = 0;

   if(-1 == diag_fd || !log_inited)
   {
      return NULL;
   }
  if (length <= sizeof(log_header_type))
  {
     DIAG_LOGE(" Alloc invalid length %d", length);
  }
  else if ((enabled_mask = log_status_mask(code)) != 0)
  {
    /*------------------------------------------------
     Allocate enough for entire LOG response packet,
     not just the log given.
    ------------------------------------------------*/

     /*WM7 prototyping: need to allocate bytes that indicate diag data type,.*/
      plog_pkt_ptr = (diag_log_rsp_type *) DiagSvc_Malloc
          (DIAG_REST_OF_DATA_POS + FPOS (diag_log_rsp_type, log) + length, GEN_SVC_ID);

      if (plog_pkt_ptr != NULL)
      {
          byte* temp = (byte*)plog_pkt_ptr;
          diag_data* pdiag_data = (diag_data*) plog_pkt_ptr;
          //Prototyping Diag 1.5 WM7:Fill in the fact that this is a log.
	  pdiag_data->diag_data_type = enabled_mask;
          //Prototyping Diag 1.5 WM7:Advance the pointer to point to the log_header_type part
          temp += DIAG_REST_OF_DATA_POS;
          plog_pkt_ptr = (diag_log_rsp_type*)temp;

          plog_pkt_ptr->cmd_code = DIAG_LOG_F;
          phdr_ptr = (log_header_type *) &(plog_pkt_ptr->log);
          ts_get_lohi(&(phdr_ptr->ts_lo), &(phdr_ptr->ts_hi));
          phdr_ptr->len = (uint16)length;
          phdr_ptr->code = code;

         /* Fill in top of packet. */
          plog_pkt_ptr->more = 0;
          plog_pkt_ptr->len  = (uint16)length;

          return_ptr = (void *) &(plog_pkt_ptr->log);
      }
      else
      {
         /* Dropped a log. */
         //MSG_LOW("Dropped log 0x%x", code, 0, 0);
         /*  WM7 prototyping */
         DIAG_LOGE(" Dropped log 0x%x", code);
      }

  } /* if valid and enabled */

  // removed masking for Phase I
  else
  {
     //printf("log_alloc: mask check returned FALSE \n");
  }

  return return_ptr;

} /* log_alloc */


/*===========================================================================

FUNCTION LOG_SHORTEN

DESCRIPTION
  This function shortens the length of a previously allocated logging buffer in
  legacy code. This is used when the size of the record is not known at allocation
  time.Now that diagbuf is not used in the LSM layer and we just use memory from
  a pre-allocated pool, calling log_shorten, does not free the excess memory, it just
  updates the length field.

DEPENDENCIES:
   Diag log service must be initialized.
   This must be called prior to log_commit().

RETURN VALUE
  None.

===========================================================================*/
 void log_shorten (
   void *ptr,
   unsigned int length
 )
 {
     byte *pdiag_pkt = (byte *) ptr; /* use byte* for pointer arithmetic */
     diag_log_rsp_type *pdiag_log;

     if (ptr)
     {
        /* WM7 Diag 1.5 prototyping */
        pdiag_pkt -= (LOG_DIAGPKT_OFFSET);
        pdiag_log = (diag_log_rsp_type *) pdiag_pkt;

        if (length < pdiag_log->len)
        {
         /* LSM does not free any memory here. We only update
		  the length parameter. The entire chunk
          of pre-malloced memory is freed after use */

       /* Set the log packet length to the new length */
            pdiag_log->len = (uint16)length;

       /* log_set_length takes the log itself, not the log packet */
           log_set_length (ptr, length);
        }
     }
 } /* log_shorten */

/*===========================================================================

FUNCTION LOG_COMMIT

DESCRIPTION
  This function commits a log buffer allocated by log_alloc().  Calling this
  function tells the logging service that the user is finished with the
  allocated buffer.

DEPENDENCIES:
   Diag log service must be initialized.
   'ptr' must point to the address that was returned by a prior call to
  log_alloc().

RETURN VALUE
  None.

SIDE EFFECTS
  Since this allocation is made from a shared resource pool, this must be
  called as soon as possible after a log_alloc call.  This operation is not
  intended for logs that take considerable amounts of time ( > 0.01 sec ).
===========================================================================*/
void log_commit (void *ptr)
{
  if (ptr)
  {
      log_header_type *phdr_ptr = NULL;
      log_commit_last = (void *) ptr;
    /* Set pointer to begining of diag pkt, not the log */
      phdr_ptr = (log_header_type *)ptr;
      ptr = ((byte *) ptr - (LOG_DIAGPKT_OFFSET+DIAG_REST_OF_DATA_POS)); /* WM7 prototyping */
      if(-1 != diag_fd)
      {
       int NumberOfBytesWritten = 0;
	   if((NumberOfBytesWritten = write(diag_fd, (const void*) ptr, DIAG_REST_OF_DATA_POS+LOG_DIAGPKT_OFFSET + phdr_ptr->len)) != 0) /*TODO: Check the Numberofbyteswritten against number of bytes we wanted to write?*/
       {
		DIAG_LOGE("Diag_LSM_log: Write failed in %s, bytes written: %d, error: %d\n",
			 __func__, NumberOfBytesWritten, errno);
		log_commit_to_cs_fail++;
       }
       DiagSvc_Free(ptr, GEN_SVC_ID);
      }
  }
  return;
} /* log_commit */

/*===========================================================================

FUNCTION LOG_FREE

DESCRIPTION
  This function frees the buffer in pre-allocated memory.

DEPENDENCIES:
  Diag log service must be initialized.

RETURN VALUE

SIDE EFFECTS
  None.
===========================================================================*/
void log_free (void *ptr)
{
   if(ptr)
   {
       ptr = ((byte *) ptr - (LOG_DIAGPKT_OFFSET + DIAG_REST_OF_DATA_POS));
      DiagSvc_Free(ptr, GEN_SVC_ID);
   }

} /* log_free */



/*===========================================================================

FUNCTION LOG_SUBMIT

DESCRIPTION
  This function is called to log an accumlated log entry. If logging is
  enabled for the entry by the external device, then this function essentially
  does the folliwng:
  log = log_alloc ();
  memcpy (log, ptr, log->len);
  log_commit (log);


DEPENDENCIES
   Diag log service must be initialized.

RETURN VALUE
  Boolean indicating success.

SIDE EFFECTS
  None.
===========================================================================*/
#ifndef MSM5000_IRAM_FWD /* Flag to use internal RAM */
boolean log_submit (void *ptr)
{
   boolean bReturnVal = FALSE; //# int i;
   /* The header is common to all logs, and is always at the beginning of the
		  * packet. */
   log_header_type *plog_ptr = (log_header_type *) ptr;
   uint32 enabled_mask = 0;

   if (plog_ptr && (diag_fd != -1) && log_inited)
   {
      /* Local vars to avoid use of misaligned variables */
      log_code_type code = plog_ptr->code;
      unsigned int length = plog_ptr->len;

	if (length > sizeof(log_header_type))
	{
		if ((enabled_mask = log_status_mask(code)) != 0) {
			diag_data* pdiag_data = (diag_data*)
                    		DiagSvc_Malloc(DIAG_REST_OF_DATA_POS + FPOS (diag_log_rsp_type, log) + length, GEN_SVC_ID);
			if (pdiag_data != NULL) {
				diag_log_rsp_type *plog_pkt_ptr = NULL;
				byte* temp = (byte*)pdiag_data;
				//Prototyping Diag 1.5 WM7:Fill in the fact that this is a log.
				pdiag_data->diag_data_type = enabled_mask;
				//Prototyping Diag 1.5 WM7:Advance the pointer to point to the log_header_type part
				temp += DIAG_REST_OF_DATA_POS;
				plog_pkt_ptr = (diag_log_rsp_type*)temp;
				plog_pkt_ptr->cmd_code = DIAG_LOG_F;
				plog_pkt_ptr->more = 0;
				plog_pkt_ptr->len  = (uint16)length;
				memcpy (&plog_pkt_ptr->log, (void *) ptr, length);
                int NumberOfBytesWritten = 0;
				if((NumberOfBytesWritten = write(diag_fd, (const void*) pdiag_data, DIAG_REST_OF_DATA_POS +  FPOS (diag_log_rsp_type, log) + length)) != 0) {
					DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
						__func__, NumberOfBytesWritten, errno);
					log_commit_to_cs_fail++;
					bReturnVal = FALSE;
				}
				else
					bReturnVal = TRUE;
				DiagSvc_Free(pdiag_data, GEN_SVC_ID);
			} /* if (pdiag_data != NULL) */
			else {
				/* Dropped a log */
				DIAG_LOGE(" Dropped log 0x%x", code);
			}
			//removed masking for Phase I
		}/* if (log_status (code)) */
		else {
			//printf("log_submit: mask check returned FALSE \n");
                }
  	} /* if (length > sizeof(log_header_type)) */
   } /* if (plog_ptr) */
   return bReturnVal;

} /* log_submit */
#endif /* !MSM5000_IRAM_FWD */




/*===========================================================================

FUNCTION LOG_SET_LENGTH

DESCRIPTION
  This function sets the length field in the given log record.

  !!! Use with caution.  It is possible to corrupt a log record using this
  command.  It is intended for use only with accumulated log records, not
  buffers returned by log_alloc().

DEPENDENCIES
  Diag log service must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void log_set_length (void *ptr, unsigned int length)
{
	if(ptr)
	{
		/* All log packets are required to start with 'log_header_type'. */
        ((log_header_type *) ptr)->len = (uint16) length;
	}
} /* log_set_length */



/*===========================================================================

FUNCTION LOG_SET_CODE

DESCRIPTION
  This function sets the logging code in the given log record.

DEPENDENCIES
  Diag log service must be initialized.


RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void log_set_code (void *ptr, log_code_type code)
{
	if (ptr)
	{
		/* All log packets are required to start with 'log_header_type'. */
        ((log_header_type *) ptr)->code = code;
	}

} /* log_set_code */


/*===========================================================================

FUNCTION LOG_SET_TIMESTAMP

DESCRIPTION
  This function captures the system time and stores it in the given log record.

DEPENDENCIES
   Diag log service must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.
===========================================================================*/
void log_set_timestamp (void *plog_hdr_ptr)
{
	if (plog_hdr_ptr)
	{
		ts_get_lohi(&(((log_header_type *) plog_hdr_ptr)->ts_lo),
				&(((log_header_type *) plog_hdr_ptr)->ts_hi));
	}
} /* log_set_timestamp */


/*===========================================================================

FUNCTION LOG_GET_LENGTH

DESCRIPTION
  This function returns the length field in the given log record.

DEPENDENCIES
   Diag log service must be initialized.

RETURN VALUE
  An unsigned int, the length

SIDE EFFECTS
  None.
===========================================================================*/
unsigned int log_get_length (void *ptr)
{
	unsigned int length = 0;
	if(ptr)
	{
		log_header_type *plog = (log_header_type *) ptr;

        if (plog)
        {
			length = plog->len;
        }
	}
	return length;
}

/*===========================================================================

FUNCTION LOG_GET_CODE

DESCRIPTION
  This function returns the log code field in the given log record.

DEPENDENCIES
   Diag log service must be initialized.

RETURN VALUE
  log_code_type, the code

SIDE EFFECTS
  None.
===========================================================================*/
log_code_type log_get_code (void *ptr)
{
	log_code_type code = 0;
	if(ptr)
	{
		log_header_type *plog = (log_header_type *) ptr;

        if (plog)
        {
			code = (log_code_type) plog->code;
        }
	}
    return code;
}

/*===========================================================================

FUNCTION LOG_STATUS

DESCRIPTION
  This function returns whether a particular code is enabled for logging.

DEPENDENCIES
   Diag log service must be initialized.

RETURN VALUE
  boolean indicating if enabled

SIDE EFFECTS
  None.
===========================================================================*/
 boolean log_status (log_code_type code)
 {
	uint32 status = FALSE;

	if (log_inited)
		status = log_status_mask (code);

     return (status != 0) ? TRUE : FALSE;
 } /* log_status */

 /*===========================================================================

FUNCTION DIAG_LSM_LOG_INIT

DESCRIPTION
  Initializes the log service

RETURN VALUE
  boolean indicating success

SIDE EFFECTS
  None.
===========================================================================*/

boolean Diag_LSM_Log_Init(void)
{
	if (log_inited)
		return TRUE;

	log_mask = malloc(LOG_MASK_SIZE);
	if (!log_mask) {
		DIAG_LOGE("diag: unable to alloc memory for log mask\n");
		return FALSE;
	}

	dci_cumulative_log_mask = malloc(DCI_LOG_MASK_SIZE);
	if (!dci_cumulative_log_mask) {
		DIAG_LOGE("diag: unable to alloc memory for dci log mask\n");
		free(log_mask);
		return FALSE;
	}

	num_dci_clients_log = 0;
	memset(dci_cumulative_log_mask, 0, DCI_LOG_MASK_SIZE);
	log_inited = TRUE;

	return TRUE;
} /* Diag_LSM_Log_Init */


/*===========================================================================
FUNCTION DIAG_LSM_LOG_DEINIT

DESCRIPTION
  Releases all resources related to logging service

RETURN VALUE
  None

SIDE EFFECTS
  None.
===========================================================================*/
void Diag_LSM_Log_DeInit(void)
{
	if (log_mask)
		free(log_mask);
	if (dci_cumulative_log_mask)
		free(dci_cumulative_log_mask);

	log_inited = FALSE;
}

/*===========================================================================

FUNCTION LOG_STATUS_MASK

DESCRIPTION
  This routine is a wrapper for log_search_mask().  It is used to look up
  the given code in the log mask.

RETURN VALUE
  A mask indicating if the specified log is enabled.

===========================================================================*/
static uint32
log_status_mask (log_code_type code)
{
	unsigned int id, item;
	boolean status = FALSE;
	uint32 status_mask = 0;

	id = LOG_GET_EQUIP_ID (code);
	item = LOG_GET_ITEM_NUM (code);
	status = log_search_mask(DIAG_DATA_TYPE_LOG, id, item);
	if (status)
		status_mask |= DIAG_DATA_TYPE_LOG;
	status = log_search_mask(DIAG_DATA_TYPE_DCI_LOG, id, item);
	if (status)
		status_mask |= DIAG_DATA_TYPE_DCI_LOG;

    return status_mask;
}

/*===========================================================================

FUNCTION log_search_mask

DESCRIPTION
  This function returns a boolean indicating TRUE if the given ID and 'item'
  denotes a valid and enabled log code from the specified log mask.

===========================================================================*/
static boolean log_search_mask (
	unsigned int log_type,
	unsigned int id,
	unsigned int item
)
{
	boolean return_val = FALSE;
	/* If valid code val */
	if (log_type == DIAG_DATA_TYPE_LOG) {
		if (log_mask_enabled(log_mask, id, item))
			return_val = TRUE;
	} else if (num_dci_clients_log > 0 && log_type == DIAG_DATA_TYPE_DCI_LOG) {
		if (log_dci_mask_enabled(dci_cumulative_log_mask, id, item))
			return_val = TRUE;
	}

	return return_val;
} /* log_search_mask */

/*===========================================================================

FUNCTION LOG_MASK_ENABLED

DESCRIPTION
  This function returns a boolean indicating if the specified code is enabled.

  The equipment ID and item code are passed in to avoid duplicating the
  calculation within this call.  It has to be done for most routines that call
  this anyways.

===========================================================================*/
static boolean log_mask_enabled(const byte *mask_ptr, unsigned int id,
				unsigned int item)
{
	unsigned int byte_index, offset;
	byte byte_mask;
	const byte *ptr = NULL;
	boolean enabled = FALSE;

	if (!mask_ptr)
		return enabled;

	ptr = mask_ptr;

	if (id >= MAX_EQUIP_ID) {
		DIAG_LOGE("diag: Invalid equip id %d in %s\n", id, __func__);
		return enabled;
	}

	/*
	 * Seek to the exact byte index in the log mask for a given
	 * equip id. Also include the offset of equip id (uint8) and the
	 * number of items in the equipment (unsigned int )
	 */
	byte_index = (item/8) + sizeof(uint8) + sizeof(unsigned int);
	byte_mask = 0x01 << (item % 8);
	offset = (id * LOG_MASK_ITEM_SIZE) + byte_index;

	if (offset > LOG_MASK_SIZE) {
		DIAG_LOGE("diag: Invalid offset %d in %s\n", offset, __func__);
		return enabled;
	}

	enabled = ((*(ptr + offset) & byte_mask) == byte_mask) ? TRUE: FALSE;
	return enabled;
} /* log_mask_enabled */

/*===========================================================================

FUNCTION LOG_DCI_MASK_ENABLED

DESCRIPTION
  This function returns a boolean indicating if the specified code is enabled.

  The equipment ID and item code are passed in to avoid duplicating the
  calculation within this call.  It has to be done for most routines that call
  this anyways.

===========================================================================*/
static boolean log_dci_mask_enabled(
	const unsigned char *mask_ptr,
	unsigned int id,
	unsigned int item
)
{
	unsigned char byte_mask;
	int byte_index, offset;
	boolean enabled = FALSE;

	if (!mask_ptr)
		return enabled;

	byte_index = item/8 + 2;
	byte_mask = 0x01 << (item % 8);
	offset = id * 514;
	if ((mask_ptr[offset + byte_index] & byte_mask) == byte_mask)
		enabled = TRUE;

	return enabled;
} /* log_dci_mask_enabled */


