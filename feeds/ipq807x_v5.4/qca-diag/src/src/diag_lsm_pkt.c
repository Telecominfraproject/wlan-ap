/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

              Legacy Service Mapping layer implementation for Packet request/response

GENERAL DESCRIPTION
  Contains main implementation of Legacy Service Mapping layer for Diagnostic Packet Req/Res Services.

EXTERNALIZED FUNCTIONS
  diagpkt_alloc
  diagpkt_subsys_alloc
  diagpkt_shorten
  diagpkt_commit
  diagpkt_get_cmd_code
  diagpkt_set_cmd_code
  diagpkt_subsys_get_id
  diagpkt_subsys_get_cmd_code
  diagpkt_err_rsp
  diagpkt_subsys_alloc_v2
  diagpkt_subsys_alloc_v2_delay
  diagpkt_delay_commit
  diagpkt_subsys_get_status
  diagpkt_subsys_set_status
  diagpkt_subsys_get_delayed_rsp_id
  diagpkt_subsys_reset_delayed_rsp_id
  diagpkt_subsys_set_rsp_cnt

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2007-2011, 2013-2015, 2016 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
10/01/08   SJ    Changes for CBSP2.0
04/14/08   JV    Added support to pass a pointer to a locally created object as
                 an argument to IDiagPkt_BindPkt()
04/14/08   JV    Replaced KxMutex lock and unlock with ICritSect enter and leave
02/27/08   JV    Created

===========================================================================*/


/* ==========================================================================
   Include Files
========================================================================== */
#include "stdio.h"
#include "comdef.h"
#include "diagpkt.h"
#include "diag_lsmi.h"
#include "diagdiag.h"
#include "diagcmd.h"
#include "diag.h" /* For definition of diag_cmd_rsp */
#include "diag_lsm_pkt_i.h"
#include "diagi.h"
#include "diag_shared_i.h" /* data structures for registration */
#include "diagsvc_malloc.h"

#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include "errno.h"
#include "../include/diag_lsm.h"
#include "diag_lsm_dci.h"
#include "diag_lsm_dci_i.h"

#define std_strlprintf			snprintf
#define DIAGPKT_HDR_PATTERN		(0xDEADD00DU)
#define DIAGPKT_OVERRUN_PATTERN		(0xDEADU)
#define DIAGPKT_USER_TBL_SIZE		128
#define DIAGPKT_MAX_DELAYED_RSP		0xFFFF
#define DIAGPKT_PKT2LSMITEM(p)	\
	((diagpkt_lsm_rsp_type *) (((byte *) p) - FPOS (diagpkt_lsm_rsp_type, rsp.pkt)))

static unsigned int gPkt_commit_fail = 0;
static byte user_tbl_inited = FALSE;
static diag_dci_delayed_rsp_tbl_t dci_delayed_rsp_tbl_head;
static pthread_mutex_t dci_delayed_rsp_tbl_mutex;
static diagpkt_user_table_type *diagpkt_user_table[DIAGPKT_USER_TBL_SIZE];
static int pkt_type;
static int dci_tag;
static pthread_mutex_t user_tbl_mutex;
/*
 * delayed_rsp_id 0 represents no delay in the response. Any other
 * number means that the diag packet has a delayed response.*
 */
static uint16 delayed_rsp_id = 1;

typedef struct {
	uint8 command_code;
} diagpkt_hdr_type;

typedef struct {
	uint8 command_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
} diagpkt_subsys_hdr_type;

typedef struct {
	uint8 command_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint32 status;
	uint16 delayed_rsp_id;
	uint16 rsp_cnt; /* 0, means one response and 1, means two responses */
} diagpkt_subsys_hdr_type_v2;

/*=============================================================================
FUNCTION DIAG_DCI_ADD_DELAYED_RSP

DESCRIPTION
Internal function.
Adds the Delayed response ID and the packet tag to an
internal table for lookup

DEPENDENCIES
DCI

RETURN VALUE
None

SIDE EFFECTS
None
=============================================================================*/
static void diag_dci_add_delayed_rsp(diagpkt_subsys_delayed_rsp_id_type id, int tag)
{
	diag_dci_delayed_rsp_tbl_t *temp = NULL, *head_ptr = NULL;
	diag_dci_delayed_rsp_tbl_t *new_entry = (diag_dci_delayed_rsp_tbl_t *)malloc(sizeof(diag_dci_delayed_rsp_tbl_t));
	if (!new_entry) {
		DIAG_LOGE("diag: Unable to allocate memory for DCI delayed resonse table entry\n");
		return;
	}

	new_entry->delayed_rsp_id = id;
	new_entry->dci_tag = tag;
	head_ptr = &dci_delayed_rsp_tbl_head;
	pthread_mutex_lock(&dci_delayed_rsp_tbl_mutex);
	temp = head_ptr->prev;
	head_ptr->prev = new_entry;
	new_entry->next = head_ptr;
	new_entry->prev = temp;
	temp->next = new_entry;
	pthread_mutex_unlock(&dci_delayed_rsp_tbl_mutex);
}

/*=============================================================================
FUNCTION DIAG_DCI_DELAYED_RSP_ID_PRESENT

DESCRIPTION
Internal function.
Looks up the internal DCI delayed response table for a
particular delayed response ID and returns the
corresponding tag.

DEPENDENCIES
DCI

RETURN VALUE
DCI Request Tag

SIDE EFFECTS
None
=============================================================================*/
static diag_dci_delayed_rsp_tbl_t *diag_dci_delayed_rsp_id_present(diagpkt_subsys_delayed_rsp_id_type id)
{
	diag_dci_delayed_rsp_tbl_t *walk_ptr = NULL, *head_ptr = NULL;

	head_ptr = &dci_delayed_rsp_tbl_head;
	for (walk_ptr = head_ptr->next; walk_ptr && walk_ptr != head_ptr; walk_ptr = walk_ptr->next)
		if (walk_ptr->delayed_rsp_id == id)
			return walk_ptr;

	return NULL;
}

