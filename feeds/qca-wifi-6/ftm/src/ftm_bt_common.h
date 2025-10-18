/*==========================================================================

                     FTM BT Commom Header File

Description
  The header file includes helper enums for request_status and bt_power_state.

# Copyright (c) 2010-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
09/28/11   rrr      Common utility API abstracted,
06/18/10   rakeshk  Created a header file to hold the  helper enums for
                    request_status and bt_power_state
========================================================================*/

#ifdef  CONFIG_FTM_BT

#include "event.h"
#include "msg.h"
#include "log.h"

#include "diag_lsm.h"
#include <sys/types.h>

#ifndef __FTM_BT_COMMON_H__

#define __FTM_BT_COMMON_H__

#define TRUE 1
#define FALSE 0

/* request_status - enum to encapuslate the status of a HAL request*/
typedef enum request_status
{
  STATUS_SUCCESS,
  STATUS_FAIL,
  STATUS_NO_RESOURCES,
  STATUS_SHORT_WRITE,
  STATUS_SHORT_READ
}request_status;

/* request_status - enum to encapuslate the possible statea of BT power*/
typedef enum bt_power_state
{
  BT_OFF = 0x30, /* Its the value 0 to be input to rfkill driver */
  BT_ON = 0x31 /* ASCII value for '1'*/
}bt_power_state;

typedef enum
{
  FTM_BT_DRV_NO_ERR = 0,
  FTM_BT_DRV_CONN_TEST_FAILS,
  FTM_BT_DRV_QSOC_POWERUP_FAILS,
  FTM_BT_DRV_RX_PKT_TYPE_NOT_SUPPORTED,
  FTM_BT_DRV_SIO_OPEN_FAILS,
  FTM_BT_DRV_NO_SOC_RSP_TOUT,
  FTM_BT_DRV_BAD_NVM,
#ifdef BT_NV_SUPPORT
  FTM_BT_NV_READ_FAIL,
  FTM_BT_NV_WRITE_FAIL,
#endif
  FTM_BT_DRV_UNKNOWN_ERR
} ftm_bt_drv_err_state_type;

/*===========================================================================
FUNCTION   ftm_bt_hci_qcomm_handle_event

DESCRIPTION
  Routine called by the HAL layer reader thread to process the HCI events
  The post conditions of each event is covered in a state machine pattern

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_qcomm_handle_event();

/*===========================================================================
FUNCTION   ftm_log_send_msg

DESCRIPTION
  Processes the buffer sent and sends it to the libdiag for sending the Cmd
  response

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/


void ftm_log_send_msg(const uint8 *pEventBuf,int event_bytes);
#endif //__FTM_BT_COMMON_H__
#endif /* CONFIG_FTM_BT */
