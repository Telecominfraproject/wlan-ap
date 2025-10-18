#ifndef DIAG_LSM_LOG_I_H
#define DIAG_LSM_LOG_I_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

EXTENDED DIAGNOSTIC LOG LEGACY SERVICE MAPPING HEADER FILE
(INTERNAL ONLY)

GENERAL DESCRIPTION

  All the declarations and definitions necessary to support the reporting
  of messages.  This includes support for the
  extended capabilities as well as the legacy messaging scheme.

Copyright (c) 2007-2011,2014 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

$Header:

when       who     what, where, why
--------   ---     ----------------------------------------------------------
11/26/07   JV      Created File
===========================================================================*/

typedef PACK(struct) {
	uint8 equip_id;
	unsigned int num_items;
} diag_log_mask_update_t;

#define LOG_ITEMS_TO_SIZE(num_items)	((num_items + 7) / 8)

/* Initializes legacy service mapping for Diag log service */
boolean Diag_LSM_Log_Init(void);

/* Releases all resources related to Diag Log Service */
void Diag_LSM_Log_DeInit(void);

/* updates the copy of log_mask */
void log_update_mask(unsigned char *, int len);

/* updates the copy of the dci log_mask */
void log_update_dci_mask(unsigned char*, int len);

#endif /* DIAG_LSM_LOG_I_H */