/*=============================================================================
FUNCTION DIAG_DCI_DELETE_DELAYED_RSP_ENTRY

DESCRIPTION
Internal function.
Deletes a particular DCI delayed response entry from the table.

DEPENDENCIES
DCI

RETURN VALUE
None

SIDE EFFECTS
None
=============================================================================*/
static void diag_dci_delete_delayed_rsp_entry(diag_dci_delayed_rsp_tbl_t *entry)
{
	if (!entry || !entry->next || !entry->prev)
		return;

	pthread_mutex_lock(&dci_delayed_rsp_tbl_mutex);
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	pthread_mutex_unlock(&dci_delayed_rsp_tbl_mutex);
	free(entry);
}

/*=============================================================================
FUNCTION DIAGPKT_GET_DELAYED_RSP_ID_LSM

DESCRIPTION
Internal function.
Issues an IOCTL to the diag driver and gets the next delayed response id in the system

RETURN VALUE
0 if it fails to retrieve the next delayed response id. Valid value otherwise.

DEPENDENCIES
Diag driver should be initialized.
=============================================================================*/
static uint16 diagpkt_get_delayed_rsp_id_lsm(void)
{
	uint16 delayed_rsp_id = 0;
	int err = 0;

	err = ioctl(diag_fd, DIAG_IOCTL_GET_DELAYED_RSP_ID, &delayed_rsp_id, 0, NULL, 0, NULL, NULL);
	if (err) {
		delayed_rsp_id = 0;
		DIAG_LOGE("diag: Unable to retrieve new delayed response id, err: %d, errno: %d\n", err, errno);
	}

	return delayed_rsp_id;
}

/*===========================================================================

FUNCTION DIAGPKT_WITH_DELAY

DESCRIPTION
  This procedure checks if the diagnostic packet has been registered with or
  without delay.

DEPENDENCIES
  None.

RETURN VALUE
  Return value is 0 if Diag packet has no delayed response and 1 if Diag
      packet has delayed response

SIDE EFFECTS
  None



===========================================================================*/
static uint32 diagpkt_with_delay (diagpkt_subsys_id_type id,
				  diagpkt_subsys_cmd_code_type code)
{
	uint16 packet_id = code;      /* Command code for std or subsystem */
    uint8 subsys_id = id;
    const diagpkt_user_table_type *user_tbl_entry = NULL;
    const diagpkt_user_table_entry_type *tbl_entry = NULL;
    int tbl_entry_count = 0;
    int i, j;
    boolean found = FALSE;
    uint32 delay_flag = 0;

  /* Search the dispatch table for a matching subsystem ID.  If the
     subsystem ID matches, search that table for an entry for the given
     command code. */
    for (i = 0; !found && i < DIAGPKT_USER_TBL_SIZE; i++)
    {
		user_tbl_entry = diagpkt_user_table[i];
		if (user_tbl_entry != NULL && user_tbl_entry->subsysid == subsys_id)
        {
			tbl_entry = user_tbl_entry->user_table;
			delay_flag = user_tbl_entry->delay_flag;
			tbl_entry_count = (tbl_entry) ? user_tbl_entry->count : 0;

            for (j = 0; (tbl_entry!=NULL) && !found && j < tbl_entry_count; j++)
            {
				if (packet_id >= tbl_entry->cmd_code_lo &&
                       packet_id <= tbl_entry->cmd_code_hi)
                {
                /* If the entry has no func, ignore it. */
                    found = TRUE;
                }
                tbl_entry++;
            }
        } /* endif if (user_tbl_entry != NULL && user_tbl_entry->subsysid == subsys_id) */
    }
	return delay_flag;
}               /* diagpkt_with_delay */


/*===========================================================================

FUNCTION DIAGPKT_DELAY_ALLOC

DESCRIPTION
  This function allocates the specified amount of space in a pre-malloced buffer.

DEPENDENCIES
  None.

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
static void *diagpkt_delay_alloc(diagpkt_cmd_code_type code, unsigned int length)
{
  void *ptr = NULL;
  unsigned int size = 0;
   diagpkt_subsys_hdr_type_v2 *pkt = NULL;

  size = DIAG_DEL_RESP_REST_OF_DATA_POS + length;


  /* We allocate from the memory pool used for events, logs and messages
  because it is a delayed response. */
  ptr = DiagSvc_Malloc (size, GEN_SVC_ID);
  if (NULL != ptr)
   {
     diag_data_delayed_response* pdiag_data = (diag_data_delayed_response*) ptr;
     pdiag_data->length = length;
     pdiag_data->diagdata.diag_data_type = DIAG_DATA_TYPE_DELAYED_RESPONSE;
     pkt = (diagpkt_subsys_hdr_type_v2*) ((byte*)(pdiag_data)+DIAG_DEL_RESP_REST_OF_DATA_POS);
     pkt->command_code = code;
  }
  else
  {
	  /* Alloc not successful.  Return NULL. DiagSvc_Malloc() allocates memory
	  from client's heap using a malloc call if the pre-malloced buffers are not available.
	  So if this fails, it means that the client is out of heap. */

	DIAG_LOGE(" diagpkt_delay_alloc: DiagSvc_Malloc Failed");
  }
  return pkt;
} /* diagpkt_delay_alloc */


/*===========================================================================

FUNCTION DIAGPKT_LSM_PROCESS_REQUEST

DESCRIPTION
  This procedure formats a response packet in response to a request
  packet received from the Diagnostic Monitor.  Calls packet processing
  functions through a table, counts input and output types

DEPENDENCIES
  None.

RETURN VALUE
  None

SIDE EFFECTS
  None
===========================================================================*/

