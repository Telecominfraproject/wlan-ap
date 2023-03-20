#ifndef DIAG_H
#define DIAG_H
/*==========================================================================

                     Diagnostic Task Header File

Description
  Global Data declarations of the diag_task.

# Copyright (c) 2007-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.===========================================================================*/


/*===========================================================================

                         Edit History

      $Header: //depot/asic/msmshared/services/diag/DIAG_7K/diag.h#5 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
12/22/06   as      Moved proc ID macros to diag.h
12/05/06   as      Added signal for Diag drain timer
11/21/06   as      Moved DIAG internal features from diagi.h to diag.h
03/30/05   sl      Added support for SPC_UNLOCK_TTL to unlock/lock the sp_state
10/17/03   ph      For Rel A builds, redirect to new MC subsystem command.
09/23/03    gr     Function prototypes related to changes that enable more
                   efficient detection of the condition where diag is out of
                   heap space.
08/20/01   jal     Changes to support more Diag packets.  Support for powerup
                   online/offline state, service programming lock state.
04/06/01   lad     Cosmetic changes.
02/23/01   lad     Rearchitected the diagnostics subsystem to be a service
                   rather than a module.  All coupling to targets has been
                   moved to target-specific implementations.  This file now
                   contains an API.  No other information or coupling remains
                   except for featurized legacy code that impacts existing
                   targets.
                   Old history comments have been removed since most of them
                   are no longer applicable.

===========================================================================*/

/* -------------------------------------------------------------------------
** Definitions and Declarations
** ------------------------------------------------------------------------- */
/* Diagnostics version (protocol revision).
*/
/* DIAG_DIAGVER
   Diagnostics version, used to ensure compatibility of the DM and the DMSS.
   1   Original
   2   Changes to status packet and verno packet. Removed verstr packet
   3   for release to factory. Sends RF mode in status packet
   4   Adds release directory to verno packet and dipswitch packets
   5   Many changes in DM.
   6   Added Vocoder PCM and PKT loopback, and Analog IS-55 tests
   7   New FER data in tagraph packet
   8   Streaming protocol enhancements and event reporting.
*/
#ifdef FEATURE_DIAG_NON_STREAMING
#define DIAG_DIAGVER 7
#else
#define DIAG_DIAGVER 8
#endif

/* -------------------------------------------------------------------------
** Diag Internal features
** ------------------------------------------------------------------------- */
/* Group all standalone features */
#if (defined(FEATURE_STANDALONE) || defined(FEATURE_STANDALONE_APPS) || \
     defined(FEATURE_STANDALONE_MODEM))
  #define DIAG_STANDALONE
#endif

/* Multi processor Diag */
#if defined(FEATURE_MULTIPROCESSOR) && !(defined(DIAG_STANDALONE))
 #define DIAG_MP
#endif

#define DIAG_MODEM_PROC 0
#define DIAG_APP_PROC  1
#define DIAG_DUAL_PROC 2

#if defined (DIAG_MP)
  #if defined (FEATURE_DIAG_MP_MASTER_MODEM)
  /* Master DIAG is on Modem Processor */
     #if defined (IMAGE_MODEM_PROC)
      #define DIAG_MP_MASTER IMAGE_MODEM_PROC
      #undef DIAG_MP_SLAVE
    #endif
    #if defined (IMAGE_APPS_PROC)
      #define DIAG_MP_SLAVE IMAGE_APPS_PROC
      #undef DIAG_MP_MASTER
    #endif
    #define DIAGPKT_REQ_FWD_PROC DIAG_APP_PROC
  #elif defined (FEATURE_DIAG_MP_MASTER_APPS)
  /* Master DIAG is on Apps Processor */
    #if defined (IMAGE_APPS_PROC)
      #define DIAG_MP_MASTER IMAGE_APPS_PROC
      #undef DIAG_MP_SLAVE
      /* Mobile View code */
      #define DIAG_MV_CMD_RPC
    #endif
    #if defined (IMAGE_MODEM_PROC)
      #define DIAG_MP_SLAVE IMAGE_MODEM_PROC
      #undef DIAG_MP_MASTER
    #endif
    #define DIAGPKT_REQ_FWD_PROC DIAG_MODEM_PROC
  #elif defined (FEATURE_DIAG_MP_MODEM_ONLY)
    /* Should be same as single processor DIAG_SINGLE_PROC */
  #else
    #error "Select where master diag should be"
  #endif
