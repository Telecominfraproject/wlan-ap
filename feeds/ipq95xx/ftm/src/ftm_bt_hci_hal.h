/*==========================================================================

                     FTM BT HCI PFAL Header File

Description
   Warpper API definitions of the ftm bt hci hal component.

# Copyright (c) 2010 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/18/10   rakeshk  Created a header file to hold the wrapper HAL
                    definitions for HCI UART control
===========================================================================*/

#include "ftm_bt_common.h"
#include "ftm_bt_hci_pfal.h"
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_set_transport

DESCRIPTION
  sets the type of transport based on the msm type

DEPENDENCIES
  NIL

RETURN VALUE
returns the type of transport

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_hal_set_transport()
{
   return ftm_bt_hci_pfal_set_transport();
}
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_deinit_transport

DESCRIPTION
  Platform independent wrapper API which intiatea a De-intialise of UART/SMD
  resources with PFAL layer and returns the status of the PFAL operation

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_hal_deinit_transport()
{
  return ftm_bt_hci_pfal_deinit_transport();
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_init_transport

DESCRIPTION
  Platform independent wrapper API which intiatea a intialise of UART/SMD
  resources with PFAL layer and returns the status of the PFAL operation

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_hal_init_transport (int mode)
{
  return ftm_bt_hci_pfal_init_transport(mode);
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_nwrite

DESCRIPTION
  Platform independent wrapper API which intiates a write operation
  with the PFAL layer and returns the status of the PFAL operation.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_hal_nwrite(uint8 *buf, int size)
{
  return ftm_bt_hci_pfal_nwrite(buf,size);

}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_nread

DESCRIPTION
  Platform independent wrapper API which intiates a read operation
  with the PFAL layer and returns the status of the PFAL operation.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_hal_nread(uint8 *buf, int size)
{
  return ftm_bt_hci_pfal_nread(buf,size);
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_changebaudrate

DESCRIPTION
  Platform independent wrapper API which intiatea a UART baud rate change
  with the PFAL layer and returns the status of the PFAL request.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  TRUE if SUCCESS, else FAIL

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_hal_changebaudrate (uint32 new_baud)
{
  return ftm_bt_hci_pfal_changebaudrate(new_baud);
}