void diagpkt_LSM_process_request(void *req_pkt, uint16 pkt_len, int type)
{
	uint16 packet_id;
	uint8 subsys_id = DIAGPKT_NO_SUBSYS_ID;
	const diagpkt_user_table_type *user_tbl_entry = NULL;
	const diagpkt_user_table_entry_type *tbl_entry = NULL;
	int tbl_entry_count = 0;
	int i, j;
	void *rsp_pkt = NULL;
	boolean found = FALSE;
	uint16 cmd_code = 0xFF;
	void *request_ptr = NULL;
	request_ptr = req_pkt;
	if (!request_ptr) {
		DIAG_LOGE("diag: Invalid request packet in %s\n", __func__);
		return;
	}
	if (type != PKT_TYPE && type != DCI_PKT_TYPE) {
		DIAG_LOGE("diag: Invalid packet type %d, in %s\n", type, __func__);
		return;
	}
	pkt_type = (type == PKT_TYPE) ? DIAG_DATA_TYPE_RESPONSE : type;
	if (type == DCI_PKT_TYPE) {
		if (pkt_len < (sizeof(diag_dci_pkt_header_t) + sizeof(uint8))) {
			DIAG_LOGE("diag: In %s, invalid pkt_len for DCI request: %d\n", __func__, pkt_len);
			return;
		}
		diag_dci_pkt_header_t *dci_header;
		dci_header = (diag_dci_pkt_header_t *)request_ptr;
		dci_tag = dci_header->tag;
		request_ptr = (unsigned char *)request_ptr + sizeof(diag_dci_pkt_header_t);
		pkt_len -= (sizeof(diag_dci_pkt_header_t) + sizeof(uint8));
	}
	packet_id = diagpkt_get_cmd_code(request_ptr);
	if (packet_id == DIAG_SUBSYS_CMD_VER_2_F)
		cmd_code = packet_id;
	if ((packet_id == DIAG_SUBSYS_CMD_F) || (packet_id == DIAG_SUBSYS_CMD_VER_2_F)) {
		subsys_id = diagpkt_subsys_get_id(request_ptr);
		packet_id = diagpkt_subsys_get_cmd_code(request_ptr);
	}

	/*
	 * Search the dispatch table for a matching subsystem ID.  If the
	 * subsystem ID matches, search that table for an entry for the given
	 * command code.
	 */
	for (i = 0; !found && i < DIAGPKT_USER_TBL_SIZE; i++) {
		user_tbl_entry = diagpkt_user_table[i];
		if (user_tbl_entry != NULL && user_tbl_entry->subsysid == subsys_id && user_tbl_entry->cmd_code == cmd_code) {
			tbl_entry = user_tbl_entry->user_table;
			tbl_entry_count = (tbl_entry) ? user_tbl_entry->count : 0;
			for (j = 0; (tbl_entry!=NULL) && !found && j < tbl_entry_count; j++) {
				if (packet_id >= tbl_entry->cmd_code_lo && packet_id <= tbl_entry->cmd_code_hi) {
					found = TRUE;
					if (tbl_entry->func_ptr) {
						rsp_pkt = (void *) (*tbl_entry->func_ptr) (request_ptr, pkt_len);
						if (rsp_pkt) {
							/*
							 * The most common case: response is returned.  Go ahead
							 * and commit it here.
							 */
							 diagpkt_commit(rsp_pkt);
						}
					}
				}
				tbl_entry++;
			}
		}
	}

	/* Assume that rsp and rsp_pkt are NULL if !found */
	if (!found)
		DIAG_LOGE("diag: In %s, Did not find match in user table.\n", __func__);
	return;
}

/* ==========================================================================
FUNCTION
DIAGPKT_USER_TBL_INIT

DESCRIPTION
  Registers the table given to the diagpkt user table

DEPENDENCIES
  None.

RETURN VALUE
  None

SIDE EFFECTS
  None
============================================================================= */
static void diagpkt_user_tbl_init(void)
{
	int i = 0;
	if (user_tbl_inited)
		return;

	pthread_mutex_init(&user_tbl_mutex, NULL);
	for (i = 0; i < DIAGPKT_USER_TBL_SIZE; i++)
		diagpkt_user_table[i] = NULL;
	user_tbl_inited = TRUE;
}
/*===========================================================================
FUNCTION   Diag_LSM_Pkt_Init

DESCRIPTION
  Initializes the Packet Req/Res service.

DEPENDENCIES
  None.

RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean Diag_LSM_Pkt_Init(void)
{
	diag_dci_delayed_rsp_tbl_t *head_ptr = &dci_delayed_rsp_tbl_head;
	head_ptr->delayed_rsp_id = 0;
	head_ptr->dci_tag = 0;
	head_ptr->next = head_ptr;
	head_ptr->prev = head_ptr;
	pthread_mutex_init(&dci_delayed_rsp_tbl_mutex, NULL);
	return TRUE;
} /* Diag_LSM_Pkt_Init */