#else
    /* Should be same as single processor DIAG_SINGLE_PROC*/

#endif


/* Runtime Device Map port selection for diag */
#if defined (FEATURE_RUNTIME_DEVMAP)
#if defined (DIAG_MP)
  #if defined (DIAG_MP_MASTER)|| defined(FEATURE_DIAG_MP_MODEM_ONLY)
    #define DIAG_RUNTIME_DEVMAP
  #endif
#else
   #define DIAG_RUNTIME_DEVMAP
#endif
#endif

#if defined DIAG_MP
  #if defined (DIAG_MP_MASTER)
    #define DIAG_REQ_FWD   /* Fowards requests to the slave proc */
    #define DIAG_NO_DSM_CHAINING
    #define DIAG_FRAMING
    #define DIAG_SIO_USB
  #elif defined (DIAG_MP_SLAVE)
    #define DIAG_RSP_SEND /* sends response */
  #endif

  #if defined (FEATURE_DIAG_MP_MODEM_ONLY)
    #define DIAG_NO_DSM_CHAINING
    #define DIAG_FRAMING
    #define DIAG_RSP_SEND /* sends response */
    #undef DIAG_REQ_FWD
  #endif

#else  /*DIAG_MP */
/* Single processor or standalone builds */
    #define DIAG_NO_DSM_CHAINING
    #define DIAG_FRAMING
    #define DIAG_RSP_SEND /* sends response */
#endif /*DIAG_MP*/


/* Error cases */
#if defined (DIAG_MP) && !defined(FEATURE_SMD)
  #error "Error: FEATURE_SND is required for DIAG_MP"
#endif
#if defined (FEATURE_DIAG_MP_MODEM_ONLY) &&  \
    !(defined (DIAG_MP) || defined (IMAGE_MODEM_PROC))
  #error "Error: Invalid configuration for DIAG MP"
#endif

#if defined (DIAG_MP_MASTER)
 #if defined (DIAG_RSP_SEND)
   #error "Error DIAG_RSP_SEND defined on Master"
 #endif
#endif

#if defined (DIAG_MP_SLAVE)
 #if defined (DIAG_REQ_FWD)
   #error "Error: DIAG_REQ_FWD defined on Slave"
 #endif
 #if defined (DIAG_NO_DSM_CHAINING)
   #error "Error: DIAG_NO_DSM_CHAINING defined on Slave"
 #endif
 #if defined (DIAG_FRAMING)
   #error "Error: DIAG_FRAMING defined on Slave"
 #endif
#endif


#define DIAG_FTM_SWITCH_VAL 2
/* This structure is sent to an event listener when an event is processed.
   The implementation relies on the format of this structure.  It is
   dangerous to change this format of this structure. */
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
  unsigned int event_id;    /* event ID */
  //qword ts;         /* 8-byte CDMA time stamp. */
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;

  uint8 length;     /* length of the payload */
  /* Payload of size 'length': */
  uint8 payload[255];       /* payload, if length > 0 */

}
diag_event_type;

/*===========================================================================

FUNCTION TYPE DIAG_CMD_RSP

DESCRIPTION
  This function type is provided by the caller when sending a request packet
  using diag_cmd_req().  After the request is processed, this function is
  called (if specified) with a pointer to the response packet and the handle
  returned from the corresponding diag_cmd_req() call.

  Memory is owned by the DIAG task and is freed when this function returns.

  'param' is the unmodified value from the corresponding diag_cmd_req() call.

RETURN VALUE
  None.

===========================================================================*/
  typedef void (*diag_cmd_rsp) (const byte *rsp, unsigned int length,
                                void *param);

#ifdef __cplusplus
}
#endif

#endif              /* DIAG_H  */
