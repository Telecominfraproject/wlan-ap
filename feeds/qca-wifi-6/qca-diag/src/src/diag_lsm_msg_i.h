#ifndef DIAG_LSM_MSG_I_H
#define DIAG_LSM_MSG_I_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

EXTENDED DIAGNOSTIC MESSAGE SERVICE LEGACY MAPPING
INTERNAL HEADER FILE

GENERAL DESCRIPTION
Internal header file

Copyright (c) 2007-2011, 2013-2015 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

$Header:

when       who     what, where, why
--------   ---     ----------------------------------------------------------
12/03/07   mad     Created File
===========================================================================*/

#include "msg.h"
#include "msgcfg.h"

#define MAX_SSID_PER_RANGE	200

typedef PACK(struct) {
	uint32 ssid_first;
	uint32 ssid_last;
	uint32 ptr[MAX_SSID_PER_RANGE];
} diag_msg_mask_t;

typedef PACK(struct) {
	uint32_t ssid_first;
	uint32_t ssid_last;
	uint32_t range;
} diag_msg_mask_update_t;

/*
 * Each row contains First (uint32_t), Last (uint32_t) values along with the
 * range of SSIDs (MAX_SSID_PER_RANGE * uint32_t). And there are
 * MSG_MASK_TBL_CNT rows.
 */
#define MSG_MASK_SIZE		(sizeof(diag_msg_mask_t) * MSG_MASK_TBL_CNT)

unsigned char msg_mask[MSG_MASK_SIZE];

/* Initializes Mapping layer for message service*/
boolean Diag_LSM_Msg_Init(void);

/* clean up before exiting legacy service mapping layer.
Does nothing as of now, just returns TRUE. */
void Diag_LSM_Msg_DeInit(void);

/* updates the copy of the run-time masks for messages */
void msg_update_mask(unsigned char *ptr, int len);

#endif /* DIAG_LSM_MSG_I_H */