/*===========================================================================

FUNCTION    Diag_LSM_Pkt_DeInit

DESCRIPTION
  De-Initialize the Diag Packet Req/Res service.

DEPENDENCIES
  None.

RETURN VALUE
  boolean: returns TRUE; currently does nothing.

SIDE EFFECTS
  None

===========================================================================*/
boolean Diag_LSM_Pkt_DeInit(void)
{
	int i = 0;
	int client_id = gdwClientID;
	boolean ret = TRUE;
	diag_dci_delayed_rsp_tbl_t *walk_ptr = NULL, *head_ptr = NULL;

	if (ioctl(diag_fd, DIAG_IOCTL_COMMAND_DEREG, (void*)&client_id, sizeof(client_id), NULL, 0, NULL, NULL) != 0) {
		DIAG_LOGE(" Diag_LSM_Pkt_DeInit: DeviceIOControl failed, Error = %d\n.", errno);
		ret = FALSE;
	}

	/* free the entries in user table */
	for (i = 0; i < DIAGPKT_USER_TBL_SIZE; i++) {
		if (diagpkt_user_table[i] != NULL) {
			free(diagpkt_user_table[i]);
			diagpkt_user_table[i] = NULL;
		}
		else
			break;
	}

	head_ptr = &dci_delayed_rsp_tbl_head;
	/* Remove entries from the DCI delayed response table */
	for (walk_ptr = head_ptr->next; walk_ptr && walk_ptr != head_ptr; walk_ptr = walk_ptr->next)
		diag_dci_delete_delayed_rsp_entry(walk_ptr);

	return ret;
} /* Diag_LSM_Pkt_DeInit */


/* ==========================================================================*/
/* Externalized functions */

/* Do not call this function directly. Use the macros defined in diagpkt.h. */
/* ==========================================================================
FUNCTION DIAGPKT_TBL_REG

DESCRIPTION
   Registers the table given to the diagpkt user table

DEPENDENCIES
  None.

RETURN VALUE
  None

SIDE EFFECTS
  None
=============================================================================*/
void diagpkt_tbl_reg(const diagpkt_user_table_type *tbl_ptr)
{
	int i;
	word num_entries = 0;
	diag_cmd_reg_entry_t *entries = NULL;
	diag_cmd_reg_tbl_t reg_tbl;

	if (-1 == diag_fd) {
		DIAG_LOGE("diag: In %s, service not initialized.\n", __func__);
		return;
	}

	if (!tbl_ptr) {
		DIAG_LOGE("diag: In %s, invalid input\n", __func__);
		return;
	}

	num_entries = tbl_ptr->count;
	entries = (diag_cmd_reg_entry_t *)malloc(sizeof(diag_cmd_reg_entry_t) * num_entries);
	if (!entries) {
		DIAG_LOGE("diag: In %s, unable to create temporary memory for registration\n", __func__);
		return;
	}

	/* Make sure this is initialized */
	diagpkt_user_tbl_init();
	pthread_mutex_lock(&user_tbl_mutex);
	for (i = 0; i < DIAGPKT_USER_TBL_SIZE; i++) {
		if (diagpkt_user_table[i] != NULL)
			continue;
		diagpkt_user_table[i] = (diagpkt_user_table_type *)malloc(sizeof(diagpkt_user_table_type));
		if (!diagpkt_user_table[i]) {
			DIAG_LOGE("diag: In %s, unable to allocate entry in the table\n", __func__);
			pthread_mutex_unlock(&user_tbl_mutex);
			free(entries);
			return;
		}
		memcpy(diagpkt_user_table[i], tbl_ptr, sizeof(diagpkt_user_table_type));
		break;
	}
	pthread_mutex_unlock(&user_tbl_mutex);

	reg_tbl.count = num_entries;
	(void)std_strlprintf(reg_tbl.sync_obj_name, MAX_SYNC_OBJ_NAME_SIZE,
			     "%s%d", DIAG_LSM_PKT_EVENT_PREFIX, gdwClientID);
	for (i = 0; i < num_entries; i++) {
		entries[i].cmd_code = tbl_ptr->cmd_code;
		entries[i].subsys_id = tbl_ptr->subsysid;
		entries[i].cmd_code_lo = tbl_ptr->user_table[i].cmd_code_lo;
		entries[i].cmd_code_hi = tbl_ptr->user_table[i].cmd_code_hi;
	}
	reg_tbl.entries = entries;
	if (ioctl(diag_fd, DIAG_IOCTL_COMMAND_REG, &reg_tbl, sizeof(reg_tbl), NULL, 0, NULL, NULL))
		DIAG_LOGE("diag: Unable to register commands with the driver, error: %d\n", errno);
	free(entries);
}

/*===========================================================================

FUNCTION DIAGPKT_GET_CMD_CODE

DESCRIPTION
  This function returns the command code in the specified diag packet.

DEPENDENCIES
  None.

RETURN VALUE
  cmd_code

SIDE EFFECTS
  None

===========================================================================*/
diagpkt_cmd_code_type
diagpkt_get_cmd_code (void *ptr)
{
	diagpkt_cmd_code_type cmd_code = 0;
	if(ptr)
	{
		/* Diag command codes are the first byte */
        return *((diagpkt_cmd_code_type *) ptr);
	}
	return cmd_code;
}               /* diag_get_cmd_code */


/*===========================================================================

FUNCTION DIAGPKT_SET_CMD_CODE

DESCRIPTION
  This function sets the command code in the specified diag packet.

DEPENDENCIES
  None.

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void
diagpkt_set_cmd_code (void *ptr, diagpkt_cmd_code_type cmd_code)
{
	if(ptr)
	{
		*((diagpkt_cmd_code_type *) ptr) = cmd_code;
	}
}               /* diagpkt_set_cmd_code */



/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_GET_ID

DESCRIPTION
  This function returns the subsystem ID in the specified diag packet.


DEPENDENCIES
  None.

RETURN VALUE
  subsys_id. If the packet is not a DIAG_SUBSYS_CMD_F or DIAG_SUBSYS_CMD_VER_2_F packet,
  0xFF is returned.

SIDE EFFECTS
  None

