/*=========================================================================
                       NFC FTM HEADER File
Description
   This file contains the definitions of the function used to check
   which chip is present on the device.

Copyright (c) 2013-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/

#ifndef _FTM_NFC
#define _FTM_NFC

#include "ftm_nfcnq.h"

#define NFC_QCA1990                 // Defnition to enable the NFC FTM inclusion

typedef enum _CHIP_TYPE{
    UNDEFINED_CHIP_TYPE     = 0,
    QTI_CHIP                = 1,
    NQ_CHIP                 = 2,
    CHIP_ERROR              = 3,
    MAXIMUM_CHIP_TYPE       = 4,
} CHIP_TYPE;

extern CHIP_TYPE chipType;

void* ftm_nfc_dispatch(ftm_nfc_pkt_type *ftm_nfc_pkt, uint16 pkt_len);

void* ftm_nfc_dispatch_qti(ftm_nfc_pkt_type *ftm_nfc_pkt, uint16 pkt_len);

void ftm_nfc_dispatch_nq_fwdl();

void ftm_nfc_dispatch_nq_test(int argc, char **argv);
#endif // _FTM_NFC
