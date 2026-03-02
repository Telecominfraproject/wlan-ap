/*==========================================================================

                     FTM BT HCI PFAL Header File

Description
   PFAL API declarations of the ftm bt hci pfal component.

# Copyright (c) 2010 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/18/10   rakeshk  Created a header file to hold the PFAL declarations for
                    HCI UART programming
===========================================================================*/
#include "ftm_bt_common.h"

#ifndef __FTM_BT_HCI_PFAL_H__
#define __FTM_BT_HCI_PFAL_H__

#define PIN_CON_CMD_OGF               0xFC
#define PIN_CON_CMD_OCF               0x0C
#define PIN_CON_CMD_SUB_OP            0x38
#define PIN_CON_INTERFACE_ID          0x01
#define PIN_CON_EVENT_LEN             0x06
#define EXT_PIN_CON_LEN               0x02

#define PIN_CON_CMD_OCF_BIT           0x01
#define PIN_CON_CMD_OGF_BIT           0x02
#define PIN_CON_CMD_SUBOP_BIT         0x04
#define PIN_CON_CMD_INTER_BIT         0x05

#define PIN_CON_EVT_OGF_BIT           0x05
#define PIN_CON_EVT_OCF_BIT           0x04
#define PIN_CON_EVT_SUB_OP_BIT        0x07
#define PIN_CON_INTERFACE_ID_EVT_BIT  0x08
#define PIN_CON_EVENT_LEN_BIT         0x02
#define PIN_CON_EVT_STATUS_BIT        0x06

#define LOG_TAG "ftmdaemon"

#define PRI_INFO " I"
#define PRI_WARN " W"
#define PRI_ERROR " E"
#define PRI_DEBUG " D"
#define PRI_VERB " V"

#define ALOG(pri, tag, fmt, arg...) fprintf(stderr, tag pri ": " fmt"\n", ##arg)
#define ALOGV(fmt, arg...) ALOG(PRI_VERB, LOG_TAG, fmt, ##arg)
#define ALOGD(fmt, arg...) ALOG(PRI_DEBUG, LOG_TAG, fmt, ##arg)
#define ALOGI(fmt, arg...) ALOG(PRI_INFO, LOG_TAG, fmt, ##arg)
#define ALOGW(fmt, arg...) ALOG(PRI_WARN, LOG_TAG, fmt, ##arg)
#define ALOGE(fmt, arg...) ALOG(PRI_ERROR, LOG_TAG, fmt, ##arg)

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_set_transport

DESCRIPTION
 sets the type of transport based on the msm type

DEPENDENCIES
  NIL

RETURN VALUE
returns the type of transport

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_pfal_set_transport(void);

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_deinit_transport

DESCRIPTION
  Platform specific routine to de-intialise the UART/SMD resource.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_deinit_transport();

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_init_transport

DESCRIPTION
  Platform specific routine to intialise the UART/SMD resources.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_init_transport ();

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_nwrite

DESCRIPTION
  Platform specific routine to write the data in the argument to the UART/SMD
  port intialised.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_nwrite(uint8 *buf, int size);

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_nread

DESCRIPTION
  Platform specific routine to read data from the UART/SMD port intialised into
  the buffer passed in argument.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_nread(uint8 *buf, int size);

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_changebaudrate

DESCRIPTION
  Platform specific routine to intiate a change in baud rate

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  TRUE if SUCCESS, else FALSE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_pfal_changebaudrate (uint32 new_baud);

#endif //__FTM_BT_HCI_PFAL_H__