===========================================================================*/
diagpkt_subsys_id_type
diagpkt_subsys_get_id (void *ptr)
{
	diagpkt_subsys_id_type id = 0;
	if (ptr)
	{
		diagpkt_subsys_hdr_type *pkt_ptr = (void *) ptr;

        if ((pkt_ptr->command_code == DIAG_SUBSYS_CMD_F) || (pkt_ptr->command_code
                      == DIAG_SUBSYS_CMD_VER_2_F))
        {
		    id = (diagpkt_subsys_id_type) pkt_ptr->subsys_id;
        }
        else
        {
		    id = 0xFF;
        }
	}
    return id;
}               /* diagpkt_subsys_get_id */

/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_GET_CMD_CODE

DESCRIPTION
  This function returns the subsystem command code in the specified
  diag packet.

DEPENDENCIES
  None.

RETURN VALUE
  subsys_cmd_code. If the packet is not a DIAG_SUBSYS_CMD_F or DIAG_SUBSYS_CMD_VER_2_F packet,
  0xFFFF is returned.

SIDE EFFECTS
  None
===========================================================================*/
diagpkt_subsys_cmd_code_type
diagpkt_subsys_get_cmd_code (void *ptr)
{
	diagpkt_subsys_cmd_code_type code = 0;
	if(ptr)
	{
		diagpkt_subsys_hdr_type *pkt_ptr = (void *) ptr;

        if ((pkt_ptr->command_code == DIAG_SUBSYS_CMD_F) || (pkt_ptr->command_code
            == DIAG_SUBSYS_CMD_VER_2_F))
        {
		    code = pkt_ptr->subsys_cmd_code;
        }
        else
        {
            code = 0xFFFF;
		}
	}
	return code;
}               /* diagpkt_subsys_get_cmd_code */


/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_GET_STATUS

DESCRIPTION
  This function gets the status field in the DIAG_SUBSYS_CMD_VER_2_F packet

DEPENDENCIES
  This function's first argument (ptr) should always be DIAG_SUBSYS_CMD_VER_2_F
  packet.

RETURN VALUE
  status

SIDE EFFECTS
  None

===========================================================================*/
diagpkt_subsys_status_type
diagpkt_subsys_get_status (void *ptr)
{
  diagpkt_subsys_hdr_type_v2 *pkt_ptr = (void *) ptr;

  assert (pkt_ptr != NULL);
  assert (pkt_ptr->command_code == DIAG_SUBSYS_CMD_VER_2_F);

  return pkt_ptr->status;
}               /* diagpkt_subsys_get_status */


/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_SET_STATUS

DESCRIPTION
  This function sets the status field in the DIAG_SUBSYS_CMD_VER_2_F packet.

DEPENDENCIES
  This function's first argument (ptr) should always be DIAG_SUBSYS_CMD_VER_2_F
  packet.

RETURN VALUE
  None

SIDE EFFECTS
  None
===========================================================================*/
void
diagpkt_subsys_set_status (void *ptr, diagpkt_subsys_status_type status)
{
  diagpkt_subsys_hdr_type_v2 *pkt_ptr = (void *) ptr;

  assert (pkt_ptr != NULL);
  assert (pkt_ptr->command_code == DIAG_SUBSYS_CMD_VER_2_F);

  pkt_ptr->status = status;
}               /* diagpkt_subsys_set_status */


/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_GET_DELAYED_RSP_ID

DESCRIPTION
  This function gets the delayed response ID field in the
  DIAG_SUBSYS_CMD_VER_2_F packet.

DEPENDENCIES
  This function's first argument (ptr) should always be DIAG_SUBSYS_CMD_VER_2_F
  packet.

RETURN VALUE
  delayed response ID

SIDE EFFECTS
  None
===========================================================================*/
diagpkt_subsys_delayed_rsp_id_type
diagpkt_subsys_get_delayed_rsp_id (void *ptr)
{
  diagpkt_subsys_hdr_type_v2 *pkt_ptr = (void *) ptr;

  assert (pkt_ptr != NULL);
  assert (pkt_ptr->command_code == DIAG_SUBSYS_CMD_VER_2_F);

  return (pkt_ptr) ? pkt_ptr->delayed_rsp_id : 0;
}               /* diagpkt_subsys_get_delayed_rsp_id */


/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_RESET_DELAYED_RSP_ID

DESCRIPTION
  This function sets the delayed response ID to zero in the
  DIAG_SUBSYS_CMD_VER_2_F packet.

DEPENDENCIES
  This function's first argument (ptr) should always be DIAG_SUBSYS_CMD_VER_2_F
  packet.

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void
diagpkt_subsys_reset_delayed_rsp_id (void *ptr)
{
  diagpkt_subsys_hdr_type_v2 *pkt_ptr = (void *) ptr;

  assert (pkt_ptr != NULL);
  assert (pkt_ptr->command_code == DIAG_SUBSYS_CMD_VER_2_F);

  pkt_ptr->delayed_rsp_id = 0;
}               /* diagpkt_subsys_reset_delayed_rsp_id */


/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_SET_RSP_CNT

DESCRIPTION
  This function sets the response count in the DIAG_SUBSYS_CMD_VER_2_F packet.

DEPENDENCIES
  This function's first argument (ptr) should always be DIAG_SUBSYS_CMD_VER_2_F
  packet.

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void
diagpkt_subsys_set_rsp_cnt (void *ptr, diagpkt_subsys_rsp_cnt rsp_cnt)
{
  diagpkt_subsys_hdr_type_v2 *pkt_ptr = (void *) ptr;

  assert (pkt_ptr != NULL);
  assert (pkt_ptr->command_code == DIAG_SUBSYS_CMD_VER_2_F);

  pkt_ptr->rsp_cnt = rsp_cnt;
}               /* diagpkt_subsys_set_rsp_cnt */


/*============================================================================
FUNCTION DIAGPKT_ALLOC

DESCRIPTION
  This function allocates the specified amount of space from a pre-malloced buffer.
  If space is unavailable in the pre-malloced buffer, then a malloc is done.

DEPENDENCIES
  diagpkt_commit() must be called to commit the response packet to be sent.
  Not calling diagpkt_commit() will result in a memory leak.

RETURN VALUE
  pointer to the allocated memory

SIDE EFFECTS
  None

============================================================================*/


