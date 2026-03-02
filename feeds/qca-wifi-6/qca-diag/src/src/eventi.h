#ifndef EVENTI_H
#define EVENTI_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                          Event Services internal header file

General Description
  Internal declarations to support diag event service.

Initializing and Sequencing Requirements
  None

# Copyright (c) 2007-2011 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                          Edit History

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   sj      Changes for CBSP2.0
01/10/08   mad     Added copyright and file description.
12/5/07    as      Created

===========================================================================*/

#include "event_defs.h"
#include "diagdiag.h"
#include "comdef.h"

/* Size of the event mask array, which is derived from the maximum number
 of events.
*/
#define EVENT_MASK_SIZE (EVENT_LAST_ID/8 + 1)

#define EVENT_MASK_BIT_SET(id) \
  (event_mask[(id)/8] & (1 << ((id) & 0x07)))

#define EVENT_SEND_MAX 50
#define EVENT_RPT_PKT_LEN_SIZE 0x200

/* The event mask. */
/* WM7 */
unsigned char event_mask[EVENT_MASK_SIZE];
//unsigned char* event_mask;

/* NOTE: diag_event_type and event_store_type purposely use the same
   format, except that event_store type is preceeded by a Q link
   and the event ID field has a union for internal packet formatting.
   If either types are changed, the service will not function properly. */
#ifndef FEATURE_LE_DIAG
struct event_store_type
{
  uint8  cmd_code;                       /* 96 (0x60)            */
  uint16 length;                        /* Length of packet     */
  union
  {
    event_id_type event_id_field;
  }
  event_id;
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;

  event_payload_type payload;
} __attribute__((__packed__));
#else
#pragma pack(push, 1)
struct event_store_type
{
  uint8  cmd_code;                       // 96 (0x60)
  uint16 length;                        // Length of packet
  union
  {
  event_id_type event_id_field;
  }
  event_id;
  uint32 ts_lo; // Time stamp
  uint32 ts_hi;

  event_payload_type payload;
};
#pragma pack(pop)
#endif

typedef struct
{
  uint8 payload[32];
} event_large_payload_type;

#define EVENT_LARGE_PAYLOAD(X) \
     (((event_large_payload_type *)X)->payload)

/* WM7 */
#define DIAG_EVENT_MASK_SHAREMAP_NAME _T("Diag_Event_Mask_Shared")

#endif /* EVENTI_H */
