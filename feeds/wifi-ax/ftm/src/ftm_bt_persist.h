#ifndef _FTM_BT_PERSIST_H_
#define _FTM_BT_PERSIST_H_

/*==========================================================================

                     BT persist NV items access source file

Description
  Read/Write APIs for retreiving NV items from persist memory.

# Copyright (c) 2011 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
09/27/11   rrr     Moved persist related API for c/c++ compatibility, needed
                   for random BD address to be persistent across target
                   reboots.
==========================================================================*/

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "ftm_bt_common.h"
#include <string.h>


#ifdef BT_NV_SUPPORT

#define FTM_BT_CMD_NV_READ 0xB
#define FTM_BT_CMD_NV_WRITE 0xC

const uint8 nv_read_response_size = 8;
const uint8 nv_read_response_size_fail = 2;
const uint8 nv_write_response_size = 2;

/* NV Write Responses */
const uint8 event_buf_bt_nv_write_pass[2] = { FTM_BT_CMD_NV_WRITE, FTM_BT_DRV_NO_ERR};
const uint8 event_buf_bt_nv_write_fail[2] = { FTM_BT_CMD_NV_WRITE, FTM_BT_NV_WRITE_FAIL};

/* NV Read Responses */
const uint8 event_buf_nv_read_response_fail[8] =
{
  FTM_BT_CMD_NV_READ, FTM_BT_NV_READ_FAIL, 0x0, 0x0,0x0,0x0,0x0,0x0
};

uint8 event_buf_nv_read_response[8];
#endif /* BT_NV_SUPPORT */

/*===========================================================================
FUNCTION   ftm_bt_send_nv_read_cmd

DESCRIPTION
 Helper Routine to process the nv read command

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_send_nv_read_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
);

/*===========================================================================
FUNCTION   ftm_bt_send_nv_write_cmd

DESCRIPTION
 Helper Routine to process the nv write command

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_send_nv_write_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
);

#ifdef __cplusplus
}
#endif

#endif /* _FTM_BT_PERSIST_H_ */

