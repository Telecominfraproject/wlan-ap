/*==========================================================================

                     FTM FM Common Header File

Description
  Global Data declarations of the ftm ant component.

# Copyright (c) 2012,2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who      what, where, why
--------   ---      ----------------------------------------------------------
05/16/2012 ankurn   Adding support for ANT+
11/28/12  c_ssugas  Adds data structures and macro for ant log event support.
===========================================================================*/

#ifdef CONFIG_FTM_ANT

#include "diagpkt.h"
#include "log.h"
#include "ftm_bt_common.h"

#include <sys/types.h>

#define APPS_RIVA_ANT_CMD_CH            "/dev/smd5"
#define APPS_RIVA_ANT_DATA_CH           "/dev/smd6"

#define FTM_ANT_CMD_CODE                94
#define OPCODE_OFFSET                   5

#define FTM_ANT_LOG_HEADER_SIZE (sizeof(ftm_ant_log_pkt_type) - 1)
#define FTM_ANT_LOG_PKT_ID 0x0D

/* FTM Log Packet - Used to send back the event of a ANT Command */
typedef PACKED struct
{
   log_hdr_type hdr;
   word         ftm_log_id;     /* FTM log id */
   byte         data[1];        /* Variable length payload,
                                 look at FTM log id for contents */
} ftm_ant_log_pkt_type;

/* Generic result, used for any command that only returns an error code */
typedef enum {
     FTM_ANT_FAIL,
     FTM_ANT_SUCCESS,
} ftm_ant_api_result_type;

typedef PACKED struct
{
    diagpkt_subsys_header_type  header ;
    char    result ;
} ftm_ant_generic_sudo_res;


/* Generic Response */
typedef PACKED struct
{
  diagpkt_subsys_header_type  header; /*Diag header*/
  uint8     evt[18];  /*allocates memory to hold longest valid event */
  char      result; /* result */
}__attribute__((packed)) ftm_ant_generic_res;

/* FTM ANT request type */
typedef PACKED struct
{
  diagpkt_cmd_code_type              cmd_code;
  diagpkt_subsys_id_type             subsys_id;
  diagpkt_subsys_cmd_code_type       subsys_cmd_code;
  uint8                              cmd_id; /* command id (required) */
  uint8                              cmd_data_len;
  byte                               data[1];
}__attribute__((packed))ftm_ant_pkt_type;



/*===========================================================================
FUNCTION   ftm_ant_dispatch

DESCRIPTION
  Dispatch routine for the various ANT commands. Copies the data into
  a global union data structure before calling the processing routine

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the FTM ANT packet

SIDE EFFECTS
  None

===========================================================================*/

void * ftm_ant_dispatch(ftm_ant_pkt_type *ftm_ant_pkt, uint16 length );

/*===========================================================================
FUNCTION   ftm_ant_qcomm_handle_event

DESCRIPTION
  Handler for the various ANT Events received. Sends data as log packets
  using diag to upper layers.
DEPENDENCIES
  NIL

RETURN VALUE
  Status value TRUE if event received successfuly
  otherwise returns status value FALSE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_ant_qcomm_handle_event ();

#endif /* CONFIG_FTM_ANT */
