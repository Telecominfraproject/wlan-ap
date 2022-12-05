/*=========================================================================
                       NFC FTM C File
Description
   This file contains the definitions of the function used to check
   which chip is present on the device.

Copyright (c) 2013-2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/
/*===========================================================================
                         Edit History
when        who              what,              where,     why
--------   ---     ----------------------------------------------------------
===========================================================================*/

#include "ftm_nfc.h"

CHIP_TYPE chipType = UNDEFINED_CHIP_TYPE;

/*=========================================================================
FUNCTION   checkChip

DESCRIPTION
  Checks whether it can open the NQ Kernel, if not, it means
  the device has a QTI chip.

PARAMETERS
  None

RETURN VALUE
  void

===========================================================================*/
void checkChip( void )
{
    int ret = 0;

    ret = ftm_nq_nfc_open( );                   // can you open the NQ Kernel?

    if( ret > 0 )                               // yes
    {
        printf( "%s: NQ CHIP \n", __func__ );
        chipType = NQ_CHIP;                     // so it's an NQ Chip

        ret = ftm_nq_nfc_close( );              // close the handle
        if( ret != 0 )                          // not successful?
        {
            printf( "%s: Could not close the File Handle for NQ Chip \n", __func__ );
            chipType = CHIP_ERROR;              // something is wrong
        }
    }
    else
    {
        printf( "%s: QTI CHIP \n", __func__ );
        chipType = QTI_CHIP;
    }
}

/*=========================================================================
FUNCTION   ftm_nfc_dispatch

DESCRIPTION
  Dispatches QRCT commands and Chip Replies/Notifications/Data
  to the required FTM NFC Chip Handler

PARAMETERS
  ftm_nfc_pkt_type *nfc_ftm_pkt - FTM Packet
  uint16            pkt_len     - FTM Packet Length

RETURN VALUE
  void *

===========================================================================*/
void* ftm_nfc_dispatch( ftm_nfc_pkt_type *nfc_ftm_pkt, uint16 pkt_len )
{
    ftm_nfc_pkt_type *reply = NULL;

    if( UNDEFINED_CHIP_TYPE == chipType )
    {
        printf( "%s: Checking Chip Type \n", __func__ );
        checkChip( );
    }

    switch( chipType )
    {
        case NQ_CHIP:
            if( nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id == FTM_NFC_REQ_CHIP_TYPE )
                reply = PrepareRsp( nfc_ftm_pkt );
            else
                reply = ftm_nfc_dispatch_nq( nfc_ftm_pkt, pkt_len );
            break;

        case QTI_CHIP:
            if( nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id == FTM_NFC_REQ_CHIP_TYPE )
                reply = PrepareRsp( nfc_ftm_pkt );
            else
                reply = ftm_nfc_dispatch_qti( nfc_ftm_pkt, pkt_len );
            break;

        default:
            printf( "%s: ERROR - THIS SHOULD HAVE NEVER BEEN REACHED, CHIP TYPE %d", __func__, chipType );
            break;
    }

    return reply;
}
