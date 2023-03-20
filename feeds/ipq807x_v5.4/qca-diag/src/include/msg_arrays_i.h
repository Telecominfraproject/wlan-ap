#ifndef MSG_ARRAYS_I_H
#define MSG_ARRAYS_I_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
         
        DIAGNOSTIC MESSAGE SERVICE: ARRAYS FOR BUILD/RUN-TIME MASKS


GENERAL DESCRIPTION
   Internal Header File. Contains structures/definitions for message masks.
  
Copyright (c) 2009 by Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

  $Header: //source/qcom/qct/core/services/diag/main/latest/diag_mask_tbl/src/msg_arrays_i.h#1 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
11/10/09   sg      Moved MSG_MASK_TBL_CNT to msgcfg.h
09/29/09   mad     Created from msgcfg.h.
===========================================================================*/

#include "comdef.h"

/*---------------------------------------------------------------------------
  This is the structure used for both the build mask array and the RT mask
  array.  It consists of a range of IDs and a pointer to an array indexed
  as follows:
  
  index:
  SSID - ssid_start
  
  Total # of entries:
  ssid_last - ssid_first + 1;
---------------------------------------------------------------------------*/
typedef struct
{
  uint16 ssid_first;      /* Start of range of supported SSIDs */
  uint16 ssid_last;       /* Last SSID in range */

  /* Array of (ssid_last - ssid_first + 1) masks */
  const uint32 *bt_mask_array;
  uint32 *rt_mask_array;
}
msg_mask_tbl_type;

extern const msg_mask_tbl_type msg_mask_tbl[];
/* Number of SSID range entries in table */

#endif /* MSG_ARRAYS_I_H */
