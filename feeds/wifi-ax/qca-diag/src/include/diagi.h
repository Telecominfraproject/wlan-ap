#ifndef DIAGI_H
#define DIAGI_H
/*==========================================================================

                 Diagnostic Subsystem Internal Header File

Description
  Shared declarations and prototypes internal to the Diagnostic subsystem.

# Copyright (c) 2007-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
===========================================================================*/


/*===========================================================================

                            Edit History

$Header: //depot/asic/msmshared/services/diag/DIAG_7K/diagi.h#1 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   sj      Changes for CBSP2.0
11/21/06   as      Moved DIAG internal features from diagi.h to diag.h
08/28/06   as      Added win mobile featurization support.
03/28/06   pc      Changed DIAG_TX_APP_SIG from 0x00800000 to 0x00080000.
10/19/05   as      Modified diag_searchlist_type to make it 4-byte aligned.
07/05/05   sl      Added support for SPC_TIMEOUT to double the timeout value
                   on consecutive incorrect SPCs.
06/16/05   as      New signal for DM to communicate completion.
05/17/05   as      Added new signal for Application processor data.
10/24/01   jal     New signal for SIO to indicate the UART is flushed.
                   New signal for SIO to indicate port closure is complete.
08/20/01   jal     Supported more Diag packets.  Added NV cache typedef,
                   prototypes for diag_set_sp_state(), downloader status.
06/27/01   lad     Various non-functional changes to facilitate update of
                   log services.
04/06/01   lad     Added definitions of DIAG_TASK_xxx sigs to decouple from 
                   task.h.
                   ASYNC con<F2st definitions moved to diagcomm.h?
                   Externalized msg_nice[] and log_nice[] for streaming config 
                   support.
                   Moved prototype of diag_kick_watchdog() to diagtarget.h.
                   Added prototype of diagpkt_process_request() (moved from 
                   diagpkt.h).
                   Updated prototype for diag_do_escaping().
                   Removed prototype for diag_do_unescaping().
                   Removed references to diagpkt_refresh_cache().
02/23/01   lad     Rearchitected the diagnostics subsystem to be a service 
                   rather than a module.  All coupling to targets has been
                   moved to target-specific implementations.  The only coupling
                   that remains are common services and REX.
                   Old history comments have been removed since most of them
                   are no longer applicable.

===========================================================================*/

#include "diag.h"
#include "diagpkt.h"
#include "log.h"
#include <string.h>

/* -------------------------------------------------------------------------
** Diag Task Signal Definitions
** ------------------------------------------------------------------------- */
// Added this for CBSP2.0
typedef enum { /* begin feature_query_enum_type */
	 FEATURE_QUERY_ENUM_LENGTH /* used to determine mask size */
} feature_query_enum_type;


/* The shortest length of the mask to cover all entries in the enum */
#define FEATURE_MASK_LENGTH \
  ((FEATURE_QUERY_ENUM_LENGTH / 8)+1)


/* This specifies the last equipment ID for use in logging services.  This is a
 * 4-bit value, so it cannot exceed 15. */
#define LOG_EQUIP_ID_LAST LOG_EQUIP_ID_LAST_DEFAULT


/* -------------------------------------------------------------------------
** Externalized data members
** ------------------------------------------------------------------------- */



/* The following is used by the packet service to store rsp packets. */
typedef struct {
  unsigned int pattern;     /* Pattern to check validity of committed pointers. */
  unsigned int size;        /* Size of usable buffer (diagpkt_q_type->pkt) */
  unsigned int length;      /* Size of packet */
  byte pkt[DIAG_MAX_RX_PKT_SIZ];	/* Sized by 'length' field. */
} diagpkt_rsp_type;

#endif /* DIAGI_H */