void *
diagpkt_alloc (diagpkt_cmd_code_type code, unsigned int length)
{
    diagpkt_lsm_rsp_type *item = NULL;
    diagpkt_hdr_type *pkt = NULL;
    uint16 *pattern = NULL;    /* Overrun pattern. */
    unsigned char *p;
    diag_data* pdiag_data = NULL;
    unsigned int size = 0;

	 if(-1 == diag_fd)
     {
         return NULL;
     }

     size = DIAG_REST_OF_DATA_POS + FPOS (diagpkt_lsm_rsp_type, rsp.pkt) + length + sizeof (uint16);
     if (size > DIAGSVC_MALLOC_PKT_ITEM_SIZE) {
	     DIAG_LOGE("diag: In %s, invalid len: %d, max length: %d\n",
		       __func__, size, DIAGSVC_MALLOC_PKT_ITEM_SIZE);
	     return NULL;
     }

    /*-----------------------------------------------
      Try to allocate a buffer.  Size of buffer must
      include space for overhead and CRC at the end.
    -----------------------------------------------*/
      pdiag_data = (diag_data*)DiagSvc_Malloc (size, PKT_SVC_ID);
      if(NULL == pdiag_data)
      {
         /* Alloc not successful.  Return NULL. DiagSvc_Malloc() allocates memory
	  from client's heap using a malloc call if the pre-malloced buffers are not available.
	  So if this fails, it means that the client is out of heap. */
         return NULL;
      }
      /* Fill in the fact that this is a response */
      pdiag_data->diag_data_type = DIAG_DATA_TYPE_RESPONSE;
      // WM7 prototyping: advance the pointer now
      item = (diagpkt_lsm_rsp_type*)((byte*)(pdiag_data)+DIAG_REST_OF_DATA_POS);

    /* This pattern is written to verify pointers elsewhere in this
       service  are valid. */
    item->rsp.pattern = DIAGPKT_HDR_PATTERN;    /* Sanity check pattern */

    /* length ==  size unless packet is resized later */
    item->rsp.size = length;
    item->rsp.length = length;

    pattern = (uint16 *) &item->rsp.pkt[length];

    /* We need this to meet alignment requirements - MATS */
    p = (unsigned char *) pattern;
    p[0] = (DIAGPKT_OVERRUN_PATTERN >> 8) & 0xff;
    p[1] = (DIAGPKT_OVERRUN_PATTERN >> 0) & 0xff;

    pkt = (diagpkt_hdr_type *) & item->rsp.pkt;

    if (pkt)
    {
        pkt->command_code = code;
    }
    return (void *) pkt;

}               /* diagpkt_alloc */

/*===========================================================================

FUNCTION DIAGPKT_DELAY_COMMIT

DESCRIPTION
  This function commits the response.

DEPENDENCIES
  None

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void diagpkt_delay_commit (void *pkt)
{
	if (!pkt)
		return;

	unsigned int pkt_len = 0;
	diag_data_delayed_response* pdiag_del_rsp_data = NULL;
	diag_data* pdiag_data = NULL;
	diagpkt_subsys_delayed_rsp_id_type delayed_rsp_id;
	int is_dci_pkt = 0, num_bytes = 0;
	uint32 dci_pkt_type = DCI_PKT_TYPE, dci_cmd_code = DCI_DELAYED_RSP_CODE;
	uint32 rsp_len = 0, rsp_index = 0;
	unsigned char *temp = NULL;
	diagpkt_subsys_rsp_cnt rsp_count = ((diagpkt_subsys_hdr_type_v2 *)pkt)->rsp_cnt;
	diag_dci_delayed_rsp_tbl_t *delayed_rsp_dci_entry = NULL;

	delayed_rsp_id = diagpkt_subsys_get_delayed_rsp_id(pkt);
	/* Check if it is a DCI packet */
	delayed_rsp_dci_entry = diag_dci_delayed_rsp_id_present(delayed_rsp_id);
	if (delayed_rsp_dci_entry)
		is_dci_pkt = 1;

	pdiag_del_rsp_data = (diag_data_delayed_response*)((byte *)(pkt) - DIAG_DEL_RESP_REST_OF_DATA_POS);
	pkt_len = pdiag_del_rsp_data->length;
	/*
	 * We don't need to Write the "length" field in pdiag_del_rsp_data to DCM,
	 * so strip that out to get the diag_data from diag_data_delayed_response.
	 */
	pdiag_data = (diag_data*)((byte*)pdiag_del_rsp_data + sizeof(pdiag_del_rsp_data->length));
	if (pkt_len > 0 && -1 != diag_fd) {
		/* This is a DCI packet. Write DCI headers in front of the packet */
		if (is_dci_pkt) {
			rsp_len = sizeof(uint32) + sizeof(uint32) + sizeof(int) + pkt_len;
			temp = (unsigned char*)DiagSvc_Malloc(rsp_len, PKT_SVC_ID);
			if (!temp) {
				DIAG_LOGE("diag: In %s Could not allocate memory\n", __func__);
				DiagSvc_Free(pdiag_del_rsp_data, GEN_SVC_ID);
				return;
			}
			memcpy(temp, &dci_pkt_type, sizeof(uint32));
			rsp_index += sizeof(uint32);
			memcpy(temp + rsp_index, &dci_cmd_code, sizeof(uint32));
			rsp_index += sizeof(uint32);
			memcpy(temp + rsp_index, &delayed_rsp_dci_entry->dci_tag, sizeof(int));
			rsp_index += sizeof(int);
			memcpy(temp + rsp_index, (void *)((byte *)(pdiag_data) + sizeof(int)), pkt_len);
			num_bytes = write(diag_fd, temp, rsp_len);
			DiagSvc_Free(temp, PKT_SVC_ID);
			/* If it is the last response in the delayed response sequence, delete the entry from table */
			if (rsp_count > 0 && rsp_count < 0x1000)
				diag_dci_delete_delayed_rsp_entry(delayed_rsp_dci_entry);
		} else {
			num_bytes = write(diag_fd, (const void*)pdiag_data, pkt_len + DIAG_REST_OF_DATA_POS);
		}
		if(num_bytes != 0) {
			DIAG_LOGE("Diag_LSM_Pkt: Write failed in %s, bytes written: %d, error: %d\n",
						__func__, num_bytes, errno);
			gPkt_commit_fail++;
		}
	}
	DiagSvc_Free(pdiag_del_rsp_data, GEN_SVC_ID);
}

