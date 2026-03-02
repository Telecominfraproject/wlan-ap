
#ifndef DIAG_LSM_EVENT_I_H
#define DIAG_LSM_EVENT_I_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                Internal Header File for Event Legacy Service Mapping

GENERAL DESCRIPTION

Copyright (c) 2007-2011, 2014-2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

$Header:

when       who     what, where, why
--------   ---     ---------------------------------------------------------
11/26/07   mad     Created File
===========================================================================*/

/* Initializes legacy service mapping for Diag event service */
boolean Diag_LSM_Event_Init(void);

/* Deinitializes legacy service mapping for Diag event service */
void Diag_LSM_Event_DeInit(void);

/* updates the copy of event_mask */
void event_update_mask(unsigned char*, int len);

/* updates the copy of dci event_mask */
void event_update_dci_mask(unsigned char*, int len);

#endif /* DIAG_LSM_EVENT_I_H */
