#ifndef DIAG_LSM_PKT_I_H
#define DIAG_LSM_PKT_I_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                Internal Header File for Event Packet Req/Res Service Mapping

GENERAL DESCRIPTION

Copyright (c) 2007-2011, 2013-2014, 2016 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

$Header:

when       who     what, where, why
--------   ---     ---------------------------------------------------------
02/27/08   JV     Created File
===========================================================================*/

#include "diagi.h"

typedef struct {
	diag_cmd_rsp rsp_func;
	void *rsp_func_param;
	diagpkt_rsp_type rsp;
} diagpkt_lsm_rsp_type;

boolean Diag_LSM_Pkt_Init(void);

/* clean up packet Req/Res service before exiting legacy service mapping layer.
Currently does nothing, just returns TRUE */
boolean Diag_LSM_Pkt_DeInit(void);

void diagpkt_LSM_process_request(void *req_pkt, uint16 pkt_len, int type);

/*===========================================================================

FUNCTION DIAGPKT_FREE

DESCRIPTION
  This function free the packet allocated by diagpkt_alloc(), which doesn't
  need to 'commit' for sending as a response because it is merely a temporary
  processing packet.

===========================================================================*/
  void diagpkt_free(void *pkt);

#endif /* DIAG_LSM_EVENT_I_H */