/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_ALLOC

DESCRIPTION
  This function returns the command code in the specified diag packet.

DEPENDENCIES
  None

RETURN VALUE
  Pointer to allocated memory

SIDE EFFECTS
  None

===========================================================================*/
void *
diagpkt_subsys_alloc (diagpkt_subsys_id_type id,
              diagpkt_subsys_cmd_code_type code, unsigned int length)
{
  diagpkt_subsys_hdr_type *hdr = NULL;
  if(-1 == diag_fd)
  {
     return NULL;
  }

  hdr = (diagpkt_subsys_hdr_type *) diagpkt_alloc (DIAG_SUBSYS_CMD_F, length);

  if( hdr != NULL )
  {
      hdr->subsys_id = id;
      hdr->subsys_cmd_code = code;

  }

  return (void *) hdr;

}               /* diagpkt_subsys_alloc */


/*===========================================================================
FUNCTION DIAGPKT_SUBSYS_ALLOC_V2

DESCRIPTION
  This function allocates the specified amount of space from a pre-malloced buffer.
  If space is unavailable in the pre-malloced buffer, then a malloc is done.

DEPENDENCIES
  diagpkt_commit() must be called to commit the response packet to be sent.
  Not calling diagpkt_commit() will result in a memory leak.

RETURN VALUE
  pointer to the allocated memory

SIDE EFFECTS
  None

============================================================================*/

void *diagpkt_subsys_alloc_v2(diagpkt_subsys_id_type id,
				     diagpkt_subsys_cmd_code_type code, unsigned int length)
{
	diagpkt_subsys_hdr_type_v2 *hdr = NULL;
	if(diag_fd == DIAG_INVALID_HANDLE)
		return NULL;

	hdr = (diagpkt_subsys_hdr_type_v2 *)diagpkt_alloc(DIAG_SUBSYS_CMD_VER_2_F, length);
	if (hdr == NULL)
		return NULL;

	hdr->subsys_id = id;
	hdr->subsys_cmd_code = code;
	hdr->status = 0;
	if (diagpkt_with_delay(id, code)) {
		hdr->delayed_rsp_id = diagpkt_get_delayed_rsp_id_lsm();
		if (!hdr->delayed_rsp_id) {
			diagpkt_lsm_rsp_type *item = DIAGPKT_PKT2LSMITEM(hdr);
			diag_data* pdiag_data = (diag_data*)((byte*)(item) - DIAG_REST_OF_DATA_POS);
			DiagSvc_Free(pdiag_data,PKT_SVC_ID);
			return NULL;
		}
	} else {
		hdr->delayed_rsp_id = 0;
	}
	hdr->rsp_cnt = 0;
	return (void *) hdr;
}

/*===========================================================================

FUNCTION DIAGPKT_SUBSYS_ALLOC_V2_DELAY

DESCRIPTION
  This function allocates the specified amount of space from a pre-malloced buffer.
  If space is unavailable in the pre-malloced buffer, then a malloc is done.This
  function is used to send a delayed response.This response has same priority as
  F3 messages and logs.

DEPENDENCIES
  diagpkt_delay_commit() must be called to commit the response packet to be
  sent. Not calling diagpkt_delay_commit() will result in a memory leak and
  response packet will not be sent.

  Note:User is required to provide delayed response id as an argument.
       This helps tools to match the delayed response with the original
       request response pair.

RETURN VALUE
  pointer to the allocated memory

SIDE EFFECTS
  None

===========================================================================*/
void *
diagpkt_subsys_alloc_v2_delay (diagpkt_subsys_id_type id,
              diagpkt_subsys_cmd_code_type code,
              diagpkt_subsys_delayed_rsp_id_type delayed_rsp_id_arg,
              unsigned int length)
{
  diagpkt_subsys_hdr_type_v2 *hdr = NULL;
   if(-1 == diag_fd)
   {
       return NULL;
   }

  hdr = (diagpkt_subsys_hdr_type_v2 *) diagpkt_delay_alloc(
                 DIAG_SUBSYS_CMD_VER_2_F,
                 length);

  if(hdr != NULL)
  {
      hdr->subsys_id = id;
      hdr->subsys_cmd_code = code;
      hdr->status = 0;
      hdr->delayed_rsp_id = delayed_rsp_id_arg;
      hdr->rsp_cnt = 1;
  }
  return (void *) hdr;
}               /* diagpkt_subsys_alloc_v2_delay */


