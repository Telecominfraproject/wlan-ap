#ifndef EVENT_H
#define EVENT_H

/*===========================================================================

                   Event Reporting Services

General Description
  All declarations and definitions necessary to support the static
  system event reporting service.

# Copyright (c) 2007-2011 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
===========================================================================*/

/*===========================================================================
                          Edit History

$Header: //depot/asic/msmshared/services/diag/Diag_1.5/Diag_LSM/event.h#1 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
01/02/08   mad     Added extern "C" modifiers for function declarations
05/14/04   eav     Added FEATURE_SAVE_DEBUG_TRACE.  Added the function
                   event_save_circ_buffer_to_efs, which is called from err.c
04/17/01   lad     Moved constants to diagtune.h.
                   Moved event definitions to event_defs.h.
02/23/01   lad     Updated API for core diagnostics service.
11/17/00   jal     Bit fields in event_id_type came out in inverted order
                   from what we wanted.  Fixed.
11/13/00   lcc     Added event and type definitions for some events.
11/10/00   lad     Made obsolete event_report_data() a macro to NULL.
11/09/00   jal     Took the old event IDs out (again!)
11/07/00   jal     Renamed event_extra_data_type to event_payload_type
10/17/00   jal     Changes for new event accumulation mechanism
05/15/00   lad     Changed truncated timsteamp from uint32 to uint16.
04/11/00   lad     Increased # of customer reserved event IDs to 0x100.
12/16/99   lad     Added support for event reporting service.
01/21/99   lad     Created file.

===========================================================================*/

/* Since the IDs and type definitions are part of the API, include it here */
#include "event_defs.h"

/* -------------------------------------------------------------------------
   Function Defintions
   ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif
/*===========================================================================

FUNCTION EVENT_REPORT

DESCRIPTION
  Report an event.

DEPENDENCIES
  Event services must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void event_report (event_id_enum_type event_id);

/*===========================================================================

FUNCTION EVENT_REPORT_PAYLOAD

DESCRIPTION
  Report an event with payload data.

DEPENDENCIES
  Event services must be initialized.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
void event_report_payload (event_id_enum_type event_id, uint8 length, void *data);


#ifdef __cplusplus
}
#endif /* for extern "C" modifier */
#endif         /* EVENT_H */
