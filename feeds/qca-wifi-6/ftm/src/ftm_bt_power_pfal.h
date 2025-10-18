/*==========================================================================

                     FTM BT POWER PFAL Header File

Description
   PFAL API declarations of the ftm bt power pfal component.

# Copyright (c) 2010 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/18/10   rakeshk  Created a header file to hold the PFAL declarations for
                    BT power programming
07/07/10   rakeshk  Modified the function name of BT power set PFAL routine
===========================================================================*/
#include "ftm_bt_common.h"

#ifndef __FTM_BT_POWER_PFAL_H__
#define __FTM_BT_POWER_PFAL_H__

/*===========================================================================
FUNCTION   ftm_bt_power_pfal_set

DESCRIPTION
  Platform dependent interface API which sets the BT power
  and returns the status of the toggle operation.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_power_pfal_set(bt_power_state state);

/*===========================================================================
FUNCTION   ftm_bt_power_pfal_check

DESCRIPTION

  Platform dependent interface API which intiates a BT power read/check
  and returns the current state of the BT HW.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  Current BT power state

SIDE EFFECTS
  None

===========================================================================*/
bt_power_state ftm_bt_power_pfal_check();

#endif