/*===========================================================================

FUNCTION DIAGPKT_SHORTEN

DESCRIPTION
  In legacy diag, this function was used to shorten a previously allocated
  response buffer. Now, since we use pre-malloced buffers, this function will
  not serve the purpose of freeing any memory. It just updates the length
  field with the new length.

DEPENDENCIES
  None

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/

void
diagpkt_shorten (void *pkt, unsigned int new_length)
{

  diagpkt_lsm_rsp_type *item = NULL;
  uint16 *pattern = NULL;

  if (pkt)
  {
    /* Do pointer arithmetic in bytes, then case to q_type; */
      item = DIAGPKT_PKT2LSMITEM (pkt);

      if (new_length < item->rsp.size)
      {
		  unsigned char *p;
          item->rsp.length = new_length;

      /* Write the new buffer overrun detection pattern */
          pattern = (uint16 *) & item->rsp.pkt[new_length];

      /* We need this to meet alignment requirements - MATS */
          p = (unsigned char *) pattern;
          p[0] = (DIAGPKT_OVERRUN_PATTERN >> 8) & 0xff;
          p[1] = (DIAGPKT_OVERRUN_PATTERN >> 0) & 0xff;
      }
      else
      {
		  DIAG_LOGE(" diagpkt_shorten: diagpkt_shorten Failed");
		  return;
      }
  }
  return;
}               /* diagpkt_shorten */
/*===========================================================================

FUNCTION DIAGPKT_COMMIT

DESCRIPTION
  This function commits previously allocated space in the diagnostics output
  buffer.

DEPENDENCIES
  None

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/

void diagpkt_commit (void *pkt)
{
	if (!pkt)
		return;

	unsigned char *temp = NULL;
	int ret;
	unsigned int rsp_length = 0;
	unsigned int rsp_index = 0;
	uint8 dci_cmd_code = DCI_PKT_RSP_CODE;
	uint8 dci_cmd_code_len = sizeof(uint8);
	diagpkt_cmd_code_type packet_id;

	diagpkt_lsm_rsp_type *item = DIAGPKT_PKT2LSMITEM(pkt);
	item->rsp_func = NULL;
	item->rsp_func_param = NULL;

	if (item->rsp.length <= 0)
		goto err;
	if (-1 == diag_fd)
		goto err;
	rsp_length = (int)(item->rsp.length);
	if (pkt_type == DIAG_DATA_TYPE_DCI_PKT) {
		packet_id = diagpkt_get_cmd_code(pkt);
		if (packet_id == DIAG_SUBSYS_CMD_VER_2_F) {
			dci_cmd_code_len = sizeof(int);
			dci_cmd_code = DCI_DELAYED_RSP_CODE;
			diag_dci_add_delayed_rsp(diagpkt_subsys_get_delayed_rsp_id(pkt), dci_tag);
		}
		rsp_length += sizeof(uint32) + dci_cmd_code_len;
	}
	temp = (unsigned char*)DiagSvc_Malloc((int)DIAG_REST_OF_DATA_POS + rsp_length, PKT_SVC_ID);
	if (!temp) {
		DIAG_LOGE("diag: In %s Could not allocate memory\n", __func__);
		goto err;
	}

	memcpy(temp, (unsigned char*)&pkt_type, sizeof(pkt_type));
	rsp_index += DIAG_REST_OF_DATA_POS;
	if (pkt_type == DIAG_DATA_TYPE_DCI_PKT) {
		memcpy(temp + rsp_index, &(dci_cmd_code), sizeof(uint8));
		rsp_index += dci_cmd_code_len;
		memcpy(temp + rsp_index, &(dci_tag), sizeof(dci_tag));
		rsp_index += sizeof(uint32);
	}
	memcpy(temp + rsp_index, pkt, item->rsp.length);
	if ((ret = write(diag_fd, (const void*) temp, DIAG_REST_OF_DATA_POS + rsp_length)) != 0) {
		DIAG_LOGE("Diag_LSM_Pkt: Write failed in %s, bytes written: %d, error: %d\n", __func__, ret, errno);
		gPkt_commit_fail++;
	}
	DiagSvc_Free(temp, PKT_SVC_ID);
err:
	/* Free the original response */
	diagpkt_free(pkt);
}

/*===========================================================================

FUNCTION DIAGPKT_ERR_RSP

DESCRIPTION
  This function generates an error response packet.

DEPENDENCIES
  None

RETURN VALUE
  pointer to the error response

SIDE EFFECTS
  None

===========================================================================*/

void *
diagpkt_err_rsp (diagpkt_cmd_code_type code,
         void *req_pkt, uint16 req_len)
{
  DIAG_BAD_CMD_F_rsp_type *rsp;
  const unsigned int rsp_len = MIN (sizeof (DIAG_BAD_CMD_F_rsp_type),
               req_len + FPOS (DIAG_BAD_CMD_F_rsp_type, pkt));
  rsp = (DIAG_BAD_CMD_F_rsp_type *) diagpkt_alloc (code, rsp_len);

  if (!rsp) {
	  DIAG_LOGE("rsp pointer is null");
	  return NULL;
  }

  if(req_pkt)
  {
    memcpy ((void *) rsp->pkt,
            (void *) req_pkt,
            rsp_len - FPOS (DIAG_BAD_CMD_F_rsp_type, pkt));
  }
  else if (req_len != 0)
  {
      //MSG_HIGH("Non-0 request length (%d) and NULL request pointer!",req_len,0,0);
     DIAG_LOGE("Non-0 request length (%d) and NULL request pointer!",
								req_len);
  }

  return ((void *) rsp);
}               /* diagkt_err_rsp */

/*=========================================================================
FUNCTION DIAGPKT_FREE

DESCRIPTION

  This function free the packet allocated by diagpkt_alloc(), which doesn't

  need to 'commit' for sending as a response if it is merely a temporary

  processing packet.

===========================================================================*/

void

diagpkt_free(void *pkt)

{
  if (pkt)
  {
    byte *item = (byte*)DIAGPKT_PKT2LSMITEM(pkt);
    item -= DIAG_REST_OF_DATA_POS;
    DiagSvc_Free ((void *)item,PKT_SVC_ID);
  }
 return;
}
























