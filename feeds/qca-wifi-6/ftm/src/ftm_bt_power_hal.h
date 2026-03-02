/*==========================================================================

                     FTM BT POWER HAL Header File

Description
   Wrapper API definitions of the ftm bt power hal component.

# Copyright (c) 2010 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/18/10   rakeshk  Created a header file to include the wrapper API
                    definitions for BT power control
07/07/10   rakeshk  Modified the function name of BT power set HAL routine
===========================================================================*/
#include "ftm_bt_common.h"
#include "ftm_bt_power_pfal.h"


#ifndef __FTM_BT_POWER_HAL_H__
#define __FTM_BT_POWER_HAL_H__
/*===========================================================================
FUNCTION   ftm_bt_power_hal_set

DESCRIPTION
  Platform independent wrapper API which sets a BT power  from PFAL
  layer and returns the status of the PFAL operation.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_power_hal_set(bt_power_state state)
{
  return ftm_bt_power_pfal_set(state);
}
/*===========================================================================
FtUNCTION    ftm_bt_power_hal_check

DESCRIPTION

  Platform independent wrapper API which gets the BT power from PFAL
  layer and returns the current state of the BT HW.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  Current BT power state

SIDE EFFECTS
  None

===========================================================================*/
bt_power_state ftm_bt_power_hal_check()
{
  return ftm_bt_power_pfal_check();
}

#endif //__FTM_BT_POWER_HAL_H__
