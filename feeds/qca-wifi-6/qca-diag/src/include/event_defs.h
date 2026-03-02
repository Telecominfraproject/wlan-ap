#ifndef EVENT_DEFS_H
#define EVENT_DEFS_H

/*!
@ingroup event_service
@file event_defs.h

@brief
Diagnostic Event Reporting Service ID and Payload Definitions

@details
This file contains all enumeration constants for event Identifiers
(event_id_enum_type) and the typedefines for event payload structures used by
clients of Diagnostic event reporting service.Please refer to 80-V6196-1 for
details.

@note
DO NOT MODIFY THIS FILE WITHOUT PRIOR APPROVAL

Event IDs, by design, are a tightly controlled set of values.  Developers
may not create event IDs at will.

Request new events using the folloiwng process:

 1. Send email to asw.diag.request requesting event ID assignments.
 2. Identify each event needed by name.
 3. Provide a brief description for each event.
 4. Describe the payload and size, if any.

*/

/*=======================================================================
  Copyright (c) 2001-2016 by Qualcomm Technologies, Inc.  All Rights Reserved.
===========================================================================*/

/*===========================================================================
                          Edit History

$Header: //source/qcom/qct/core/api/services/diag/main/latest/event_defs.h#104 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/14/14   xy      Added new events
10/01/14   xy      Added new events
08/21/14   xy      Added new events
08/04/14   xy      Added new events
07/16/14   xy      Added new events
06/18/14   xy      Added new events
05/27/14   xy      Added new events
04/23/14   xy      Added new events
03/31/14   xy      Added new events
02/25/14   xy      Added new events
01/29/14   xy      Added new events
01/14/14   xy      Added new events
11/13/13   xy      Added new events
10/09/13   xy      Added new events
09/25/13   xy      Added new events
09/09/13   sr      Added new events
08/26/13   sr      Added new events
08/09/13   sr      Added new events
07/30/13   sr      Added new events
06/26/13   sr      Added new events
05/08/13   sr      Added new events
03/12/13   sr      Added new events
02/06/13   sr      Added new events
01/22/13   sr      Added new events
11/27/12   sr      Added new events
11/08/12   sr      Added new events
10/31/12   sr      Added new events
10/15/12   sr      Added new events
09/20/12   rh      Added new events
08/29/12   rh      Added new events
08/08/12   rh      Added new events
07/16/12   rh      Added new events
06/06/12   rh      Added new events
03/26/12   rh      Added new events
02/29/12   rh      Added new GSM events
02/03/12   rh      Added SSR power events
01/12/12   rh      Added new events
11/29/11   rh      Added new event
09/14/11   hm      Added new events
08/19/11   hm      Added two new WCDMA events
08/05/11   hm      Reserved new event codes for TDSCDMA
06/13/11   hm      Added new RRC events
06/10/11   hm      Added new event
05/20/11   hm      Added new events for Sensors and Geran
04/29/11   hm      Added new events
04/25/11   hm      Added new events
04/18/11   hm      Added new events for LTE and WCDMA
04/05/11   hm      Added new events for LTE NAS
03/14/11   hm      Added new events for GPS and WCDMA
01/19/11   hm      Added new event for LTE ML1 (Tx Power backoff)
10/07/10   sg      Added new event for LTE ML1
05/21/10   sg      Doxygenated the file
08/24/09   mad     Removed inclusion of customer.h
10/03/08   vg      Updated code to use PACK() vs. PACKED
05/21/01   jal     Removed the #ifndef FEATURE_DIAG_NO_EVENTS -- this stuff
                   still needs to be there, even if the service is turned off.
05/21/01   sfh     Added EVENT_WCDMA_L1_STATE.
05/21/01   lad     Removed version check (now in event.c).
05/08/01   sfh     Merged events dropped when branch was made.  Added ba
                   approved HDR events.
04/17/01   lad     Created file from event.h.

===========================================================================*/

#include "comdef.h"   /* Definitions for byte, word, etc. */

/* -------------------------------------------------------------------------
** Definitions and Declarations
** ------------------------------------------------------------------------- */

/*!
@cond DOXYGEN_BLOAT
*/

typedef enum
{
  EVENT_DROP_ID = 0,

  EVENT_BAND_CLASS_CHANGE = 0x0100, /* Includes band class as payload */
  EVENT_CDMA_CH_CHANGE = 0x0101, /* Includes cdma channel as payload */
  EVENT_BS_P_REV_CHANGE = 0x0102, /* Includes BS p_rev as payload */
  EVENT_P_REV_IN_USE_CHANGE = 0x0103, /* Includes p_rev_in_use as payload */
  EVENT_SID_CHANGE = 0x0104,       /* Includes SID as payload */
  EVENT_NID_CHANGE = 0x0105,       /* Includes NID as payload */
  EVENT_PZID_CHANGE = 0x0106,      /* Includes PZID as payload */
  EVENT_PDE_SESSION_END = 0x0107,    /* No payload */
  EVENT_OP_MODE_CHANGE = 0x0108,   /* Includes operation mode as payload */
  EVENT_MESSAGE_RECEIVED = 0x0109,  /* Includes channel and message ID as
				       payload */
  EVENT_MESSAGE_TRANSMITTED = 0x010a, /* Includes channel and message ID as
                                        payload */
  EVENT_TIMER_EXPIRED = 0x010b,        /* Includes timer ID as payload */
  EVENT_COUNTER_THRESHOLD =0x10c,      /* Includes counter ID as payload */
  EVENT_CALL_PROCESSING_STATE_CHANGE = 0x010d, /* Includes new state and old state as
                                         payload */
  EVENT_CALL_CONTROL_INSTANTIATED = 0x10e,    /* Includes con_ref as payload */
  EVENT_CALL_CONTROL_STATE_CHANGE = 0x10f,    /* Includes con_ref, old substate and
                                         new substate as payload */
  EVENT_CALL_CONTROL_TERMINATED = 0x0110,      /* Includes con_ref as payload */
  EVENT_REG_ZONE_CHANGE = 0x0111,              /* Includes reg_zone as payload */
  EVENT_SLOTTED_MODE_OPERATION = 0x0112,       /* Includes enter/exit bit as payload */
  EVENT_QPCH_IN_USE,                  /* Includes enable/disable bit as payload */
  EVENT_IDLE_HANDOFF,                 /* Includes pn_offset as payload */
  EVENT_ACCESS_HANDOFF,               /* Includes pn_offset as payload */
  EVENT_ACCESS_PROBE_HANDOFF,         /* Includes pn_offset as payload */
  EVENT_SOFT_HANDOFF,
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH
       aset*/
  EVENT_HARD_HANDOFF_FREQ_CHANGE,
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH
       aset*/
  EVENT_HARD_HANDOFF_FRAME_OFFSET_CHANGE,
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH
       aset*/
  EVENT_HARD_HANDOFF_DISJOINT_ASET,
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH
       aset*/
  EVENT_UNSUCCESSFUL_HARD_HANDOFF,    /* No payload */
  EVENT_TMSI_ASSIGNED,                /* Includes TMSI as payload */
  EVENT_SERVICE_NEGOTIATION_COMPLETED,/* No payload */
  EVENT_SO_NEGOTIATION_COMPLETED,     /* No payload */
  EVENT_ENTER_CONTROL_HOLD_MODE,      /* No payload */
  EVENT_EXIT_CONTROL_HOLD_MODE,       /* No payload */
  EVENT_START_FWD_SUPP_BURST_ASSGN,   /* Includes SCH rate as payload */
  EVENT_END_FWD_SUPP_BURST_ASSGN,     /* No payload */
  EVENT_START_REV_SUPP_BURST_ASSGN,   /* Includes SCH rate as payload */
  EVENT_END_REV_SUPP_BURST_ASSGN,     /* No payload */
  EVENT_DTX,                          /* No payload */
  EVENT_T_ADD_ABORT,                  /* No payload */
  EVENT_CH_IND_CHANGE,                /* Include ch_ind as payload */
  EVENT_TRANSMITTER_DISABLED,         /* No payload */
  EVENT_TRANSMITTER_ENABLED,          /* No payload */
  EVENT_SMS_RECEIVED,                 /* No payload */
  EVENT_SMS_SENT,                     /* No payload */
  EVENT_INACTIVITY_TIMER_EXPIRED,     /* No payload */
  EVENT_DORMANT_TIMER_EXPIRED,        /* No payload */
  EVENT_ACCESS_ATTEMPT_FAIL_MAX_PROBES_SENT, /* No payload */
  EVENT_ACCESS_ATTEMPT_FAIL_LOSS_OF_PC_OR_FCCC, /* No payload */
  EVENT_PCH_ACQUIRED,                 /* Includes pagech and pn_offset
                                         as payload */
  EVENT_BCCH_ACQUIRED,                /* Includes walsh code for BCCH and
                                         pn_offset as payload */
  EVENT_FFCH_ACQUIRED,                /* Payload: 14 bytes */
  EVENT_FDCCH_ACQUIRED,               /* Payload: 14 bytes */
  EVENT_FFCH_PLUS_DCCH_ACQUIRED,      /* No payload */
  EVENT_REGISTRATION_PERFORMED,       /* Includes reg_type as payload */
  EVENT_NEW_SYSTEM_IDLE_HANDOFF,      /* No payload */
  EVENT_SYSTEM_RESELECTION,           /* Includes ecio and ps as payload */
  EVENT_RESCAN,                       /* No payload */
  EVENT_PROTOCOL_MISMATCH,            /* No payload */
  EVENT_LOCK,                         /* No payload */
  EVENT_UNLOCK,                       /* No payload */
  EVENT_ACCESS_DENIED,                /* No payload */
  EVENT_NDSS_OFF,                     /* No payload */
  EVENT_RELEASE,                      /* Payload: 1 byte */
  EVENT_ERROR,                        /* No payload */
  EVENT_REDIRECTION,                  /* No payload */
  EVENT_REGISTRATION_REJECTED,        /* No payload */
  EVENT_WRONG_SYSTEM,                 /* No payload */
  EVENT_WRONG_NETWORK,                /* No payload */
  EVENT_LOSS_OF_ACQ_AFTER_SLEEP,      /* No payload */
  EVENT_POWER_DOWN,                   /* No payload */
  EVENT_CALL_RELEASE_REQUEST,         /* No payload */
  EVENT_SERVICE_INACTIVE,             /* No payload */
  EVENT_EXTENDED_RELEASE,             /* No payload */

  EVENT_HDR_MSG_RX,                   /* protocol, msg- 3 bytes */
  EVENT_HDR_RXMSG_IGNORED_STATE,      /* protocol, msg- 3 bytes */
  EVENT_HDR_RXMSG_IGNORED_SEQ,        /* protocol, msg- 3 bytes */
  EVENT_HDR_TXMSG_ACKED,              /* protocol, msg- 3 bytes */
  EVENT_HDR_TXMSG_DROPPED,            /* protocol, msg- 3 bytes */
  EVENT_HDR_STATE_CHANGE,             /* protocol, from, to - 5 bytes */
  EVENT_HDR_ALMP_OBEYING_REDIRECTION, /* No payload */
  EVENT_HDR_ALMP_CONNECTION_CLOSED,   /* No payload */
  EVENT_HDR_ALMP_T_SD_RESELECT,       /* No payload */
  EVENT_HDR_ALMP_CONNECTION_OPENED,   /* No payload */
  EVENT_HDR_HMP_QUEUED_MSG,           /* protocol, msg- 3 bytes */
  EVENT_HDR_HMP_SENT_MSG,             /* protocol, msg, chan, is_reliable - 5 bytes */
  EVENT_HDR_HMP_ABORTING_ACMAC_ACTIVATION, /* No payload */
  EVENT_HDR_IDLE_T_CONFIG_RSP,        /* No payload */
  EVENT_HDR_IDLE_T_AT_SETUP,          /* No payload */
  EVENT_HDR_IDLE_T_SUSPEND,           /* No payload */
  EVENT_HDR_IDLE_CONNECTION_DENIED,   /* No payload */
  EVENT_HDR_INIT_T_SYNC_ACQ,          /* No payload */
  EVENT_HDR_INIT_PROTOCOL_MISMATCH,   /* No payload */
  EVENT_HDR_OVHD_INFO_CURRENT,        /* No payload */
  EVENT_HDR_OVHD_T_QC_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_T_SP_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_T_AP_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_IGNORED_MSG_UNEXPECTED_LINK, /* msg, exp_link.chan_num,
                                              exp_link.pilot, rx_link.chan_num,
                                              rx_link.pilot - 10 bytes */
  EVENT_HDR_OVHD_IGNORED_SP_MSG_DIFF_SEC_SIG, /* exp_sig, rx_sig - 8 bytes */
  EVENT_HDR_OVHD_IGNORED_AP_MSG_DIFF_ACC_SIG, /* exp_sig, rx_sig - 8 bytes */
  EVENT_HDR_OVHD_IGNORED_SP_MSG_DIFF_SEC_ID,  /* No payload */
  EVENT_HDR_OVHD_SP_MSG_RX,           /* No payload */
  EVENT_HDR_OVHD_AP_MSG_RX,           /* No payload */
  EVENT_HDR_RUP_T_CONNECTION_SETUP,   /* No payload */
  EVENT_HDR_SLP_MAX_RETRIES,          /* msg - 2 bytes */
  EVENT_HDR_LMAC_ACQ_FAIL_PILOT,      /* No payload */
  EVENT_HDR_LMAC_ACQ_SUCCESS,         /* No payload */
  EVENT_HDR_LMAC_NETWORK_LOST,        /* No payload */
  EVENT_HDR_LMAC_IDLE_HO,             /* new_pilot - 2 bytes */
  EVENT_HDR_LMAC_CHAN_CHANGE_COMPLETE, /* No payload */
  EVENT_HDR_LMAC_ACCESS_HO_NEEDED,    /* suggested_pilot - 2 bytes */
  EVENT_HDR_LMAC_ACCESS_HO_COMPLETE,  /* new_pilot - 2 bytes */
  EVENT_HDR_LMAC_ACQUIRE,             /* channel 2 bytes */
  EVENT_HDR_LMAC_CHANGING_CC_HASH,    /* cc_hash - 1 byte */
  EVENT_HDR_LMAC_IDLE_CHAN_CHANGE,    /* channel - 2 bytes */
  EVENT_HDR_CMAC_T_SUPERVISION,       /* No payload */
  EVENT_HDR_AMAC_START_ACCESS,        /* No payload */
  EVENT_HDR_AMAC_PROBING_STOPPED,     /* No payload */
  EVENT_HDR_AMAC_ACCESS_COMPLETE,     /* No payload */
  EVENT_HDR_AMAC_ACCESS_ABORTED,      /* No payload */
  EVENT_HDR_AMAC_MAX_PROBES,          /* No payload */
  EVENT_HDR_FMAC_DROP_PKT,            /* No payload */
  EVENT_HDR_RMAC_T_RATE_LIMIT,        /* No payload */
  EVENT_HDR_RMAC_TX_STARTED,          /* No payload */
  EVENT_HDR_RMAC_TX_STOPPED,          /* No payload */
  EVENT_HDR_SMP_T_KEEP_ALIVE,         /* No payload */
  EVENT_HDR_AMP_ASSIGN_MSG_IGNORED_FRESH, /* No payload */
  EVENT_HDR_AMP_T_AT_RESPONSE,        /* No payload */
  EVENT_HDR_AMP_T_DUAL_ADDRESS,       /* No payload */
  EVENT_HDR_SCP_BEGIN_CONFIGURATION,  /* No payload */
  EVENT_HDR_SCP_T_CONFIG_RSP,         /* No payload */
  EVENT_HDR_SCP_T_AN_INIT_STATE,      /* No payload */

  EVENT_WCDMA_L1_STATE,               /* l1_state - 1 byte */
  EVENT_WCDMA_IMSI,                   /* IMSI - 9 bytes */
  EVENT_GSM_L1_STATE,                 /* GSM l1_state - 1 byte */
  EVENT_RANDOM_ACCESS_REQUEST,        /* GSM Random Access Request - 4 bytes */
  EVENT_HIGH_LEVEL_CALL_PROCESSING_STATE_CHANGE, /* Puma requested event */
                                                 /* (same payload as CALL_PROCESSING_STATE_CHANGE) */
  EVENT_ENCRYPTION_FAILURE,                  /* Puma event, no payload */
  EVENT_ACCT_BLOCKED,                        /* Puma event, no payload */
  EVENT_COMMON_CHANNEL_MONITORED,            /* Puma event, 1 byte payload */
  EVENT_SOFT_HANDOFF_V2,                     /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_FREQ_CHANGE_V2,         /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_FRAME_OFFSET_CHANGE_V2, /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_DISJOINT_ASET_V2,       /* Puma event, 14 byte payload */
  EVENT_WCDMA_NEW_REFERENCE_CELL,
  EVENT_CALL_CONTROL_CONREF_CHANGE,          /* Puma event, 2 byte payload */

  EVENT_GPS_SESSION_BEGIN,
  EVENT_GPS_SESSION_END,
  EVENT_GPS_WAITING_ON_SA,
  EVENT_GPS_PPM_START,
  EVENT_GPS_PPM_RESULTS,
  EVENT_GPS_PPM_END,
  EVENT_GPS_VISIT_BEGIN,
  EVENT_GPS_VISIT_END,
  EVENT_GPS_CDMA_RESUMED_AFTER_GPS_VISIT,
  EVENT_GPS_PD_SESSION_BEGIN,
  EVENT_GPS_PD_SESSION_END,                  /* Payload: 1 byte PDSM substate */
  EVENT_GPS_IS801_RX,                        /* Payload, 1 byte msg_type */
  EVENT_GPS_IS801_TX,                        /* Payload: 1 byte msg_type */
  EVENT_POWERUP,
  EVENT_WCDMA_ASET,
  EVENT_CM_CALL_STATE,                       /* (1 byte payload: overall call state) */
  EVENT_CM_OPERATIONAL_MODE,                 /* (1 byte payload: op mode) */
  EVENT_CM_SYSTEM_MODE,                      /* (1 byte payload: sys_mode) */

  EVENT_DEEP_SLEEP,                          /* no payload */
  EVENT_WAKEUP,                              /* unsigned long (4 bytes) payload */
  EVENT_ACQUISITION_MODE,                    /* unsigned char (1 byte) payload */
  EVENT_ACQUISITION_TYPE,                    /* unsigned char (1 byte) payload */
  EVENT_ACP_EXIT,                            /* unsigned char (1 byte) payload */
  EVENT_CDMA_EXIT,                           /* unsigned char (1 byte) payload */

  EVENT_HDR_HYBRID_POWER_SAVE,               /* No payload */
  EVENT_HDR_DEEP_SLEEP,                      /* No payload */
  EVENT_HDR_RESELECTION,                     /* No payload */
  EVENT_SAM_LOCK_GRANTED,                    /* <NewOwner (1 byte) <Duration (2 bytes) */
  EVENT_SAM_LOCK_RELEASED,                   /* <OldOwner (1 byte) */

  EVENT_GSM_HANDOVER_START,
  EVENT_GSM_HANDOVER_END,
  EVENT_GSM_LINK_FAILURE,
  EVENT_GSM_RESELECT_START,
  EVENT_GSM_RESELECT_END,
  EVENT_GSM_CAMP_ATTEMPT_START,
  EVENT_GSM_RR_IN_SERVICE,
  EVENT_GSM_RR_OUT_OF_SERVICE,
  EVENT_GSM_PAGE_RECEIVED,
  EVENT_GSM_CAMP_ATTEMPT_END,
  EVENT_GPS_IS801_TIMEOUT,
  EVENT_GPS_IS801_DISCARD,                   /* Payload: 1 byte msg type */
  EVENT_GSM_CELL_SELECTION_START,            /* Payload: 1 byte cell_selection_reason */
  EVENT_GSM_CELL_SELECTION_END,              /* Payload: 1 byte end_status success/failure reason */
  EVENT_GSM_POWER_SCAN_STATUS,               /* Payload: 1 byte status (started or completed) */
  EVENT_GSM_PLMN_LIST_START,                 /* Payload: 1 byte */
  EVENT_GSM_PLMN_LIST_END,
  EVENT_WCDMA_INTER_RAT_HANDOVER_START,      /* Payload: 4 bytes handover type, BCCH ARFCN, BSIC */
  EVENT_WCDMA_INTER_RAT_HANDOVER_END,        /* Payload: 2 bytes success, failure reason */

  EVENT_GSM_MESSAGE_SENT,                    /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_MESSAGE_RECEIVED,                /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_TIMER_EXPIRED,                   /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_COUNTER_EXPIRED,                 /* Payload: 2 bytes indicating identity of the message */

  EVENT_NAS_MESSAGE_SENT,
  EVENT_NAS_MESSAGE_RECEIVED,
  EVENT_RRC_MESSAGE_SENT,
  EVENT_RRC_MESSAGE_RECEIVED,
                                    /* Camera events: No Payload */
  EVENT_CAMERA_CANNOT_CAPTURE,      /* Cannot capture, transition to ready */
  EVENT_CAMERA_CANNOT_CONFIG_JPEG,  /* Cannot config JPEG, transition to ready */
  EVENT_CAMERA_CANNOT_CONFIG_VFE,   /* Cannot config VFE, transition to ready */
  EVENT_CAMERA_CANNOT_ENCODE,       /* Cannot encode, transition to ready */
  EVENT_CAMERA_CANNOT_IDLE_DSP,     /* Cannot idle DSP, transition to ready */
  EVENT_CAMERA_CANNOT_LOAD_DSP,     /* Cannot load DSP, transition to ready */
  EVENT_CAMERA_DSP_FATAL,           /* DSP fatal error, transition to ready */
  EVENT_CAMERA_DSP_REQ_ILLEGAL,     /* DSP request illegal, transition to ready */
  EVENT_CAMERA_EFS_FAILED,          /* EFS failed, transition to ready */
  EVENT_CAMERA_EXIT,                /* Stop, transition to ready */
  EVENT_CAMERA_FORMAT_NOT_SUPPORTED,/* Format not supported */
  EVENT_CAMERA_FUNCTION_REJECTED,   /* Function rejected, transition to ready */
  EVENT_CAMERA_IMAGE_CORRUPT,       /* Image corrupted, reconfig */
  EVENT_CAMERA_INVALID_CONFIG_PARM, /* Invalid config parm, to ready */
  EVENT_CAMERA_INVALID_SET_ID,      /* Invalid set param ID type */
  EVENT_CAMERA_INVALID_STATE,       /* Invalid state */
  EVENT_CAMERA_JPEG_ENCODED,        /* JPEG encoded */
  EVENT_CAMERA_NO_MEMORY,           /* No memory, transition to ready */
  EVENT_CAMERA_NO_PICTURE,          /* Picture not ready */
  EVENT_CAMERA_PICTURE_SAVED,       /* Picture saved, transition to ready */
  EVENT_CAMERA_PICTURE_TAKEN,       /* Picture taken, transition to ready */
  EVENT_CAMERA_PREVIEW,             /* Enter preview */
  EVENT_CAMERA_RECORD,              /* Enter recording */
  EVENT_CAMERA_SAVE_PICTURE,        /* Enter save picture */
  EVENT_CAMERA_SET_FAILED,          /* Set failed */
  EVENT_CAMERA_SET_SUCCEEDED,       /* Set succeeded */
  EVENT_CAMERA_START,               /* Start, transition to ready */
  EVENT_CAMERA_STOP,                /* Stop, transition to init */
  EVENT_CAMERA_TAKE_PICTURE,        /* Enter take picture */

  EVENT_DIAG_STRESS_TEST_NO_PAYLOAD,
  EVENT_DIAG_STRESS_TEST_WITH_PAYLOAD,

  EVENT_CM_CALL_ORIG_START_P1,      /* Payload: 5 single bytes */
  EVENT_CM_CALL_ORIG_START_P2,      /* Payload: 3 single bytes */
  EVENT_CM_CALL_ORIG_START_P3,      /* Payload: 3 single bytes */
  EVENT_CM_CALL_ORIG_SWITCH_TO_HDR, /* No payload */
  EVENT_CM_CALL_ORIG_REDIAL,        /* Payload: 4 bytes */
  EVENT_CM_CALL_ORIG_SEND_HDR_ORIG, /* No payload */
  EVENT_CM_CALL_ORIG_SEND_MC_ORIG,  /* No payload */
  EVENT_CM_CALL_ORIG_END,           /* Payload: 2 bytes */
  EVENT_CM_CALL_ORIG_CONNECTED,     /* Payload: 3 bytes */

  EVENT_MT_SMS_NOTIFY,              /* Payload: 1 byte mem_type, 4 byte msg_index */
  EVENT_SMS_SLOT_WAKEUP,            /* Payload: 1 byte slot_mask */
  EVENT_MO_SMS_STATUS,              /* Payload: 4 byte transaction_id, 1 byte mem_type,
                                                4 byte msg_index, 1 byte report_status,
                                                1 byte case_code */
  EVENT_GPRS_SURROUND_SEARCH_START,
  EVENT_GPRS_SURROUND_SEARCH_END,
  EVENT_GPRS_MAC_RESELECT_IND,
  EVENT_GPRS_PAGE_RECEIVED,
  EVENT_GPRS_LINK_FAILURE,
  EVENT_GPRS_CELL_UPDATE_START,
  EVENT_GPRS_CELL_UPDATE_END,
  EVENT_GPRS_EARLY_CAMPING,
  EVENT_PACKET_RANDOM_ACCESS_REQ,
  EVENT_GPRS_MAC_MSG_SENT,          /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_MAC_MSG_RECEIVED,      /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_SMGMM_MSG_SENT,        /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_SMGMM_MSG_RECEIVED,    /* Payload: 3 bytes giving message identity and channel */

  EVENT_CP_MATCHED_MSG,             /* Payload: 1 byte */
  EVENT_PREF_SYS_RESEL,             /* Payload: none */

  EVENT_WCDMA_LAYER1_PRACH,         /* Payload: 4 bytes */
  EVENT_WCDMA_LAYER1_MEASUREMENT,   /* Payload: Unspecified */

  EVENT_MOBILITY_MANAGEMENT_STATE_CHANGE, /* Payload: 1 byte old state, 1 byte new state */

  EVENT_LSM_STATE_CHANGE,           /* Payload: 1 byte */
  EVENT_RLP,                        /* Payload: 2 bytes */

  EVENT_CM_MODE_PREF,               /* Payload: 4 bytes */
  EVENT_CM_BAND_PREF,               /* Payload: 4 bytes */
  EVENT_CM_ROAM_PREF,               /* Payload: 4 bytes */
  EVENT_CM_SRV_DOMAIN_PREF,         /* Payload: 4 bytes */
  EVENT_CM_GW_ACQ_ORDER_PREF,       /* Payload: 4 bytes */
  EVENT_CM_HYBRID_PREF,             /* Payload: 4 bytes */
  EVENT_CM_NETWORK_SEL_MODE_PREF,   /* Payload: 4 bytes */

  EVENT_WCDMA_L1_SUSPEND,                 /* Payload: 3 bytes */
  EVENT_WCDMA_L1_RESUME,                  /* Payload: 5 bytes */
  EVENT_WCDMA_L1_STOPPED,                 /* Payload: 4 bytes */
  EVENT_WCDMA_TO_WCDMA_RESELECTION_START, /* Payload: 6 bytes */
  EVENT_WCDMA_TO_GSM_RESELECTION_START,   /* Payload: 4 bytes */
  EVENT_WCDMA_TO_GSM_RESELECTION_END,     /* Payload: 2 bytes */
  EVENT_WCDMA_TO_WCDMA_RESELECTION_END,   /* Payload: 4 bytes */
  EVENT_WCDMA_RACH_ATTEMPT,               /* Payload: 3 bytes */

  EVENT_START_FWD_SUPP_BURST_ASSIGN,      /* Payload: 3 bytes */
  EVENT_START_REV_SUPP_BURST_ASSIGN,      /* Payload: 3 bytes */
  EVENT_REV_FCH_GATING_IN_USE,            /* Payload: 1 byte  */
  EVENT_PPP,                              /* Payload: 7 bytes */
  EVENT_MIP,                              /* Payload: 2 bytes */
  EVENT_TCP,                              /* Payload: 5 bytes */
  EVENT_CAMERA_EXIF_FAILED,               /* EXIF encoding failed */
  EVENT_CAMERA_VIDEO_FAILED,              /* Video encoding failed */
  EVENT_CAMERA_NO_SENSOR,                 /* No sensor */
  EVENT_CAMERA_ABORT,                     /* Operation aborted */

  EVENT_CM_BLOCK_HDR_ORIG_DURING_GPS,     /* No payload */
  EVENT_CM_ALLOW_HDR_ORIG_DURING_GPS,     /* No payload */

  EVENT_GSM_AMR_STATE_CHANGE,             /* 2 bytes payload */
  EVENT_GSM_RATSCCH_IN_DTX,               /* No Payload */
  EVENT_GSM_FACCH_IN_DTX,                 /* No Payload */
  EVENT_GSM_FACCH_AND_RATSCCH_COLLISION,  /* No Payload */
  EVENT_GSM_FACCH_AND_SID_UPDATE_COLLISION,  /* No Payload */
  EVENT_GSM_RATSCCH_AND_SID_UPDATE_COLLISION,/* No Payload */
  EVENT_GSM_RATSCCH_CMI_PHASE_CHANGE,     /* 2 bytes payload */
  EVENT_GSM_RATSCCH_REQ_ACT_TIMER_EXPIRY, /* 8 bytes payload */
  EVENT_GSM_RATSCCH_ACK_ACT_TIMER_EXPIRY, /* 2 bytes payload */
  EVENT_GSM_AMR_CMC_TURNAROUND_TIME,      /* 2 bytes payload */
  EVENT_CM_PLMN_FOUND,                    /* 12 bytes */
  EVENT_CM_SERVICE_CONFIRMED,             /* 12 bytes */

  EVENT_GPRS_MAC_CAMPED_ON_CELL,          /* Event Id : 559, No payload */
  EVENT_GPRS_LLC_READY_TIMER_START,       /* Event Id : 560, No payload */
  EVENT_GPRS_LLC_READY_TIMER_END,         /* Event Id : 561, No payload */

  EVENT_WCDMA_PHYCHAN_ESTABLISHED,        /* Payload TBD */
  EVENT_HS_DISPLAY_BMP_CAPTURE_STATUS,    /* Payload 4 bytes */

  EVENT_WCDMA_CELL_SELECTED,              /* 4 byte Payload */
  EVENT_WCDMA_PAGE_RECEIVED,              /* 2 byte Payload */
  EVENT_WCDMA_SEND_KEY,                   /* Payload TBD */
  EVENT_WCDMA_RL_FAILURE,                 /* No Payload */
  EVENT_WCDMA_MAX_RESET,                  /* 2 byte Payload */
  EVENT_WCDMA_CALL_SETUP,                 /* Payload TBD */
  EVENT_WCDMA_CALL_DROPPED,               /* Payload TBD */
  EVENT_WCDMA_RRC_STATE,                  /* 3 byte Payload */
  EVENT_GPS_PD_CONNECTION_TIMEOUT,        /* No Payload */
  EVENT_GPS_PD_DISCONNECTION_COMPLETE,    /* No Payload */

  EVENT_MEDIA_PLAYER_START,               /* media player starts playing a clip, no payload */
  EVENT_MEDIA_PLAYER_STOP,                /* media player stopped playing a clip, no payload */
  EVENT_MEDIA_PLAYER_SEEK,                /* media player repositioned itself, no payload */

  EVENT_GPS_SRCH_START,             /* Payload: session_type (1 byte) */

  EVENT_GPS_SRCH_END,               /* No Payload */
  EVENT_GPS_PPM_PAUSE,              /* Payload: pause_reason (1 byte) */
  EVENT_GPS_PPM_RESUME,             /* No Payload */
  EVENT_GPS_SA_RECEIVED,            /* Payload: REF_BIT_NUM (2 bytes),
                                                DR_SIZE     (1byte)
                                    */
  EVENT_GPS_CLK_ON,                 /* No Payload */
  EVENT_GPS_CLK_OFF,                /* No Payload */
  EVENT_GPS_VISIT_REQUEST,          /* No Payload */
  EVENT_GPS_VISIT_RESPONSE,         /* Payload: response_result (1 byte)
                                    */
  EVENT_GPS_TA_START,               /* No Payload */
  EVENT_GPS_DSP_READY,              /* No Payload */
  EVENT_GPS_DSP_CHANNEL_START,      /* Payload: SV_ID         (1 byte),
                                                SRCH_MODE     (1 byte),
                                                CHANNEL_NUM   (1 byte),
                                                RESERVED      (1 byte)
                                    */
  EVENT_GPS_DSP_CHANNEL_DONE,       /* Payload: channel_num (1 byte) */
  EVENT_GPS_DSP_STOP,               /* No Payload */
  EVENT_GPS_DSP_DONE,               /* No Payload */
  EVENT_GPS_TB_END,                 /* No Payload */
  EVENT_GPS_SRCH_LARGE_DOPP_WIN,    /* Payload: sv_prn_num (1 byte),
                                                srch_mode (1 byte),
                                                dopp_wind (2 byte)
                                    */
  EVENT_GPS_SRCH_EXCEPTION,         /* Payload: grid_log_id (2 byte),
                                                exception_type (1 byte)
                                    */
  EVENT_GPS_SRCH_HW_POLLING1,       /* Payload: agc_val (2 byte),
                                                dci_off (2 byte),
                                                dcq_off (2 byte),
                                                trk_lo (2 byte),
                                                lo_bias (2 byte)
                                    */

  EVENT_GPS_SRCH_HW_POLLING2,       /* Payload: sync80 (2 byte)
                                    */
  EVENT_GPS_PGI_ACTION_PROCESS,     /* Payload: pgi_substate (1 byte),
                                                pgi_cmd (1 byte)
                                    */
  EVENT_GPS_GSC_ACTION_PROCESS,     /* Payload: gsc_substate (1 byte),
                                                gsc_cmd (1 byte)
                                    */
  EVENT_GPS_PGI_ABORT,              /* Payload: pgi_subsate (1 byte) */
  EVENT_GPS_GSC_ABORT,              /* Payload: gsc_subsate (1 byte) */



  EVENT_GPS_PD_FIX_START,                /* Payload: event_log_cnt  (2 byte),
                                                operation_mode (1 byte)
                                         */
  EVENT_GPS_PD_FIX_END,                  /* Payload: end_status     (1 byte)  */
  EVENT_GPS_DATA_DOWNLOAD_START,         /* Payload: data_type   (1 byte),
                                                     sv_mask     (4 byte)     */
  EVENT_GPS_DATA_DOWNLOAD_END,           /* Payload: end_status      (1 byte) */
  EVENT_GPS_PD_SESSION_START,            /* Payload: start_source    (4 bit)
                                                     operation_mode  (4 bit)
                                                     session_type    (4 bit)
                                                     privacy         (4 bit)
                                                     num_fixed       (2 byte)
                                                     fix_period      (2 byte)
                                                     nav_data_dl     (4 bit)
                                                     prq             (1 byte)
                                                     threshold       (2 byte)
                                                     transport_type  (4 bit)  */
  EVENT_GPS_DORMANCY_BEGIN,               /* No Payload */
  EVENT_GPS_DORMANCY_END,                 /* No Payload */
  EVENT_GPS_PRQ_TIMEOUT,                  /* No Payload */
  EVENT_GPS_PD_CONNECTION_START,          /* No Payload */
  EVENT_GPS_PD_CONNECTION_ESTABLISHED,    /* No Payload */
  EVENT_GPS_PD_DISCONNECTION_START,       /* No Payload */
  EVENT_GPS_FTEST_FIX_START,              /* Payload: reserved (4 byte) */
  EVENT_GPS_FTEST_FIX_END,                /* Payload: reserved (4 byte) */
  EVENT_GPS_PD_POSITION,                  /* No Payload */
  EVENT_GPS_E911_START,                   /* No Payload */
  EVENT_GPS_E911_END,                     /* No Payload */
  EVENT_GPS_DBM_SEND_FAILURE,             /* No Payload */
  EVENT_GPS_UAPDMS_STATE_CHANGE,          /* Payload: new_state (1 byte)
                                                      reason    (1 byte) */
  EVENT_WCDMA_OUT_OF_SERVICE,             /* No Payload */
  EVENT_GSM_L1_SUBSTATE,                  /* 2 bytes payload */
  EVENT_SD_EVENT_ACTION,                  /* 8 byte payload */
  EVENT_SD_EVENT_ACTION_HYBR,             /* 8 byte payload */

  EVENT_UMTS_CALLS_STATISTICS,            /* 1 byte payload */
  EVENT_PZID_HAT_STARTED,                 /* No payload */
  EVENT_WCDMA_DRX_CYCLE,                  /* 3 byte payload */
  EVENT_WCDMA_RE_ACQUISITION_FAIL,        /* No payload */
  EVENT_WCDMA_RRC_RB0_SETUP_FAILURE,      /* No payload */
  EVENT_WCDMA_RRC_PHYCHAN_EST_FAILURE,    /* No payload */
  EVENT_CM_CALL_EVENT_ORIG,               /* 3 byte payload */
  EVENT_CM_CALL_EVENT_CONNECT,            /* 3 byte payload */
  EVENT_CM_CALL_EVENT_END,                /* 2 byte payload */
  EVENT_CM_ENTER_EMERGENCY_CB,            /* No payload */
  EVENT_CM_EXIT_EMERGENCY_CB,             /* No payload */
  EVENT_PZID_HAT_EXPIRED,                 /* No payload */
  EVENT_HDR_SMP_SESSION_CLOSED,           /* 1 byte payload */
  EVENT_WCDMA_MEMORY_LEAK,                /* No payload */
  EVENT_PZID_HT_STARTED,                  /* 1 byte payload */
  EVENT_PZID_HT_EXPIRED,                  /* 1 byte payload */
  EVENT_ACCESS_ENTRY_HANDOFF,             /* 2 byte payload */

  EVENT_BREW_APP_START,                   /* 8 byte payload */
  EVENT_BREW_APP_STOP,                    /* 8 byte payload */
  EVENT_BREW_APP_PAUSE,                   /* 8 byte payload */
  EVENT_BREW_APP_RESUME,                  /* 8 byte payload */
  EVENT_BREW_EXT_MODULE_START,            /* 8 byte payload */
  EVENT_BREW_EXT_MODULE_STOP,             /* 8 byte payload */
  EVENT_BREW_ERROR,                       /* 8 byte payload */
  EVENT_BREW_RESERVED_647,                /* BREW internal use only */
  EVENT_BREW_RESERVED_648,                /* BREW internal use only */
  EVENT_BREW_RESERVED_649,                /* BREW internal use only */
  EVENT_BREW_RESERVED_650,                /* BREW internal use only */
  EVENT_BREW_RESERVED_651,                /* BREW internal use only */
  EVENT_BREW_RESERVED_652,                /* BREW internal use only */
  EVENT_BREW_RESERVED_653,                /* BREW internal use only */
  EVENT_BREW_RESERVED_654,                /* BREW internal use only */
  EVENT_BREW_RESERVED_655,                /* BREW internal use only */
  EVENT_BREW_USER_656,                    /* Start of BREW user events */
  EVENT_BREW_GENERIC,                     /* 8 byte payload: clsid + data */
  EVENT_BREW_MEDIAPLAYER_SELECT_FILE,     /* no payload */
  EVENT_BREW_MEDIAPLAYER_CONTROL,         /* no payload */
  EVENT_BREW_APP_FORMITEM_STACK_CHANGE,   /* 1 byte payload */
  EVENT_BREW_CATAPP_RECV_PROACTIVE_CMD,   /* No payload */
  EVENT_BREW_CATAPP_TERMINAL_RSP,         /* No payload */
  EVENT_BREW_CATAPP_NO_DISPLAY,           /* No payload */
  EVENT_BREW_SIRIUS_EMAIL_DELETE,                  /* No payload */
  EVENT_BREW_SIRIUS_EMAIL_OPERATION_COMPLETE,      /* 8 byte payload */
  EVENT_BREW_SIRIUS_EMAIL_NEW_EMAIL_NOTIFICATION,  /* No payload */
  EVENT_BREW_UNDEFINED_667,
  EVENT_BREW_UNDEFINED_668,
  EVENT_BREW_UNDEFINED_669,
  EVENT_BREW_UNDEFINED_670,
  EVENT_BREW_UNDEFINED_671,
  EVENT_BREW_UNDEFINED_672,
  EVENT_BREW_UNDEFINED_673,
  EVENT_BREW_UNDEFINED_674,
  EVENT_BREW_UNDEFINED_675,
  EVENT_BREW_UNDEFINED_676,
  EVENT_BREW_UNDEFINED_677,
  EVENT_BREW_UNDEFINED_678,
  EVENT_BREW_UNDEFINED_679,
  EVENT_BREW_UNDEFINED_680,
  EVENT_BREW_UNDEFINED_681,
  EVENT_BREW_UNDEFINED_682,
  EVENT_BREW_UNDEFINED_683,
  EVENT_BREW_UNDEFINED_684,
  EVENT_BREW_UNDEFINED_685,
  EVENT_BREW_UNDEFINED_686,
  EVENT_BREW_UNDEFINED_687,
  EVENT_BREW_UNDEFINED_688,
  EVENT_BREW_UNDEFINED_689,
  EVENT_BREW_UNDEFINED_690,
  EVENT_BREW_UNDEFINED_691,
  EVENT_BREW_UNDEFINED_692,
  EVENT_BREW_UNDEFINED_693,
  EVENT_BREW_UNDEFINED_694,
  EVENT_BREW_UNDEFINED_695,
  EVENT_BREW_UNDEFINED_696,
  EVENT_BREW_UNDEFINED_697,
  EVENT_BREW_UNDEFINED_698,
  EVENT_BREW_UNDEFINED_699,
  EVENT_BREW_UNDEFINED_700,
  EVENT_BREW_UNDEFINED_701,
  EVENT_BREW_UNDEFINED_702,
  EVENT_BREW_UNDEFINED_703,
  EVENT_BREW_UNDEFINED_704,
  EVENT_BREW_UNDEFINED_705,
  EVENT_BREW_UNDEFINED_706,
  EVENT_BREW_UNDEFINED_707,
  EVENT_BREW_UNDEFINED_708,
  EVENT_BREW_UNDEFINED_709,
  EVENT_BREW_UNDEFINED_710,
  EVENT_BREW_UNDEFINED_711,
  EVENT_BREW_UNDEFINED_712,
  EVENT_BREW_UNDEFINED_713,
  EVENT_BREW_UNDEFINED_714,
  EVENT_BREW_UNDEFINED_715,
  EVENT_BREW_UNDEFINED_716,
  EVENT_BREW_UNDEFINED_717,
  EVENT_BREW_UNDEFINED_718,
  EVENT_BREW_UNDEFINED_719,
  EVENT_BREW_UNDEFINED_720,
  EVENT_BREW_UNDEFINED_721,
  EVENT_BREW_UNDEFINED_722,
  EVENT_BREW_UNDEFINED_723,
  EVENT_BREW_UNDEFINED_724,
  EVENT_BREW_UNDEFINED_725,
  EVENT_BREW_UNDEFINED_726,
  EVENT_BREW_UNDEFINED_727,
  EVENT_BREW_UNDEFINED_728,
  EVENT_BREW_UNDEFINED_729,
  EVENT_BREW_UNDEFINED_730,
  EVENT_BREW_UNDEFINED_731,
  EVENT_BREW_UNDEFINED_732,
  EVENT_BREW_UNDEFINED_733,
  EVENT_BREW_UNDEFINED_734,
  EVENT_BREW_UNDEFINED_735,
  EVENT_BREW_UNDEFINED_736,
  EVENT_BREW_UNDEFINED_737,
  EVENT_BREW_UNDEFINED_738,
  EVENT_BREW_UNDEFINED_739,
  EVENT_BREW_UNDEFINED_740,
  EVENT_BREW_UNDEFINED_741,
  EVENT_BREW_UNDEFINED_742,
  EVENT_BREW_UNDEFINED_743,
  EVENT_BREW_UNDEFINED_744,
  EVENT_BREW_UNDEFINED_745,
  EVENT_BREW_UNDEFINED_746,
  EVENT_BREW_UNDEFINED_747,
  EVENT_BREW_UNDEFINED_748,
  EVENT_BREW_UNDEFINED_749,
  EVENT_BREW_UNDEFINED_750,
  EVENT_BREW_UNDEFINED_751,
  EVENT_BREW_UNDEFINED_752,
  EVENT_BREW_UNDEFINED_753,
  EVENT_BREW_UNDEFINED_754,
  EVENT_BREW_UNDEFINED_755,
  EVENT_BREW_UNDEFINED_756,
  EVENT_BREW_UNDEFINED_757,
  EVENT_BREW_UNDEFINED_758,
  EVENT_BREW_UNDEFINED_759,
  EVENT_BREW_UNDEFINED_760,
  EVENT_BREW_UNDEFINED_761,
  EVENT_BREW_UNDEFINED_762,
  EVENT_BREW_UNDEFINED_763,
  EVENT_BREW_UNDEFINED_764,
  EVENT_BREW_UNDEFINED_765,
  EVENT_BREW_UNDEFINED_766,
  EVENT_BREW_UNDEFINED_767,
  EVENT_BREW_UNDEFINED_768,
  EVENT_BREW_UNDEFINED_769,
  EVENT_BREW_UNDEFINED_770,
  EVENT_BREW_UNDEFINED_771,
  EVENT_BREW_UNDEFINED_772,
  EVENT_BREW_UNDEFINED_773,
  EVENT_BREW_UNDEFINED_774,
  EVENT_BREW_UNDEFINED_775,
  EVENT_BREW_UNDEFINED_776,
  EVENT_BREW_UNDEFINED_777,
  EVENT_BREW_UNDEFINED_778,
  EVENT_BREW_UNDEFINED_779,
  EVENT_BREW_UNDEFINED_780,
  EVENT_BREW_UNDEFINED_781,
  EVENT_BREW_UNDEFINED_782,
  EVENT_BREW_UNDEFINED_783,
  EVENT_BREW_UNDEFINED_784,
  EVENT_BREW_UNDEFINED_785,
  EVENT_BREW_UNDEFINED_786,
  EVENT_BREW_UNDEFINED_787,
  EVENT_BREW_UNDEFINED_788,
  EVENT_BREW_UNDEFINED_789,
  EVENT_BREW_UNDEFINED_790,
  EVENT_BREW_UNDEFINED_791,
  EVENT_BREW_UNDEFINED_792,
  EVENT_BREW_UNDEFINED_793,
  EVENT_BREW_UNDEFINED_794,
  EVENT_BREW_UNDEFINED_795,
  EVENT_BREW_UNDEFINED_796,
  EVENT_BREW_UNDEFINED_797,
  EVENT_BREW_UNDEFINED_798,
  EVENT_BREW_UNDEFINED_799,
  EVENT_BREW_UNDEFINED_800,
  EVENT_BREW_UNDEFINED_801,
  EVENT_BREW_UNDEFINED_802,
  EVENT_BREW_UNDEFINED_803,
  EVENT_BREW_UNDEFINED_804,
  EVENT_BREW_UNDEFINED_805,
  EVENT_BREW_UNDEFINED_806,
  EVENT_BREW_UNDEFINED_807,
  EVENT_BREW_UNDEFINED_808,
  EVENT_BREW_UNDEFINED_809,
  EVENT_BREW_UNDEFINED_810,
  EVENT_BREW_UNDEFINED_811,
  EVENT_BREW_UNDEFINED_812,
  EVENT_BREW_UNDEFINED_813,
  EVENT_BREW_UNDEFINED_814,
  EVENT_BREW_UNDEFINED_815,
  EVENT_BREW_UNDEFINED_816,
  EVENT_BREW_UNDEFINED_817,
  EVENT_BREW_UNDEFINED_818,
  EVENT_BREW_UNDEFINED_819,
  EVENT_BREW_UNDEFINED_820,
  EVENT_BREW_UNDEFINED_821,
  EVENT_BREW_UNDEFINED_822,
  EVENT_BREW_UNDEFINED_823,
  EVENT_BREW_UNDEFINED_824,
  EVENT_BREW_UNDEFINED_825,
  EVENT_BREW_UNDEFINED_826,
  EVENT_BREW_UNDEFINED_827,
  EVENT_BREW_UNDEFINED_828,
  EVENT_BREW_UNDEFINED_829,
  EVENT_BREW_UNDEFINED_830,
  EVENT_BREW_UNDEFINED_831,
  EVENT_BREW_UNDEFINED_832,
  EVENT_BREW_UNDEFINED_833,
  EVENT_BREW_UNDEFINED_834,
  EVENT_BREW_UNDEFINED_835,
  EVENT_BREW_UNDEFINED_836,
  EVENT_BREW_UNDEFINED_837,
  EVENT_BREW_UNDEFINED_838,
  EVENT_BREW_UNDEFINED_839,
  EVENT_BREW_UNDEFINED_840,
  EVENT_BREW_UNDEFINED_841,
  EVENT_BREW_UNDEFINED_842,
  EVENT_BREW_UNDEFINED_843,
  EVENT_BREW_UNDEFINED_844,
  EVENT_BREW_UNDEFINED_845,
  EVENT_BREW_UNDEFINED_846,
  EVENT_BREW_UNDEFINED_847,
  EVENT_BREW_UNDEFINED_848,
  EVENT_BREW_UNDEFINED_849,
  EVENT_BREW_UNDEFINED_850,
  EVENT_BREW_UNDEFINED_851,
  EVENT_BREW_UNDEFINED_852,
  EVENT_BREW_UNDEFINED_853,
  EVENT_BREW_UNDEFINED_854,
  EVENT_BREW_UNDEFINED_855,
  EVENT_BREW_UNDEFINED_856,
  EVENT_BREW_UNDEFINED_857,
  EVENT_BREW_UNDEFINED_858,
  EVENT_BREW_UNDEFINED_859,
  EVENT_BREW_UNDEFINED_860,
  EVENT_BREW_UNDEFINED_861,
  EVENT_BREW_UNDEFINED_862,
  EVENT_BREW_UNDEFINED_863,
  EVENT_BREW_UNDEFINED_864,
  EVENT_BREW_UNDEFINED_865,
  EVENT_BREW_UNDEFINED_866,
  EVENT_BREW_UNDEFINED_867,
  EVENT_BREW_UNDEFINED_868,
  EVENT_BREW_UNDEFINED_869,
  EVENT_BREW_UNDEFINED_870,
  EVENT_BREW_UNDEFINED_871,
  EVENT_BREW_UNDEFINED_872,
  EVENT_BREW_UNDEFINED_873,
  EVENT_BREW_UNDEFINED_874,
  EVENT_BREW_UNDEFINED_875,
  EVENT_BREW_UNDEFINED_876,
  EVENT_BREW_UNDEFINED_877,
  EVENT_BREW_UNDEFINED_878,
  EVENT_BREW_UNDEFINED_879,
  EVENT_BREW_UNDEFINED_880,
  EVENT_BREW_UNDEFINED_881,
  EVENT_BREW_UNDEFINED_882,
  EVENT_BREW_UNDEFINED_883,
  EVENT_BREW_UNDEFINED_884,
  EVENT_BREW_UNDEFINED_885,
  EVENT_BREW_UNDEFINED_886,
  EVENT_BREW_UNDEFINED_887,
  EVENT_BREW_UNDEFINED_888,
  EVENT_BREW_UNDEFINED_889,
  EVENT_BREW_UNDEFINED_890,
  EVENT_BREW_UNDEFINED_891,
  EVENT_BREW_UNDEFINED_892,
  EVENT_BREW_UNDEFINED_893,
  EVENT_BREW_UNDEFINED_894,
  EVENT_BREW_UNDEFINED_895,

  EVENT_WCDMA_PS_DATA_RATE,                  /* 2 byte payload */
  EVENT_GSM_TO_WCDMA_RESELECT_END,           /* 5 byte payload */
  EVENT_PZID_HAI_ENABLED,                    /* No payload*/
  EVENT_PZID_HAI_DISABLED,                   /* No payload*/
  EVENT_GSM_TO_WCDMA_HANDOVER_START,         /* 4 byte payload */
  EVENT_WCDMA_RRC_MODE,                      /* 1 byte payload */
  EVENT_WCDMA_L1_ACQ_SUBSTATE,               /* 1 byte payload */
  EVENT_WCDMA_PHYCHAN_CFG_CHANGED,           /* 1 byte payload */

  EVENT_QTV_CLIP_STARTED,                    /* 7 byte payload */
  EVENT_QTV_CLIP_ENDED,                      /* 5 byte payload */
  EVENT_QTV_SDP_PARSER_REJECT,               /* No payload */
  EVENT_QTV_CLIP_PAUSE,                      /* 4 byte payload */
  EVENT_QTV_CLIP_REPOSITIONING,              /* 4 byte payload */
  EVENT_QTV_CLIP_ZOOM_IN,                    /* No payload */
  EVENT_QTV_CLIP_ZOOM_OUT,                   /* No payload */
  EVENT_QTV_CLIP_ROTATE,                     /* 4 byte payload */
  EVENT_QTV_CLIP_PAUSE_RESUME,               /* 4 byte payload */
  EVENT_QTV_CLIP_REPOSITION_RESUME,          /* 4 byte payload */
  EVENT_QTV_DSP_INIT,                        /* No payload */
  EVENT_QTV_STREAMING_SERVER_URL,            /* 22 byte payload */
  EVENT_QTV_SERVER_PORTS_USED,               /* 4 byte payload */
  EVENT_QTV_USING_PROXY_SERVER,              /* 6 byte payload */
  EVENT_QTV_STREAMER_STATE_IDLE,             /* No payload */
  EVENT_QTV_STREAMER_STATE_CONNECTING,       /* No payload */
  EVENT_QTV_STREAMER_STATE_SETTING_TRACKS,   /* No payload */
  EVENT_QTV_STREAMER_STATE_STREAMING,        /* No payload */
  EVENT_QTV_STREAMER_STATE_PAUSED,           /* No payload */
  EVENT_QTV_STREAMER_STATE_SUSPENDED,        /* No payload */
  EVENT_QTV_STREAMER_CONNECTED,              /* No payload */
  EVENT_QTV_STREAMER_INITSTREAM_FAIL,        /* No payload */
  EVENT_QTV_BUFFERING_STARTED,               /* 5 byte payload */
  EVENT_QTV_BUFFERING_ENDED,                 /* 5 byte payload */
  EVENT_QTV_CLIP_FULLSCREEN,                 /* No payload */
  EVENT_QTV_PS_DOWNLOAD_STARTED,             /* 8 byte payload */
  EVENT_QTV_PSEUDO_STREAM_STARTED,           /* No Payload */
  EVENT_QTV_PS_PLAYER_STATE_PSEUDO_PAUSE,    /* No payload */
  EVENT_QTV_PS_PLAYER_STATE_PSEUDO_RESUME,   /* 4 byte payload */
  EVENT_QTV_PARSER_STATE_READY,              /* 14 byte payload */
  EVENT_QTV_FRAGMENT_PLAYBACK_BEGIN,         /* 2 byte payload */
  EVENT_QTV_FRAGMENT_PLAYBACK_COMPLETE,      /* 2 byte payload */
  EVENT_QTV_PARSER_STATE_PSEUDO_PAUSE,       /* No payload */
  EVENT_QTV_PLAYER_STATE_PSEUDO_PAUSE,       /* No payload */
  EVENT_QTV_PARSER_STATE_PSEUDO_RESUME,      /* 4 byte payload */
  EVENT_QTV_PLAYER_STATE_PSEUDO_RESUME,      /* 4 byte payload */
  EVENT_QTV_FRAGMENTED_FILE_DECODE_START,    /* 2 byte payload */
  EVENT_QTV_FRAGMENTED_FILE_END_SUCCESS,     /* 2 byte payload */
  EVENT_QTV_DOWNLOAD_DATA_REPORT,            /* 4 byte payload */
  EVENT_QTV_VDEC_DIAG_DECODE_CALLBACK,       /* 5 byte payload */
  EVENT_QTV_URL_PLAYED_IS_MULTICAST,         /* No payload */
  EVENT_QTV_VDEC_DIAG_STATUS,                /* 4 byte payload */
  EVENT_QTV_STREAMING_URL_OPEN,              /* 4 byte payload */
  EVENT_QTV_STREAMING_URL_OPENING,           /* No payload */
  EVENT_QTV_CLIP_ENDED_VER2,                 /* 13 byte payload */
  EVENT_QTV_SILENCE_INSERTION_STARTED,       /* No payload */
  EVENT_QTV_SILENCE_INSERTION_ENDED,         /* 8 byte payload */
  EVENT_QTV_AUDIO_CHANNEL_SWITCH_FRAME,      /* 8 byte payload */
  EVENT_QTV_FIRST_VIDEO_FRAME_RENDERED,      /* No payload */
  EVENT_QTV_FIRST_VIDEO_I_FRAME_RENDERED,    /* No payload */
  EVENT_QTV_SDP_SELECTED,                    /* No payload */
  EVENT_QTV_DIAG_PLAYER_STATUS,              /* 12 byte payload */
  EVENT_QTV_SILENCE_INSERTION_DURATION,      /* 4 byte payload */
  EVENT_QTV_UNDEFINED_957,
  EVENT_QTV_UNDEFINED_958,
  EVENT_QTV_UNDEFINED_959,
  EVENT_QTV_UNDEFINED_960,
  EVENT_QTV_UNDEFINED_961,
  EVENT_QTV_UNDEFINED_962,
  EVENT_QTV_UNDEFINED_963,
  EVENT_QTV_UNDEFINED_964,
  EVENT_QTV_UNDEFINED_965,
  EVENT_QTV_UNDEFINED_966,
  EVENT_QTV_UNDEFINED_967,

  EVENT_DS_SETS_ARM_CLOCK_FASTER,      /* No payload */
  EVENT_DS_SETS_ARM_CLOCK_SLOWER,      /* No payload */

  EVENT_SMS_STATISTICS,                /* 2 byte payload */
  EVENT_SM_PDP_STATE,                  /* 4 byte payload */
  EVENT_MVS_STATE,                     /* 2 byte payload */

  EVENT_SECSSL,                        /* 16 byte payload */
  EVENT_SECTEST,                       /* 16 byte payload */
  EVENT_SECVPN,                        /* 16 byte payload */
  EVENT_SECCRYPT,                      /* 16 byte payload */
  EVENT_SECCRYPT_CMD,                  /* 16 byte payload */

  EVENT_SEC_RESERVED_978,              /* unknown payload */
  EVENT_SEC_RESERVED_979,              /* unknown payload */
  EVENT_SEC_RESERVED_980,              /* unknown payload */
  EVENT_SEC_RESERVED_981,              /* unknown payload */

  EVENT_ARM_CLK_FREQUENCY_CHANGE,      /* 12 byte payload */
  EVENT_ADSP_CLK_FREQUENCY_CHANGE,     /* 4 byte payload */
  EVENT_MDSP_CLK_FREQUENCY_CHANGE,     /* 4 byte payload */

  EVENT_CELL_CHANGE_INDICATION,        /* 1 byte payload */
  EVENT_CB_STATE_CHANGE,               /* 4 byte payload */
  EVENT_SMSCB_L1_STATE_CHANGE,         /* 3 byte payload */
  EVENT_SMSCB_L1_COLLISION,            /* 1 byte payload */
  EVENT_WMS_SEARCH_REQUEST,            /* 1 byte payload */
  EVENT_CM_GET_PASSWORD_IND,           /* 2 byte payload */
  EVENT_CM_PASSWORD_AUTHENTICATION_STATUS, /* 2 byte payload */
  EVENT_CM_USS_RESPONSE_NOTIFY_IND,    /* 3 byte payload */
  EVENT_CM_USS_CONF,                   /* 4 byte payload */
  EVENT_CM_RELEASE_USS_IND,            /* 4 byte payload */
  EVENT_CM_FWD_AOC_IND,                /* 1 byte payload */
  EVENT_PZID_ID,                       /* 2 byte payload */
  EVENT_PZID_HT_VALUE,                 /* 9 byte payload */
  EVENT_PZID_EXISTS_IN_LIST,           /* 1 byte payload */
  EVENT_GSDI_GET_FILE_ATTRIBUTES,      /* 6 byte payload */
  EVENT_GSDI_SIM_READ,                 /* 6 byte payload */
  EVENT_GSDI_SIM_WRITE,                /* 6 byte payload */
  EVENT_GSDI_GET_PIN_STATUS,           /* 8 byte payload */
  EVENT_GSDI_VERIFY_PIN,               /* 7 byte payload */
  EVENT_GSDI_UNBLOCK_PIN,              /* 7 byte payload */
  EVENT_GSDI_DISABLE_PIN,              /* 7 byte payload */
  EVENT_GSDI_ENABLE_PIN,               /* 7 byte payload */
  EVENT_GSDI_SIM_INCREASE,             /* 6 byte payload */
  EVENT_GSDI_EXECUTE_APDU_REQ,         /* 6 byte payload */
  EVENT_SEG_UPM_ADDR_MISMATCH,         /* 2 byte payload */
  EVENT_WCDMA_PRACH,                   /* 3 byte payload */
  EVENT_GSDI_SELECT,                   /* 6 byte payload */
  EVENT_WCDMA_RAB_RATE_RECONFIG,       /* 2 byte payload */
  EVENT_WCDMA_RLC_RESETS,              /* 3 byte payload */
  EVENT_WCDMA_RLC_OPEN_CLOSE,          /* 2 byte payload */
  EVENT_WCDMA_RLC_MRW,                 /* 3 byte payload */
  EVENT_QVP_APP_PROCESS_EVENT,         /* 2 byte payload */
  EVENT_QVP_APP_STATE_CHANGED_EVENT,   /* 2 byte payload */
  EVENT_QVP_APP_CALL_CONNECTED_EVENT,  /* 1 byte payload */
  EVENT_GSDI_CARD_EVENT_NOTIFICATION,  /* 4 byte payload */
  EVENT_CM_DATA_AVAILABLE,             /* 1 byte payload */
  EVENT_CM_DS_INTERRAT_STATE,          /* 2 byte payload */
  EVENT_MM_STATE,                      /* 2 byte payload */
  EVENT_GMM_STATE,                     /* 2 byte payload */
  EVENT_PLMN_INFORMATION,              /* 8 byte payload */
  EVENT_COREAPP_SET_VOICE_PRIVACY,     /* 5 byte payload */
  EVENT_COREAPP_GET_VOICE_PRIVACY,     /* 5 byte payload */
  EVENT_HARD_HANDOFF_LONG_CODE_MASK_CHANGE, /* 14 byte payload */
  EVENT_VCTCXO_FREEZE,                 /* payload */
  EVENT_VCTCXO_UNFREEZE,               /* payload */
  EVENT_SMS_SLOT_WAKEUP_V2,            /* 2 byte payload */
  EVENT_QVP_RCVD_FIRST_VIDEO_FRAME,    /* no payload */
  EVENT_QVP_CALL_RELEASED,             /* 8 byte payload */
  EVENT_CB_SMS_NOTIFY,                 /* 10 byte payload */
  EVENT_GPS_PDSM_EVENT_REPORT,         /* 6 byte payload */
  EVENT_LONG_CODE_MASK_CHANGED,        /* 2 byte payload */
  EVENT_DS707,                         /* 5 byte payload */

  EVENT_GSDI_ACTIVATE_FEATURE_IND,     /* 8 byte payload */
  EVENT_GSDI_DEACTIVATE_FEATURE_IND,   /* 8 byte payload */
  EVENT_GSDI_GET_FEATURE_IND,          /* 11 byte payload */
  EVENT_GSDI_SET_FEATURE_DATA,         /* 6 byte payload */
  EVENT_GSDI_UNBLOCK_FEATURE_IND,      /* 8 byte payload */
  EVENT_GSDI_GET_CONTROL_KEY,          /* 6 byte payload */
  EVENT_GSDI_OTA_DEPERSO,              /* 26 byte payload */
  EVENT_GSDI_GET_PERM_FEATURE_IND,     /* 11 byte payload */
  EVENT_GSDI_PERM_DISBALE_FEATURE_IND, /* 8 byte payload */
  EVENT_GSM_L1_VOCODER_INITIALIZE,     /* TBD */
  EVENT_GSM_L1_ALIGN_VFR,              /* TBD */
  EVENT_GSM_L1_VOCODER_ENABLED,        /* TBD */
  EVENT_HDR_AMAC_PERSISTENCE_FAILED,   /* no payload */
  EVENT_HDR_AMAC_PERSISTENCE_PASSED,   /* no payload */

  /* 20 events reserved for MediaFLO */
  EVENT_MFLO_STREAM_STATE,                /* 12 byte payload */
  EVENT_MFLO_CONTROL_CHANNEL_STATE_CHANGE,/* 12 byte payload */
  EVENT_MFLO_SLEEP_STATE_CHANGE,          /* 12 byte payload */
  EVENT_MFLO_NETWORK_STATE_CHANGE,        /* 20 byte payload */
  EVENT_MFLO_TRANS_STATE,                 /* 12 byte payload */
  EVENT_MFLO_OIS_STATE,                   /* 16 byte payload */
  EVENT_MFLO_RXD_STATE,                   /* 12 byte payload */
  EVENT_MFLO_HIPRI_STATE_CHANGE,          /* 8  byte payload */
  EVENT_MFLO_CAS_STATE,                   /* 12 byte payload */
  EVENT_MFLO_ACQ_STATE,                   /* 8  byte payload */
  EVENT_MFLO_OSCAR_FRAME_DECODED,         /* 8  byte payload */
  EVENT_MFLO_CHAN_SWITCH_RENDERED,        /* 16 byte payload */
  EVENT_MFLO_OSCAR_DEC_EXCEPTION_DETECTED,/* 4  byte payload */
  EVENT_MFLO_MFN_SUBSTATE,                /* 8  byte payload */
  EVENT_MFLO_MFN_STATE,                   /* 12  byte payload */
  EVENT_MFLO_MFN_VERTICAL_HANDOFF,        /* 16  byte payload */
  EVENT_MFLO_MFN_ACQ_STATE,               /* 10  byte payload */
  EVENT_MFLO_FLOW_STATUS,                 /* 12  byte payload */
  EVENT_MFLO_NETWORK_STATUS,					/* 12  byte payload */
  EVENT_MFLO_UNDEFINED_1070,

  EVENT_CM_LCS_MOLR_CONF,                 /* 1 byte payload */
  EVENT_PPP_NETMODEL,                     /* 7 byte payload */
  EVENT_CAMERA_PROFILING,                 /* 1 byte payload */
  EVENT_MAC_HS_T1_EXPIRY,                 /* 2 byte payload */
  EVENT_ASYNC_DS707,                      /* 4 byte payload */
  EVENT_PKT_DS707,                        /* 4 byte payload */
  EVENT_GPRS_TIMER_EXPIRY,                /* 1 byte payload */
  EVENT_GPRS_MAC_IDLE_IND,                /* no payload */
  EVENT_GPRS_PACKET_CHANNEL_REQUEST,      /* 1 byte payload */
  EVENT_GPRS_ACCESS_REJECT,               /* 1 byte payload */
  EVENT_GPRS_PACKET_RESOURCE_REQUEST,     /* 1 byte payload */
  EVENT_GPRS_PACKET_UPLINK_ASSIGNMENT,    /* 2 byte payload */
  EVENT_GPRS_PACKET_DOWNLINK_ASSIGNMENT,  /* 2 byte payload */
  EVENT_PACKET_TIMESLOT_RECONFIGURE,      /* 3 byte payload */
  EVENT_GPRS_TBF_RELEASE,                 /* 1 byte payload */
  EVENT_GPRS_CELL_CHANGE_ORDER,           /* 1 byte payload */
  EVENT_GPRS_CELL_CHANGE_FAILURE,         /* 1 byte payload */
  EVENT_GSM_AMR_RATSCCH_REQ,              /* 1 byte payload */
  EVENT_GSM_AMR_RATSCCH_RSP,              /* 1 byte payload */
  EVENT_SD_SRV_IND_HYBR_WLAN,
  EVENT_SD_EVENT_ACTION_HYBR_WLAN,
  EVENT_GPS_PD_DEMOD_SESS_START,          /* 5 byte payload */
  EVENT_GPS_PD_DEMOD_SESS_END,            /* 1 byte payload */
  EVENT_GPS_SV_ACQUIRED,                  /* 4 byte payload */
  EVENT_GPS_SV_BIT_EDGE_FOUND,            /* 4 byte payload */
  EVENT_GPS_DEMOD_STARTED,                /* 4 byte payload */
  EVENT_GPS_DEMOD_OUT_OF_LOCK,            /* 3 byte payload */
  EVENT_GPS_DEMOD_STOPPED,                /* 3 byte payload */
  EVENT_GPS_DEMOD_PREAMBLE_FOUND,         /* 3 byte payload */
  EVENT_GPS_DEMOD_FRAME_SYNC_STATUS,      /* 4 byte payload */
  EVENT_GPS_DEMOD_SUBFRAME,               /* 6 byte payload */
  EVENT_GPS_DEMOD_EPHEMERIS_COMPLETE,     /* 1 byte payload */
  EVENT_GPS_DEMOD_ALMANAC_COMPLETE,       /* 1 byte payload */
  EVENT_GPS_DEMOD_BIT_EDGE_STATUS,        /* 4 byte payload */
  EVENT_RAT_CHANGE,                       /* 1 byte payload */
  EVENT_REGISTRATION_SUPPRESSED,          /* 1 byte payload */
  EVENT_HDR_RUP_DIST_BASED_REG,           /* 3 byte payload */
  EVENT_GPS_DIAG_APP_TRACKING_START,      /* 4 byte payload */
  EVENT_GPS_DIAG_APP_TRACKING_END,        /* 12 byte payload */
  EVENT_GPS_DIAG_APP_POSITION_SUCCESS,    /* 16 byte payload */
  EVENT_GPS_DIAG_APP_POSITION_FAILURE,    /* 6 byte payload */
  EVENT_GSM_AMR_MULTIRATE_IE,             /* 9 byte payload */
  EVENT_EPZID_HYSTERESIS_ENABLED,         /* no payload */
  EVENT_EPZID_HYSTERESIS_DISABLED,        /* no payload */
  EVENT_EPZID_HT_STARTED,                 /* 10 byte payload */
  EVENT_EPZID_HT_EXPIRED,                 /* 10 byte payload */
  EVENT_HDR_BCMCS_FLOW_STATE_CHANGE,      /* 6 byte payload */
  EVENT_HDR_LMAC_UPDATE_BC_STATUS,        /* 1 byte payload */
  EVENT_DS_CAM_TIMER,                     /* 5 byte payload */
  EVENT_DS_RDUD_TIMER,                    /* 5 byte payload */
  EVENT_DS_CTA_TIMER,                     /* 8 bytes payload */
  EVENT_DS_FALLBACK,                      /* 1 byte payload */
  EVENT_DS3G_CAM_FLOW_CTRL_TIMER,         /* 5 byte payload */
  EVENT_GPS_JAMMER_DETECTION_TEST_PASS,   /* no payload */
  EVENT_GPS_JAMMER_DETECTION_TEST_FAILURE,/* 8 byte payload */
  EVENT_JAMMER_DETECT_NOISE_STATS,        /* 8 byte payload */
  EVENT_GPS_GET_PARAM,                    /* 8 byte payload */
  EVENT_GPS_GET_PARAM_BS_INFO,            /* 18 byte payload */
  EVENT_HS_SERVING_CELL_CHANGE,           /* 8 byte payload */
  EVENT_HS_DSCH_STATUS,                   /* 1 byte payload */
  EVENT_SMGMM_REQUEST_SENT,               /* 2 byte payload */
  EVENT_SMGMM_REJECT_RECEIVED,            /* 2 byte payload */
  EVENT_LINUX_APP_STOP,                   /* 8 byte payload */
  EVENT_GPS_PD_CME_SESSION_START,         /* 1 byte payload */
  EVENT_GPS_PD_CME_SESSION_END,           /* 1 byte payload */

  /* 20 events reserved for QVideoPhone */
  EVENT_SIP_REGISTER_START,               /* 4 byte payload */
  EVENT_SIP_REGISTER_DONE,                /* 1 byte payload */
  EVENT_SIP_CALL_SETUP_START,             /* 5 byte payload */
  EVENT_SIP_CALL_SETUP_DONE,              /* No payload */
  EVENT_SIP_CALL_RELEASE_START,           /* 5 byte payload */
  EVENT_SIP_CALL_RELEASE_DONE,            /* No payload */
  EVENT_AUDIO_FRAME_SENT_TO_DECODER,      /* 12 byte payload */
  EVENT_VIDEO_FRAME_SENT_TO_DECODER,      /* 12 byte payload */
  EVENT_DEC_RENDER_FRAME,                 /* 8 byte payload */
  EVENT_DEC_RENDER_DONE,                  /* No payload */
  EVENT_DEC_START_DECODING,               /* 4 byte payload */
  EVENT_DEC_FRAME_DECODED,                /* 4 byte payload */
  EVENT_V_ENCODED,                        /* 13 byte payload */
  EVENT_DEC_START_DECODING_EXT,           /* 8 byte payload */
  EVENT_DEC_FRAME_DECODED_EXT,            /* 8 byte payload */
  EVENT_QVIDEOPHONE_UNDEFINED_1151,
  EVENT_QVIDEOPHONE_UNDEFINED_1152,
  EVENT_QVIDEOPHONE_UNDEFINED_1153,
  EVENT_QVIDEOPHONE_UNDEFINED_1154,
  EVENT_QVIDEOPHONE_UNDEFINED_1155,

  EVENT_GPS_CME_POS_REQ,                  /* no payload */
  EVENT_GPS_CME_FIX_START,                /* no payload */
  EVENT_GPS_CME_FIX_END,                  /* no payload */
  EVENT_GPS_SEED_CLM,                     /* 12 byte payload */
  EVENT_GPS_SEED_SID,                     /* 10 byte payload */
  EVENT_GPS_SEED_SL,                      /* 11 byte payload */
  EVENT_GPS_SEED_GET,                     /* 13 byte payload */

  EVENT_HDR_OVHD_BC_MSG_RX,               /* no payload */
  EVENT_HDR_OVHD_T_BC_SUPERVISION,        /* no payload */
  EVENT_HDR_LMAC_SET_BCMCS_PAGE_CYCLE,    /* 1 byte payload */
  EVENT_HDR_HMP_SESSION_CLOSED,           /* 2 byte payload */

  EVENT_WLAN_CP,                          /* 15 byte payload */
  EVENT_ARP,                              /* 12 byte payload */
  EVENT_DHCP,                             /* 10 byte payload */
  EVENT_WLAN_WPA,                         /* 7 byte payload */
  EVENT_EAP,                              /* 7 byte payload */
  EVENT_LAN_1X,                           /* 7 byte payload */

  EVENT_CAMERA_SVCS_START,                /* no payload */
  EVENT_CAMERA_SVCS_STOP,                 /* no payload */

  EVENT_BCMCS_SRVC_AVAILABLE,             /* 1 byte payload */
  EVENT_BCMCS_SRVC_LOST,                  /* 1 byte payload */
  EVENT_BCMCS_FLOW_REGISTERED,            /* 18 byte payload */
  EVENT_BCMCS_FLOW_DEREGISTERED,          /* 18 byte payload */
  EVENT_BCMCS_FLOW_STATUS_CHANGED,        /* 19 byte payload */

  EVENT_CAMERA_SVCS_X,                    /* 2 byte payload */
  EVENT_CM_CALL_EVENT_ORIG_THR,           /* 3 byte payload */

  EVENT_VFE_MSG_CONFIG_COMPLETE,          /* No payload */
  EVENT_VFE_MSG_IDLE_COMPLETE,            /* No payload */
  EVENT_VFE_MSG_UPDATE_COMPLETE,          /* No payload */
  EVENT_VFE_MSG_AE_AWB_STATS,             /* No payload */
  EVENT_DSP_VIDEO_ENC_DOWNLOAD_DONE,      /* No payload */
  EVENT_DSP_VIDEO_ENC_SELECTION_DONE,     /* No payload */
  EVENT_DSP_VIDEO_ENC_CONFIG_DONE,        /* No payload */
  EVENT_DSP_VIDEO_ENC_FRAME_DONE,         /* No payload */

  EVENT_HDR_OVHD_BCMCS_CHAN_CHANGE,       /* 6 byte payload */

  EVENT_QVS_REGISTER_START,               /* 4 byte payload */
  EVENT_QVS_REGISTER_DONE,                /* 4 byte payload */
  EVENT_QVS_REGISTER_FAILED,              /* No payload */
  EVENT_QVS_CALL_SETUP_START,             /* 5 byte payload */
  EVENT_QVS_CALL_SETUP_DONE,              /* No payload */
  EVENT_QVS_CALL_SETUP_FAILED,            /* No payload */
  EVENT_QVS_CALL_RELEASE_START,           /* 5 byte payload */
  EVENT_QVS_CALL_RELEASE_DONE,            /* No payload */
  EVENT_QVS_CALL_RELEASE_FAILED,          /* No payload */

  EVENT_CAMCORDER_START_RECORD,           /* 9 byte payload */
  EVENT_CAMCORDER_START_TRANSCODE,        /* 6 byte payload */
  EVENT_CAMCORDER_FRAME_DROP,             /* No payload */
  EVENT_CAMCORDER_AUDIODUB,               /* 2 byte payload */

  EVENT_PSMM_SENT,                        /* 16 byte payload */
  EVENT_GPS_PD_FALLBACK_MODE,             /* 3 byte payload */

  EVENT_PEAP,                             /* 4 byte payload */
  EVENT_TTLS,                             /* 3 byte payload */
  EVENT_TLS,                              /* 2 byte payload */

  EVENT_WCDMA_TO_WCDMA_RESELECTION_VER2_START, /* 7 byte payload */

  EVENT_EUL_RECONFIG_OR_ASU,              /* 10 byte payload */
  EVENT_EUL_SERVING_CELL_CHANGE,          /* 4 byte payload */
  EVENT_EUL_PHYSICAL_LAYER_RECONFIG,      /* 10 byte payload */

  EVENT_DRM_ROAP_TRIGGER_RECIEVED,        /* 1 byte payload */
  EVENT_DRM_ROAP_PROTOCOL_START,          /* 1 byte payload */
  EVENT_DRM_ROAP_REQUEST,                 /* 1 byte payload */
  EVENT_DRM_ROAP_REQUEST_EXTENSION,       /* 2 byte payload */
  EVENT_DRM_ROAP_RESPONSE,                /* 2 byte payload */
  EVENT_DRM_ROAP_RESPONSE_EXTENSION,      /* 2 byte payload */
  EVENT_DRM_ROAP_RI_CONTEXT,              /* 1 byte payload */
  EVENT_DRM_ROAP_ERROR,                   /* 1 byte payload */
  EVENT_DRM_ROAP_RSP_VALIDATION,          /* 3 byte payload */
  EVENT_DRM_ROAP_PROTOCOL_END,            /* 2 byte payload */

  EVENT_DS_WMK_ALLOCATED,                 /* 16 byte payload */
  EVENT_DS_WMK_DEALLOCATED,               /* 8  byte payload */
  EVENT_DS_WMK_FLUSHED,                   /* 12 byte payload */
  EVENT_DS_WMK_FLOW_ENABLED,              /* 12 byte payload */
  EVENT_DS_WMK_FLOW_DISABLED,             /* 12 byte payload */

  EVENT_HDR_IDLE_SET_SLEEP_DURATION,      /* 2 byte payload */
  EVENT_HDR_SCM_SESSION_CHANGED,          /* 1 byte payload */

  EVENT_UMTS_TO_CDMA_DATA_HANDOVER,       /* No payload */
  EVENT_UMTS_TO_CDMA_VOICE_HANDOVER,      /* 18 byte payload */

  EVENT_MO_SMS_RETRY_ATTEMPT,             /* 14 byte payload */

  EVENT_HDR_LMAC_UPDATE_QSM_STATUS,       /* 1 byte payload */

  EVENT_CM_CELL_SRV_IND,                  /* 5 byte payload */

  EVENT_RLP_NAK_ABORT,                    /* 9 byte payload */

  EVENT_DRM_RIGHTS_OPERATION,             /* 2 byte payload */

  EVENT_DS_RESV_MSG_SENT_REV_FLOWS,       /* 15 byte payload */
  EVENT_DS_RESV_MSG_SENT_FWD_FLOWS,       /* 15 byte payload */
  EVENT_DS_RESV_RESP_SUCCESS_RECD,        /* 4 byte payload */
  EVENT_DS_RESV_RESP_FAILURE_RECD,        /* 4 byte payload */

  EVENT_GPS_PD_COMM_FAILURE,              /* 2 byte payload */
  EVENT_GPS_PD_COMM_DONE,                 /* No payload */
  EVENT_GPS_PD_EVENT_END,                 /* 1 byte payload */
  EVENT_GPS_PA_EVENT_CALLBACK,            /* 1 byte payload */
  EVENT_GPS_PD_CMD_ERR_CALLBACK,          /* 2 byte payload */
  EVENT_GPS_PA_CMD_ERR_CALLBACK,          /* 2 byte payload */

  EVENT_GPS_LM_ENTER_SA_RF_VERIF,         /* 1 byte payload */
  EVENT_GPS_LM_EXIT_SA_RF_VERIF,          /* 1 byte payload */
  EVENT_GPS_LM_ERROR_SA_RF_VERIF,         /* 1 byte payload */
  EVENT_GPS_LM_PD_COMPLETE,               /* No payload */
  EVENT_GPS_LM_IQ_TEST_COMPLETE,          /* No payload */

  EVENT_PM_APP_OTG_INIT,                                    /* No payload */
  EVENT_PM_APP_OTG_RESET,                                   /* No payload */
  EVENT_PM_APP_OTG_ACQUIRE_BUS_REQ,                         /* 2 byte payload */
  EVENT_PM_APP_OTG_RELINQUISH_BUS_REQ,                      /* No payload */
  EVENT_PM_APP_OTG_SUSPEND,                                 /* No payload */
  EVENT_PM_APP_OTG_RESUME,                                  /* No payload */
  EVENT_PM_APP_OTG_DEVICE_ATTACHED,                         /* 1 byTe payload */
  EVENT_PM_APP_OTG_DEVICE_DETACHED,                         /* No payload */
  EVENT_PM_APP_OTG_HOST_MODE_REM_PERI_DIS,                  /* No payload */
  EVENT_PM_APP_OTG_PERI_MODE_PREPARE_FOR_REM_HOST_WAKEUP_SIG,  /* No payload */
  EVENT_PM_APP_OTG_PERI_MODE_REM_HOST_WAKEUP_SIG_DONE,         /* No payload */
  EVENT_PM_APP_OTG_SET_REM_WAKEUP_CAPABILITY,                  /* 1 byte payload */
  EVENT_PM_APP_OTG_OPERATIONAL_ERROR,                          /* 1 byte Payload */
  EVENT_PM_APP_OTG_CONFIGURE_USB_POWER_CONSUMER,               /* No payload */
  EVENT_PM_APP_OTG_SET_USB_POWER_CONSUMPTION_REQUIREMENT,      /* 1 byte payload */
  EVENT_PM_APP_OTG_PERI_MODE_PROCESS_USB_POWER_LINE_CONT_REQ,  /* 1 byte payload */
  EVENT_PM_APP_OTG_PERI_MODE_SET_REM_A_DEV_INFO,               /* 3 byte payload */
  EVENT_PM_APP_OTG_STATE_TRANSITION,                           /* 2 byte payload */

  EVENT_DTV_TABLE_ACQ_SUCCESS,                                 /* 10 byte payload */
  EVENT_DTV_TABLE_ACQ_FAIL,                                    /* 5 byte payload */
  EVENT_DTV_DVBH_SEL_PLTFM_REQ_RCVD,                           /* 4 byte payload */
  EVENT_DTV_DVBH_PLTFM_ACQ_SUCCESS,                            /* 4 byte payload */
  EVENT_DTV_DVBH_PLTFM_ACQ_FAIL,                               /* 4 byte payload */
  EVENT_DTV_DVBH_TBL_MGR_STATE_CHANGED,                        /* 2 byte payload */
  EVENT_DTV_DVBH_CE_STATE_CHANGED,                             /* 2 byte payload */
  EVENT_DTV_DVBH_MCAST_JOIN_REQ_RCVD,                          /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_REQ_RCVD,                         /* 18 byte payload */
  EVENT_DTV_DVBH_INIT_REQ_RCVD,                                /* No payload */
  EVENT_DTV_DVBH_MCAST_JOIN_SUCCESS,                           /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_JOIN_FAILURE,                           /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_SUCCESS,                          /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_FAILURE,                          /* 18 byte payload */
  EVENT_DTV_DVBH_INIT_SUCCESS,                                 /* No payload */
  EVENT_DTV_DVBH_INIT_FAILURE,                                 /* No payload */

  EVENT_GPS_LM_SESSION_START,             /* 1 byte payload */
  EVENT_GPS_LM_SESSION_END,               /* No payload */
  EVENT_GPS_LM_FIX_REQUEST_START,         /* No payload */
  EVENT_GPS_LM_FIX_REQUEST_END,           /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_START,         /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_END,           /* No payload */
  EVENT_GPS_LM_SESSION_CONTINUE,          /* 1 byte payload */
  EVENT_GPS_LM_FIX_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_PPM_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_AIDING_DATA_RECEIVED,      /* 1 byte payload */
  EVENT_GPS_LM_RC_ON_TIMER_TIMEOUT,       /* No payload */
  EVENT_GPS_LM_SHUT_OFF_TIMER_TIMEOUT,    /* No payload */
  EVENT_GPS_LM_MGP_ON,                    /* No payload */
  EVENT_GPS_LM_MGP_IDLE,                  /* No payload */
  EVENT_GPS_LM_MGP_OFF,                   /* No payload */

  EVENT_DRM_RO_CONSUMPTION_VALIDATION,    /* 2 byte payload */
  EVENT_DRM_RO_INSTALLATION_VALIDATION,   /* 2 byte payload */

  EVENT_FLUTE_FDT_INST_RCVD,              /* 8 byte payload */
  EVENT_FLUTE_FDT_INST_RCV_FAIL,          /* 9 byte payload */
  EVENT_FLUTE_FDT_INST_EXPIRED,           /* 8 byte payload */
  EVENT_FLUTE_JOIN_SESSION_REQ_RCVD,      /* 24 byte payload */
  EVENT_FLUTE_LEAVE_SESSION_REQ_RCVD,     /* 4 byte payload */
  EVENT_FLUTE_SESSION_CLOSED,             /* 5 byte payload */
  EVENT_FLUTE_SESSION_CLOSED_BY_APP,      /* 4 byte payload */
  EVENT_FLUTE_B_FLAG_RCVD,                /* 8 byte payload */
  EVENT_FLUTE_GET_FILE_REQUEST_RCVD,      /* 8 byte payload */
  EVENT_FLUTE_JOIN_SESSION_RSP,           /* 6 byte payload */
  EVENT_FLUTE_FILE_STATUS_RSP,            /* 16 byte payload */
  EVENT_FLUTE_CANCEL_FILE_REQ_RCVD,       /* 8 byte payload */

  EVENT_DTV_DVBH_DEINIT_REQ_RCVD,         /* No payload */
  EVENT_DTV_DVBH_DEINIT_SUCCESS,          /* No payload */
  EVENT_DTV_DVBH_DEINIT_FAILURE,          /* No payload */

  EVENT_CONTENT_INSTALL_BEGIN,            /* No payload */
  EVENT_CONTENT_INSTALL_COMPLETE,         /* 2 byte payload */
  EVENT_CONTENT_RETRIEVAL_BEGIN ,         /* No payload */
  EVENT_CONTENT_RETRIEVAL_COMPLETE,       /* 4 byte payload */
  EVENT_CONTENT_BACKUP_BEGIN,             /* No payload */
  EVENT_CONTENT_BACKUP_COMPLETE,          /* 2 byte payload */
  EVENT_CONTENT_FWD_BEGIN,                /* No payload */
  EVENT_CONTENT_FWD_COMPLETE,             /* 2 byte payload */

  EVENT_HARD_HANDOFF_VOIP_TO_CDMA,        /* 14 byte payload */

  EVENT_EAP_SIM_AKA,                      /* 14 byte payload */
  EVENT_WLAN_CP_MEAS,                     /* 16 byte payload */
  EVENT_WLAN_CP_HO,                       /* 13 byte payload */
  EVENT_WLAN_CP_11D,                      /* 9  byte payload */
  EVENT_WLAN_MC,                          /* 2  byte payload */

  EVENT_SVG_CONTENT_SET,                  /* 2  byte payload */
  EVENT_SVG_CONTENT_PLAY,                 /* 2  byte payload */
  EVENT_SVG_CONTENT_RESUME,               /* 1  byte payload */
  EVENT_SVG_CONTENT_PAUSE,                /* 1  byte payload */
  EVENT_SVG_CONTENT_STOP,                 /* 1  byte payload */
  EVENT_SVG_CONTENT_USEREVENT,            /* 10 byte payload */
  EVENT_SVG_CONTENT_GETURIDATA,           /* 3  byte payload */
  EVENT_SVG_CONTENT_TRANSFORM,            /* 10 byte payload */
  EVENT_SVG_GET_PARAM,                    /* 3  byte payload */
  EVENT_SVG_SET_PARAM,                    /* 3  byte payload */

  EVENT_WLAN_WPA2,                        /* 5 byte payload */

  EVENT_WCDMA_PSC_SCANNER_STOP,           /* 1 byte payload */

  EVENT_MEDIA_PLAYER_KEYPRESS,            /* 4 byte payload */

  EVENT_WLAN_MC_QOS,                      /* 5 byte payload */

  EVENT_WCDMA_PSC_SCANNER_STATE,          /* 1 byte payload */

  EVENT_WLAN_CP_ADHOC,                    /* 16 byte payload */

  EVENT_DMB_STACK_SHUTDOWN,               /* 4 byte payload */
  EVENT_DMB_TUNE_DONE_SUCCESS,            /* 4 byte payload */
  EVENT_DMB_TUNE_DONE_FAILURE,            /* 4 byte payload */
  EVENT_DMB_SEARCH_DONE,                  /* 4 byte payload */
  EVENT_DMB_SCAN_DONE,                    /* 4 byte payload */
  EVENT_DMB_RECEPTION_INFO_CHANGED,       /* 4 byte payload */
  EVENT_DMB_DMB_GUIDE_CHANGED,            /* 4 byte payload */
  EVENT_DMB_LOCATION_INFO_CHANGED,        /* 4 byte payload */
  EVENT_DMB_LOST_ENSEMBLE,                /* 4 byte payload */
  EVENT_DMB_STREAM_TERMINATED,            /* 4 byte payload */
  EVENT_DMB_STREAM_DATA_AVAILABLE,        /* 4 byte payload */
  EVENT_DMB_RESERVED1,                    /* 4 byte payload */
  EVENT_DMB_RESERVED2,                    /* 4 byte payload */
  EVENT_DMB_RESERVED3,                    /* 4 byte payload */
  EVENT_DMB_RESERVED4,                    /* 4 byte payload */
  EVENT_DMB_RESERVED5,                    /* 4 byte payload */
  EVENT_DMB_RESERVED6,                    /* 4 byte payload */
  EVENT_DMB_RESERVED7,                    /* 4 byte payload */
  EVENT_DMB_RESERVED8,                    /* 4 byte payload */
  EVENT_DMB_RESERVED9,                    /* 4 byte payload */
  EVENT_DMB_RESERVED10,                   /* 4 byte payload */

  EVENT_MOBILEVIEW_RESERVED1,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED2,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED3,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED4,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED5,             /* TBD */

  EVENT_HDR_DOS_MO_DOS_STATUS,            /* 3 byte payload */

  EVENT_GPSONEXTRA_START_DOWNLOAD,        /* 4 byte payload */
  EVENT_GPSONEXTRA_END_DOWNLOAD,          /* 4 byte payload */

  EVENT_SNSD_GENERIC,                     /* 8 byte payload */
  EVENT_SNSD_DEVICE_INIT,                 /* 8 byte payload */
  EVENT_SNSD_DEVICE_CONFIGURED,           /* 8 byte payload */
  EVENT_SNSD_EVENT_DATA_READY,            /* 8 byte payload */
  EVENT_SNSD_EVENT_COND_MET,              /* 8 byte payload */
  EVENT_SNSD_DEVICE_DOWN,                 /* 8 byte payload */
  EVENT_SNSD_ERROR,                       /* 8 byte payload */

  EVENT_CM_COUNTRY_SELECTED,              /* 2 byte payload */
  EVENT_CM_SELECT_COUNTRY,                /* 7 byte payload */

  EVENT_GPS_DCME_NEW_SV_ADDED_IN_AA,      /* 1 byte payload */
  EVENT_GPS_DCME_SV_REMOVED_FROM_AA,      /* 1 byte payload */

  EVENT_ESG_GET_PROV_LIST_REQ_RCVD,       /* No payload */
  EVENT_ESG_GET_PROV_LIST_REQ_FAIL,       /* No payload */
  EVENT_ESG_PROV_LIST_AVAILABLE,          /* No payload */
  EVENT_ESG_ACQ_REQ_RCVD,                 /* 2 byte payload */
  EVENT_ESG_ACQ_REQ_FAIL,                 /* 2 byte payload */
  EVENT_ESG_STOP_REQ_RCVD,                /* No payload */
  EVENT_ESG_STOP_REQ_FAIL,                /* No payload */
  EVENT_ESG_STOP_COMPLETE,                /* 1 byte payload */

  EVENT_ADC_ONDIE_THERM_READ,             /* 2 byte payload */

  EVENT_CONTENT_NO_VALID_OR_EXPIRED_RIGHTS, /* No Payload */

  EVENT_MOBILEVIEW_RESERVED30,            /* EVENT TO BE REPLACED */
  EVENT_MOBILEVIEW_RESERVED31,            /* EVENT TO BE REPLACED */
  EVENT_MOBILEVIEW_RESERVED32,            /* EVENT TO BE REPLACED */

  EVENT_GPS_DCME_MEAS_CYCLE_START,        /* No payload */
  EVENT_GPS_DCME_MEAS_CYCLE_END,          /* No payload */
  EVENT_GPS_CME_ENGAGED,                  /* No payload */
  EVENT_GPS_CME_NOT_ENGAGED,              /* No payload */
  EVENT_GPS_DCME_ENGAGED,                 /* No payload */
  EVENT_GPS_DCME_NOT_ENGAGED,             /* No payload */

  EVENT_HS_USB_DEVICE_ATTACHED,           /* No payload */
  EVENT_HS_USB_HID_DISCONECT,             /* No payload */
  EVENT_HS_USB_HID_CONNECT,               /* 2 byte payload */
  EVENT_HS_USB_MSD_CONNECT,               /* No payload */
  EVENT_HS_USB_MSD_DISCONECT,             /* No payload */
  EVENT_HS_USB_STACK_SUSPENDED,           /* 1 byte payload */
  EVENT_HS_USB_STACK_RESUMED,             /* 1 byte payload */
  EVENT_HS_USB_ENTER_HOST_MODE,           /* No payload */
  EVENT_HS_USB_OPERATIONAL_ERROR,         /* 2 byte payload */

  EVENT_DTV_L1_ACQ_DONE,                  /* 5 byte payload */
  EVENT_DTV_L1_SCAN,                      /* 4 byte payload */
  EVENT_DTV_L1_ONLINE,                    /* 1 byte payload */
  EVENT_DTV_L1_SNOOZE,                    /* No payload */
  EVENT_DTV_L1_SLEEP,                     /* No payload */
  EVENT_DTV_L1_HANDOFF,                   /* 5 byte payload */
  EVENT_DTV_L1_SIGNAL_LOST,               /* No payload */

  EVENT_IMS_SIP_REGISTRATION_START,       /* 4 byte payload */
  EVENT_IMS_SIP_REGISTER_END,             /* 4 byte payload */
  EVENT_IMS_SIP_DEREGISTER_START,         /* 4 byte payload */
  EVENT_IMS_SIP_DEREGISTER_END,           /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_START,            /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_RINGING,          /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_ESTABLISHED,      /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_TERMINATED,       /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_CANCEL,           /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_FAILURE,          /* 4 byte payload */
  EVENT_IMS_SIP_RESPONSE_RECV,            /* 4 byte payload */
  EVENT_IMS_SIP_REQUEST_RECV,             /* 4 byte payload */
  EVENT_IMS_SIP_RESPONSE_SEND,            /* 4 byte payload */
  EVENT_IMS_SIP_REQUEST_SEND,             /* 4 byte payload */

  EVENT_WLAN_TKIP_COUNTER_MEAS,           /* 2 byte payload */

  EVENT_GPS_BLANKING_OFF,                 /* No payload */
  EVENT_GPS_BLANKING_ON,                  /* No payload */

  EVENT_MMGSDI_EVENT,                     /* 16 byte payload */

  EVENT_WLAN_CP_SYS_MGR_STATE_TRANS,      /* 3 byte payload */

  EVENT_GPS_OPTIMISTIC_PUNC_START,        /* 4 byte payload */
  EVENT_GPS_OPTIMISTIC_PUNC_END,          /* 4 byte payload */

  EVENT_QVP_SEND_RTP_PACKET,              /* 7 byte payload */
  EVENT_QVP_RECV_RTP_PACKET,              /* 7 byte payload */

  EVENT_HDR_IDLE_REACQ_FAIL_DDARF,        /* 3 byte payload */

  EVENT_BCAST_SEC_STKM_PARSE_STATUS,      /* 2 byte payload */
  EVENT_BCAST_SEC_STKM_RECEIVED,          /* No payload */
  EVENT_BCAST_SEC_SDP_PARSE_STATUS,       /* 2 byte payload */

  EVENT_CGPS_ME_DPO_STATUS,               /* 1 byte payload */
  EVENT_GPS_SV_SEARCH_STATE,              /* 6 byte payload */
  EVENT_GPS_TM_ON_DEMAND_MODE_CHANGE,     /* 3 byte payload */
  EVENT_GPS_TM_ON_DEMAND_BEGIN,           /* 6 byte payload */
  EVENT_GPS_TM_ON_DEMAND_DONE,            /* 1 byte payload */

  EVENT_RMAC_CARRIER_STATE_CHANGED,       /* 6 byte payload */

  EVENT_GPS_SBAS_DEMOD_REPORT,            /* 9 byte payload */
  EVENT_GPS_EXTERN_COARSE_POS_INJ_START,  /* No payload */
  EVENT_GPS_EXTERN_COARSE_POS_INJ_END,    /* 1 byte payload */
  EVENT_GPS_EPH_REREQUEST_TIME,           /* 2 byte payload */

  EVENT_WLAN_QOS_PSTREAM,                 /* 3 byte payload */
  EVENT_WLAN_CP_VCC,                      /* 9 byte payload */

  EVENT_CGPS_DIAG_FIRST_SUCCESSFUL_FIX,   /* No payload */

  EVENT_EUL_RECONFIG_OR_ASU_OR_TTI_RECFG, /* 12 byte payload */

  EVENT_DS707_PKT_LN_UPDATE,              /* 3 byte payload */
  EVENT_DS707_PKT_IDM_CHANGE,             /* 2 byte payload */

  EVENT_RLP_QN_ADD,                       /* 3 byte payload */
  EVENT_RLP_QN_DROP,                      /* 3 byte payload */
  EVENT_RLP_MULTILINK_NAK,                /* 9 byte payload */
  EVENT_RLP_REV_LINK_NAK,                 /* 9 byte payload */
  EVENT_GSTK_EVENT,                       /* 16 byte payload */

  EVENT_GAN_REGISTRATION_REQUEST,         /* 1 byte payload */
  EVENT_GAN_REGISTER_ACCEPT,              /* 1 byte payload */
  EVENT_CALL_RINGING_ALERT,               /* 1 byte payload */
  EVENT_GAN_PAGING_RECEIVED,              /* 1 byte payload */
  EVENT_GAN_CALL_DISCONNECT,              /* 1 byte payload */
  EVENT_GAN_CALL_RELEASE_COMPLETE,        /* 1 byte payload */
  EVENT_GAN_HANDIN_COMMAND,               /* 1 byte payload */
  EVENT_GAN_HANDIN_COMPLETE,              /* 1 byte payload */
  EVENT_GAN_HANDOUT_COMMAND,              /* 1 byte payload */
  EVENT_GAN_HANDOUT_COMPLETE,             /* 1 byte payload */
  EVENT_GAN_SMS_START,                    /* 1 byte payload */
  EVENT_GAN_SMS_ACK,                      /* 1 byte payload */
  EVENT_GAN_QDJ_ENQUEUE,                  /* No payload */
  EVENT_GAN_QDJ_DEQUEUE,                  /* No payload */
  EVENT_GAN_ACTIVATE_DATA_CHANNEL,        /* 1 byte payload */
  EVENT_GAN_DATA_CHANNEL_CONNECTED,       /* 1 byte payload */
  EVENT_GAN_RLP_SUSPEND,                  /* 1 byte payload */
  EVENT_GAN_RLP_RESUME,                   /* 1 byte payload */
  EVENT_GAN_WAKEUP_REQ,                   /* No payload */
  EVENT_GAN_WAKEUP_CNF,                   /* No payload */
  EVENT_GAN_HIBERNATION_REQ,              /* No payload */
  EVENT_GAN_HIBERNATION_CNF,              /* No payload */

  EVENT_WCDMA_UL_AMR_RATE,                /* 11 byte payload */
  EVENT_EUL_TTI_RECONFIG,                 /* 1 byte payload */
  EVENT_WCDMA_CONN_REL_CAUSE,             /* 1 byte payload */
  EVENT_WCDMA_CONN_REQ_CAUSE,             /* 1 byte payload */

  EVENT_LTE_TIMING_ADVANCE,               /* 3 byte payload */
  EVENT_LTE_UL_OUT_OF_SYNC,               /* No payload */
  EVENT_LTE_SPS_DEACTIVATED,              /* 1 byte payload */
  EVENT_LTE_RACH_ACCESS_START,            /* 2 byte payload */
  EVENT_LTE_RACH_RAID_MATCH,              /* 1 byte payload */
  EVENT_LTE_RACH_ACCESS_RESULT,           /* 1 byte payload */

  EVENT_DTV_L1_POWERUP,                   /* 2 byte payload */
  EVENT_DTV_L1_POWERDOWN,                 /* 2 byte payload */
  EVENT_DTV_L1_SOFT_RESET,                /* 2 byte payload */
  EVENT_DTV_L1_STATE_CHANGE,              /* 8 byte payload */
  EVENT_DTV_L1_ACQ_TUNE_STATUS,           /* 9 byte payload */
  EVENT_DTV_L1_ACQ_DONE_STATUS,           /* 9 byte payload */
  EVENT_DTV_L1_ACQ_FAIL,                  /* 3 byte payload */
  EVENT_DTV_L1_TRAFFIC_STARTED,           /* 2 byte payload */
  EVENT_DTV_L1_BAD_FRAME_RECEIVED,        /* 6 byte payload */
  EVENT_DTV_L1_TMCC_FAILURE,              /* 4 byte payload */
  EVENT_DTV_L1_RECOVERY_STATUS,           /* 5 byte payload */
  EVENT_DTV_L1_INTERRUPT_LOG_RECEIVED,    /* 2 byte payload */
  EVENT_DTV_L1_L3_API_COMMAND,            /* 4 byte payload */
  EVENT_DTV_L1_MODEM_FAILURE,             /* 6 byte payload */

  EVENT_GSM_CALL_DROP,                    /* 2 byte payload */
  EVENT_GSM_ACCESS_FAILURE,               /* 3 byte payload */

  EVENT_DTV_ISDB_ACTIVATE,                /* 9 byte payload */
  EVENT_DTV_ISDB_DEACTIVATE,              /* 9 byte payload */
  EVENT_DTV_ISDB_TUNE,                    /* 13 byte payload */
  EVENT_DTV_ISDB_UNTUNE,                  /* 9 byte payload */
  EVENT_DTV_ISDB_SELECT_SERVICE,          /* 11 byte payload */
  EVENT_DTV_ISDB_SERVICE_AVAILABLE,       /* 6 byte payload */
  EVENT_DTV_ISDB_TRAFFIC_LOST,            /* 4 byte payload */
  EVENT_DTV_ISDB_TABLE_UPDATE,            /* 7 byte payload */
  EVENT_DTV_ISDB_TRACKS_SELECTED,         /* 13 byte payload */
  EVENT_DTV_ISDB_PES_BUFFER_OVERFLOW,     /* 5 byte payload */
  EVENT_DTV_ISDB_PES_BUFFER_UNDERFLOW,    /* 5 byte payload */
  EVENT_DTV_ISDB_ACQUIRE_DATA_COMPONENT,  /* 10 byte payload */
  EVENT_DTV_ISDB_STOP_COMPONENT_ACQUISITION, /* 10 byte payload */
  EVENT_DTV_ISDB_DII_CHANGED,             /* 5 byte payload */
  EVENT_DTV_ISDB_DATA_EVENT_MESSAGE,      /* 5 byte payload */
  EVENT_DTV_ISDB_MODULE_CONSTRUCTION,     /* 13 byte payload */
  EVENT_DTV_ISDB_PARSING_ERROR,           /* 13 byte payload */

  EVENT_HDR_SLP_SLPQH_TIMER_STARTED,      /* No payload */
  EVENT_HDR_SLP_SLPQH_TIMER_STOPPED,      /* 1 byte payload */
  EVENT_HDR_SLP_SLPQH_NUM_PENDING_MSGS,   /* 1 byte payload */
  EVENT_HDR_OVHD_FIND_CACHED_MSG,         /* 1 byte payload */

  EVENT_WCDMA_RRC_TIMER_EXPIRED,          /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIMER_USED,            /* 4 byte payload */
  EVENT_WCDMA_UOOS_TIMER_START,           /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIMER_STOP,            /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIME_REMAINING,        /* 6 byte payload */
  EVENT_WCDMA_RRCCSP_SCAN_START,          /* 1 byte payload */
  EVENT_WCDMA_ACQUISITON_SUCCESS,         /* 5 byte payload */
  EVENT_WCDMA_CELL_SELECTION_FAIL,        /* 1 byte payload */
  EVENT_WCDMA_BPLMN_START,                /* 1 byte payload */
  EVENT_WCDMA_BPLMN_END,                  /* 1 byte payload */
  EVENT_WCDMA_BPLMN_SCAN_START,	          /* 1 byte payload */
  EVENT_WCDMA_BPLMN_SCAN_END,             /* 1 byte payload */

  EVENT_MSG_HIGH,                         /* 32 byte payload */
  EVENT_MSG_MED,                          /* 32 byte payload */
  EVENT_MSG_LOW,                          /* 32 byte payload */
  EVENT_MSG_ERROR,                        /* 32 byte payload */
  EVENT_MSG_FATAL,                        /* 32 byte payload */

  EVENT_GAN_START_TU3910,                 /* No payload */
  EVENT_GAN_STOP_TU3910,                  /* No payload */
  EVENT_GAN_EXPIRY_TU3910,                /* No payload */
  EVENT_GAN_START_TU3920,                 /* No payload */
  EVENT_GAN_STOP_TU3920,                  /* No payload */
  EVENT_GAN_EXPIRY_TU3920,                /* No payload */
  EVENT_GAN_START_TU3906,                 /* No payload */
  EVENT_GAN_STOP_TU3906,                  /* No payload */
  EVENT_GAN_EXPIRY_TU3906,                /* No payload */
  EVENT_GAN_URR_REGISTER_UPDATE,          /* 1 byte payload */

  EVENT_IPSEC_IKE_SA_INIT_SENT,            /* 8 bytes payload */
  EVENT_IPSEC_IKE_SA_INIT_RECV,            /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_ESTABLISHED,          /* 16 bytes payload */
  EVENT_IPSEC_IKE_AUTH_SENT,               /* 16 bytes payload */
  EVENT_IPSEC_IKE_AUTH_RECV,               /* 16 bytes payload */
  EVENT_IPSEC_IKE_EAP_START,               /* No payload */
  EVENT_IPSEC_IKE_EAP_FINISH,              /* 1 byte payload */
  EVENT_IPSEC_CHILD_SA_ESTABLISHED,        /* 8 bytes payload */
  EVENT_IPSEC_IKE_INFO_MSG_SENT,           /* 16 bytes payload */
  EVENT_IPSEC_IKE_INFO_MSG_RECV,           /* 16 bytes payload */
  EVENT_IPSEC_CREATE_CHILD_SA_SENT,        /* 16 bytes payload */
  EVENT_IPSEC_CREATE_CHILD_SA_RECV,        /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_DELETE_START,         /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_DELETE_DONE,          /* 16 bytes payload */
  EVENT_IPSEC_CHILD_SA_DELETE_START,       /* 8 bytes payload */
  EVENT_IPSEC_CHILD_SA_DELETE_DONE,        /* 8 bytes payload */
  EVENT_IPSEC_IKE_SA_REKEY_START,          /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_REKEY_DONE,           /* 16 bytes payload */
  EVENT_IPSEC_CHILD_SA_REKEY_START,        /* 8 bytes payload */
  EVENT_IPSEC_CHILD_SA_REKEY_DONE,         /* 8 bytes payload */
  EVENT_IPSEC_IKE_MESG_RETRANSMIT,         /* 4 bytes payload */
  EVENT_IPSEC_IKE_NAT_DETECTED,            /* No payload */
  EVENT_IPSEC_IKE_NAT_KEEPALIVE_SENT,      /* 16 bytes payload */
  EVENT_IPSEC_IKE_DPD_SENT,                /* 16 bytes payload */
  EVENT_IPSEC_IKE_ERR_NOTIFY_SENT,         /* 4 bytes payload */
  EVENT_IPSEC_IKE_ERR_NOTIFY_RECV,         /* 4 bytes payload */

  EVENT_GAN_ROVEIN_CNF,                    /* No payload */
  EVENT_GAN_ROVEOUT_CNF,                   /* No payload */
  EVENT_GAN_RRC_ROVEIN_CNF,                /* No payload */
  EVENT_GAN_RRC_ROVEIN_REJ,                /* 1 byte payload */
  EVENT_GAN_RRC_ROVEOUT_CNF,               /* No payload */
  EVENT_GAN_RRC_ROVEOUT_REJ,               /* 1 byte payload */

  EVENT_GPSXTRA_T_SESS_BEGIN,              /* 1 byte payload */
  EVENT_GPSXTRA_T_SESS_DATA,               /* 8 byte payload */
  EVENT_GPSXTRA_T_SESS_DONE,               /* 1 byte payload */
  EVENT_GPSXTRA_T_SESS_END,                /* 4 byte payload */

  EVENT_DS_GO_NULL_TIMER,                  /* 5 byte payload */

  EVENT_LTE_RRC_TIMER_STATUS,              /* 6 byte payload */
  EVENT_LTE_RRC_STATE_CHANGE,              /* 1 byte payload */
  EVENT_LTE_RRC_OUT_OF_SERVICE,            /* 2 byte payload */
  EVENT_LTE_RRC_RADIO_LINK_FAILURE,        /* 2 byte payload */
  EVENT_LTE_RRC_DL_MSG,                    /* 2 byte payload */
  EVENT_LTE_RRC_UL_MSG,                    /* 2 byte payload */
  EVENT_LTE_RRC_NEW_CELL_IND,              /* 5 byte payload */
  EVENT_LTE_RRC_CELL_RESEL_FAILURE,        /* 5 byte payload */
  EVENT_LTE_RRC_HO_FAILURE,                /* 5 byte payload */
  EVENT_LTE_RRC_PAGING_DRX_CYCLE,          /* 2 byte payload */
  EVENT_LTE_RRC_IRAT_HO_FROM_EUTRAN,       /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_HO_FROM_EUTRAN_FAILURE, /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN,            /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_FAILURE, /* 1 byte payload */
  EVENT_LTE_RRC_SIB_READ_FAILURE,          /* 6 byte payload */

  EVENT_GAN_ROVEIN_REQ,                    /* No payload */
  EVENT_GAN_ROVEOUT_REQ,                   /* No payload */

  EVENT_MBP_RF_ANALOG_JD_MODE_CHANGE,      /* 7 byte payload */
  EVENT_MBP_RF_ANALOG_JD_INT,              /* 4 byte payload */

  EVENT_CGPS_QWIP_SYSD_TRANSITION,         /* 4 byte payload */

  EVENT_HPLMN_TIMER_EXPIRED,               /* No Payload */

  EVENT_GSDI_GET_FEATURE_INDICATOR_DATA,   /* 6 byte payload */

  EVENT_LTE_CM_INCOMING_MSG,				/* 1 byte payload */
  EVENT_LTE_CM_OUTGOING_MSG,				/* 1 byte payload */
  EVENT_LTE_EMM_INCOMING_MSG,				/* 1 byte payload */
  EVENT_LTE_EMM_OUTGOING_MSG,				/* 1 byte payload */
  EVENT_LTE_EMM_TIMER_START,				/* 1 byte payload */
  EVENT_LTE_EMM_TIMER_EXPIRY,				/* 1 byte payload */

  EVENT_LTE_REG_INCOMING_MSG,				/* 1 byte payload */
  EVENT_LTE_REG_OUTGOING_MSG,				/* 1 byte payload */
  EVENT_LTE_ESM_INCOMING_MSG,				/* 1 byte payload */
  EVENT_LTE_ESM_OUTGOING_MSG,				/* 1 byte payload */
  EVENT_LTE_ESM_TIMER_START,				/* 1 byte payload */
  EVENT_LTE_ESM_TIMER_EXPIRY,				/* 1 byte payload */

  EVENT_SNS_CONTEXT_OPEN,					/* 4 byte payload */
  EVENT_SNS_CONTEXT_CLOSE,					/* 4 byte payload */
  EVENT_SNS_COND_SET,						/* 8 byte payload */
  EVENT_SNS_COND_CANCEL,					/* 8 byte payload */
  EVENT_SNS_COND_MET,						/* 8 byte payload */
  EVENT_SNS_DATA_START,						/* 12 byte payload */
  EVENT_SNS_DATA_STOP,						/* 8 byte payload */

  EVENT_WCDMA_RLC_CONFIG,					/* 4 byte payload */

  EVENT_HSPA_PLUS_CFG,						/* 6 byte payload */

  EVENT_SNS_DRIVER_STATE_CHANGE,				/* 9 byte payload */

  EVENT_WCDMA_TIMER_DISCARD_EXPIRY,				/* 3 byte payload */

  EVENT_NAS_CB_PAGE_RECEIVED,					/* 5 byte payload */

  EVENT_WCDMA_RLC_RESET,						/* 1 byte payload */

  EVENT_HDR_MRLP_EHRPD_PERSONALITY_IS_ACTIVE, /* 1 byte payload */

  EVENT_WLAN_SECURITY,                     /* 13 byte payload */
  EVENT_WLAN_STATUS,                       /* 15 byte payload */
  EVENT_WLAN_HANDOFF,                      /* 15 byte payload */
  EVENT_WLAN_VCC,                          /* 8 byte payload */
  EVENT_WLAN_QOS,                          /* 2 byte payload */
  EVENT_WLAN_PE,                           /* 16 byte payload */
  EVENT_WLAN_ADD_BLOCK_ACK_SUCCESS,        /* 11 byte payload */
  EVENT_WLAN_ADD_BLOCK_ACK_FAILED,         /* 9 byte payload */
  EVENT_WLAN_DELETE_BLOCK_ACK_SUCCESS,     /* 8 byte payload */
  EVENT_WLAN_DELETE_BLOCK_ACK_FAILED,      /* 8 byte payload */
  EVENT_WLAN_BSS_PROTECTION,               /* 2 byte payload */
  EVENT_WLAN_BRINGUP_STATUS,               /* 12 byte payload */
  EVENT_WLAN_POWERSAVE_GENERIC,            /* 16 byte payload */
  EVENT_WLAN_POWERSAVE_WOW,                /* 11 byte payload */
  EVENT_WLAN_WCM,                          /* 17 byte payload */
  EVENT_WLAN_WPS_SCAN_START,               /* 16 byte payload */
  EVENT_WLAN_WPS_SCAN_COMPLETE,            /* 2 byte payload */
  EVENT_WLAN_WPS_CONNECT_REQUEST,          /* 9 byte payload */
  EVENT_WLAN_WPS_CONNECT_RESPONSE,         /* 6 byte payload */
  EVENT_WLAN_WPS_PBC_SESSION_OVERLAP,      /* 16 byte payload */
  EVENT_WLAN_WPS_PBC_WALK_TIMER_START,     /* No payload */
  EVENT_WLAN_WPS_PBC_WALK_TIMER_STOP,      /* No payload */
  EVENT_WLAN_WPS_PBC_AP_DETECTED,          /* 14 byte payload */
  EVENT_WLAN_WPS_REGISTRATION_START,       /* 1 byte payload */
  EVENT_WLAN_WPS_WSC_MESSAGE,              /* 1 byte payload */
  EVENT_WLAN_WPS_DISCOVERY,                /* 7 byte payload */
  EVENT_WLAN_WPS_REGISTRATION_COMPLETE,    /* 1 byte payload */
  EVENT_WLAN_WPS_DISCONNECT,               /* No payload */
  EVENT_WLAN_BTC,                          /* 15 byte payload */

  EVENT_IPV6_SM_EVENT,                     /* 4 byte payload */
  EVENT_IPV6_SM_TRANSITION,                /* 5 byte payload */
  EVENT_IPV6_PREFIX_UPDATE,                /* 13 byte payload */

  EVENT_LTE_ML1_STATE_CHANGE,              /* 2 byte payload */

  EVENT_AUTH_PROTO,                        /* 2 byte payload */
  EVENT_VSNCP,                             /* 2 byte payload */
  EVENT_IID,                               /* 2 byte payload */

  EVENT_IMS_VIDEOSHARE_REGISTRATION_SUCCESS,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INVITE_SENT,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_INVITE,        /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ACCEPT_REJECT_INVITE,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ACCEPTING_SESSION,      /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_SESSION_ESTABLISHED,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_END_SESSION,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_PREVIEW_VIDEO_FRAME,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_DECODED_VIDEO_FRAME,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_RECEIVING,              /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_START_APPLICATION,      /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_END_APPLICATION,        /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_CAPABILITY_SUCCESS,     /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_MEDIA_RECORDING_RESOURCE_ACQUIRED,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_MEDIA_RECORDING_RESOURCE_RELEASED,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_SENDING,                             /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_OPTION_RECEIVED,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_OPTION_RESPONDED,           /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_CALL_FAILED,                     /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_REGISTRATION_FAILED,             /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_RECORDER_ERROR,                  /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_PLAYER_ERROR,                    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_MEDIA_SESSION_FAILURE,           /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_CAPABILITY_FAILURE,              /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_MEDIA_RECORDING_FAILED,          /* 4 byte payload */

  EVENT_WLAN_PE_FRAME,                     /* 16 byte payload */

  EVENT_SNS_VCPS_HEADING_COMPUTED,         /* 12 byte payload */
  EVENT_SNS_VCPS_TRACKED_CAL_SET_SAVED,    /* 22 byte payload */

  EVENT_GNSS_PRESC_DWELL_COMPLETE,         /* 2 byte payload */

  EVENT_LTE_MAC_RESET,                     /* 1 byte payload */
  EVENT_LTE_BSR_SR_REQUEST,                /* 1 byte payload */
  EVENT_LTE_MAC_TIMER,                     /* 2 byte payload */

  EVENT_CM_DS_OPERATIONAL_MODE,            /* 2 byte payload */
  EVENT_CM_DS_MODE_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_GW_ACQ_ORDER_PREF,           /* 5 byte payload */
  EVENT_CM_DS_SRV_DOMAIN_PREF,             /* 5 byte payload */
  EVENT_CM_DS_BAND_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_ROAM_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_HYBRID_PREF,                 /* 5 byte payload */
  EVENT_CM_DS_NETWORK_SEL_MODE_PREF,       /* 5 byte payload */
  EVENT_CM_DS_CALL_EVENT_ORIG,             /* 4 byte payload */
  EVENT_CM_DS_CALL_EVENT_CONNECT,          /* 4 byte payload */
  EVENT_CM_DS_CALL_EVENT_END,              /* 3 byte payload */
  EVENT_CM_DS_ENTER_EMERGENCY_CB,          /* 1 byte payload */
  EVENT_CM_DS_EXIT_EMERGENCY_CB,           /* 1 byte payload */
  EVENT_CM_DS_CALL_STATE,                  /* 2 byte payload */
  EVENT_CM_DS_DS_INTERRAT_STATE,           /* 3 byte payload */
  EVENT_CM_DS_CELL_SRV_IND,                /* 6 byte payload */
  EVENT_CM_DS_COUNTRY_SELECTED,            /* 3 byte payload */
  EVENT_CM_DS_DATA_AVAILABLE,              /* 2 byte payload */
  EVENT_CM_DS_SELECT_COUNTRY,              /* 8 byte payload */
  EVENT_CM_DS_CALL_EVENT_ORIG_THR,         /* 4 byte payload */
  EVENT_CM_DS_PLMN_FOUND,                  /* 13 byte payload */
  EVENT_CM_DS_SERVICE_CONFIRMED,           /* 13 byte payload */
  EVENT_CM_DS_GET_PASSWORD_IND,            /* 3 byte payload */
  EVENT_CM_DS_PASSWORD_AUTHENTICATION_STATUS, /* 3 byte payload */
  EVENT_CM_DS_USS_RESPONSE_NOTIFY_IND,     /* 4 byte payload */
  EVENT_CM_DS_LCS_MOLR_CONF,               /* 2 byte payload */

  EVENT_DS_NAS_MESSAGE_SENT,               /* 5 byte payload */
  EVENT_DS_NAS_MESSAGE_RECEIVED,           /* 5 byte payload */
  EVENT_DS_MM_STATE,                       /* 3 byte payload */
  EVENT_DS_GMM_STATE,                      /* 3 byte payload */
  EVENT_DS_PLMN_INFORMATION,               /* 10 byte payload */

  EVENT_DIAG_STRESS_TEST_COMPLETED,        /* 4 byte payload */

  EVENT_GNSS_CC_STATUS,                    /* 2 byte payload */

  EVENT_SNS_USER_STATE_CHANGE,             /* 6 byte payload */

  EVENT_DS_HPLMN_TIMER_EXPIRED,            /* 1 byte payload */
  EVENT_DS_RAT_CHANGE,                     /* 2 byte payload */

  EVENT_DTV_CMMB_API_CALL_ACTIVATE,               /*9 byte payload*/ /*ID=1757*/
  EVENT_DTV_CMMB_API_CALL_DEACTIVATE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_TUNE,                     /*13 byte payload*/
  EVENT_DTV_CMMB_API_CALL_SELECT_SERVICE,     /*11 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DESELECT_SERVICE,  /*11 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SIGNAL_PARAMETERS,      /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_NIT,                /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_CMCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SMCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_CSCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SSCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_EADT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REQUEST_CA_CARD_NUMBER,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REQUEST_CAS_ID,                 /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REGISTER_FOR_CONTROL_NOTIFICATIONS,         /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DEREGISTER_FROM_CONTROL_NOTIFICATIONS,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ACTIVATE,               /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEACTIVATE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_TUNE,      /*13 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_SELECT_SERVICE,     /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DESELECT_SERVICE,  /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_TABLE_UPDATE,        /*6 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_SIGNAL_PARAMETERS,/*14 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_AUTHORIZATION_FAILURE,                 /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_REGISTER_FOR_CONTROL_NOTIFICATIONS_COMPLETE,      /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEREGISTER_FROM_CONTROL_NOTIFICATIONS_COMPLETE, /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_CA_CARD_NUMBER,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_CAS_ID,                 /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_EMERGENCY_BROADCASTING_TRIGGER, /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_EMERGENCY_BROADCASTING_MESSAGE,/*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REGISTER_FOR_ESG_NOTIFICATIONS,                /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DEREGISTER_FROM_ESG_NOTIFICATIONS,          /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_BASIC_DESCRIPTION_INFORMATION,          /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_SET_OUTPUT_PATH,/*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_DATA_INFORMATION,                  /*6 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_DATA_INFORMATION_DOWNLOAD_COMPLETE,         /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_PROGRAM_INDICATION_INFORMATION,                   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_REGISTER_FOR_ESG_NOTIFICATIONS_COMPLETE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEREGISTER_FROM_ESG_NOTIFICATIONS_COMPLETE,     /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_INITIALIZED,                 /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_EMM_RECEIVED_AND_PROCESSED,    /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_ECM_RECEIVED_AND_PROCESSED,    /*11 byte payload*/ /*ID=1798*/

  EVENT_ECALL_START,                                      /*3 byte payload*/
  EVENT_ECALL_STOP,                                       /*1 byte payload*/
  EVENT_ECALL_SESSION_START,                        /*1 byte payload*/
  EVENT_ECALL_SESSION_FAILURE,                      /*1 byte payload*/
  EVENT_ECALL_SESSION_COMPLETE,                   /*3 byte payload*/
  EVENT_ECALL_SESSION_RESET,                        /*1 byte payload*/
  EVENT_ECALL_PSAP_MSD_DECODE_SUCCESS,     /*2 byte payload*/
  EVENT_ECALL_PSAP_LOST_SYNC,                     /*1 byte payload*/ /*ID = 1806*/

  EVENT_LTE_RRC_IRAT_REDIR_FROM_EUTRAN_START, /*1 byte payload */
  EVENT_LTE_RRC_IRAT_REDIR_FROM_EUTRAN_END,    /* 2 byte payload */ /*ID = 1808*/

  EVENT_GPRS_DS_CELL_CHANGE_ORDER,            /* 2 byte payload */ /*1809*/
  EVENT_GSM_DS_CELL_SELECTION_END,            /* 2 byte payload */ /*1810*/
  EVENT_GSM_DS_L1_STATE,                      /* 2 byte payload */ /*1811*/
  EVENT_GSM_DS_PLMN_LIST_START,               /* 2 byte payload */ /*1812*/
  EVENT_GSM_DS_PLMN_LIST_END,                 /* 1 byte payload */ /*1813*/
  EVENT_GSM_DS_POWER_SCAN_STATUS,             /* 2 byte payload */ /*1814*/
  EVENT_GSM_DS_RESELECT_START,                /* 2 byte payload */ /*1815*/
  EVENT_GSM_DS_RR_IN_SERVICE,                 /* 1 byte payload */ /*1816*/
  EVENT_GSM_DS_RR_OUT_OF_SERVICE,             /* 1 byte payload */ /*1817*/
  EVENT_GSM_DS_TIMER_EXPIRED,                 /* 3 byte payload */ /*1818*/
  EVENT_GSM_DS_TO_WCDMA_RESELECT_END,  /* 6 byte payload */ /*1819*/

  EVENT_CM_DS_SYSTEM_MODE,                  /*2 byte payload*/ /*1820*/
  EVENT_SD_DS_EVENT_ACTION = 0x71d,      /*9 byte payload*/ /*1821*/
  EVENT_SMGMM_DS_REQUEST_SENT = 0x71e,   /*3 byte payload*/ /*1822*/
  EVENT_IFACE = 0x71F,                  /*4 byte payload*/ /*1823*/
  EVENTS_DS_GSM_L1_ALIGN_VFR = 0x720,
  EVENTS_DS_GSM_L1_STATE = 0x721,
  EVENTS_DS_GSM_RATSCCH_IN_DTX = 0x722,
  EVENTS_DS_GSM_FACCH_IN_DTX = 0x723,
  EVENTS_DS_GSM_FACCH_AND_RATSCCH_COLLISION = 0x724,
  EVENTS_DS_GSM_FACCH_AND_SID_UPDATE_COLLISION = 0x725,
  EVENTS_DS_GSM_RATSCCH_AND_SID_UPDATE_COLLISION = 0x726,
  EVENTS_DS_GSM_AMR_STATE_CHANGE = 0x727,
  EVENTS_DS_GSM_RATSCCH_CMI_PHASE_CHANGE = 0x728,
  EVENTS_DS_GSM_RATSCCH_REQ_ACT_TIMER_EXPIRY = 0x729,
  EVENTS_DS_GSM_RATSCCH_ACK_ACT_TIMER_EXPIRY = 0x72a,
  EVENTS_DS_GSM_AMR_RATSCCH_REQ = 0x72b,
  EVENTS_DS_GSM_AMR_RATSCCH_RSP = 0x72c,
  EVENTS_DS_GSM_AMR_CMC_TURNAROUND_TIME = 0x72d,
  EVENTS_DS_GPRS_SMGMM_MSG_RECEIVED = 0x72e,
  EVENTS_DS_GPRS_SMGMM_MSG_SENT = 0x72f,
  EVENTS_DS_GPRS_LLC_READY_TIMER_START = 0x730,
  EVENTS_DS_GPRS_LLC_READY_TIMER_END = 0x731,
  EVENTS_DS_PACKET_TIMESLOT_RECONFIGURE = 0x732,
  EVENTS_DS_GPRS_MAC_MSG_RECEIVED = 0x733,
  EVENTS_DS_GPRS_MAC_MSG_SENT = 0x734,
  EVENTS_DS_GPRS_MAC_CAMPED_ON_CELL = 0x735,
  EVENTS_DS_GPRS_CELL_CHANGE_FAILURE = 0x736,
  EVENTS_DS_GPRS_PACKET_CHANNEL_REQUEST = 0x737,
  EVENTS_DS_GPRS_PACKET_UPLINK_ASSIGNMENT = 0x738,
  EVENTS_DS_GPRS_PACKET_DOWNLINK_ASSIGNMENT = 0x739,
  EVENTS_DS_GPRS_TBF_RELEASE = 0x73a,
  EVENTS_DS_GPRS_TIMER_EXPIRY = 0x73b,
  EVENTS_DS_GPRS_PACKET_RESOURCE_REQUEST = 0x73c,
  EVENTS_DS_RANDOM_ACCESS_REQUEST = 0x73d,
  EVENTS_DS_GSM_HANDOVER_START = 0x73e,
  EVENTS_DS_GSM_HANDOVER_END = 0x73f,
  EVENTS_DS_GSM_RESELECT_START = 0x740,
  EVENTS_DS_GSM_RESELECT_END = 0x741,
  EVENTS_DS_GSM_TO_WCDMA_RESELECT_END = 0x742,
  EVENTS_DS_GSM_MESSAGE_RECEIVED = 0x743,
  EVENTS_DS_GSM_RR_IN_SERVICE = 0x744,
  EVENTS_DS_GSM_RR_OUT_OF_SERVICE = 0x745,
  EVENTS_DS_GSM_PAGE_RECEIVED = 0x746,
  EVENTS_DS_GSM_CAMP_ATTEMPT_START = 0x747,
  EVENTS_DS_GSM_CAMP_ATTEMPT_END = 0x748,
  EVENTS_DS_GSM_CALL_DROP = 0x749,
  EVENTS_DS_GSM_ACCESS_FAILURE = 0x74a,
  EVENTS_DS_GSM_CELL_SELECTION_START = 0x74b,
  EVENTS_DS_GSM_CELL_SELECTION_END = 0x74c,
  EVENTS_DS_GSM_POWER_SCAN_STATUS = 0x74d,
  EVENTS_DS_GSM_PLMN_LIST_START = 0x74e,
  EVENTS_DS_GSM_PLMN_LIST_END = 0x74f,
  EVENTS_DS_GSM_AMR_MULTIRATE_IE = 0x750,
  EVENTS_DS_GPRS_LINK_FAILURE = 0x751,
  EVENTS_DS_GPRS_PAGE_RECEIVED = 0x752,
  EVENTS_DS_GPRS_SURROUND_SEARCH_START = 0x753,
  EVENTS_DS_GPRS_SURROUND_SEARCH_END = 0x754,
  EVENTS_DS_GPRS_EARLY_CAMPING = 0x755,
  EVENTS_DS_GSM_LINK_FAILURE = 0x756,

  EVENT_MTP_FILE_DELETED = 0x757,
  EVENT_MTP_PLAYLIST_REMOVED_OBJECT = 0x758,
  EVENT_MTP_SYNC_STARTED = 0x759,
  EVENT_MTP_SYNC_FINISHED = 0x75a,
  EVENT_MTP_SAVE_ALBUMART_STARTED = 0x75b,
  EVENT_MTP_SAVE_ALBUMART_FINISHED = 0x75c,
  EVENT_MTP_FORMAT_STORE_STARTED = 0x75d,
  EVENT_MTP_FORMAT_STORE_DONE = 0x75e,
  EVENT_MTP_FORMAT_STORE_ERROR = 0x75f,
  EVENT_LTE_RRC_SECURITY_CONFIG = 0x760,
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_START = 0x761,
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_END = 0x762,
  EVENT_SNS_REST_DETECT_ACCEL_ACTIVE_TS = 0x763,
  EVENT_SNS_REST_DETECT_ACCEL_STOP_TS = 0x764,
  EVENT_CPC_CONFIG_ACTION = 0x765,
  EVENT_FDPCH_CONFIG_ACTION = 0x766,
  EVENT_SNS_DRV_MOTION_DETECT_SIG = 0x767,
  EVENT_SNS_DRV_OPMODE_CHANGE = 0x768,
  EVENT_PM_BATT_CONNECT = 0x769,
  EVENT_PM_BATT_DISCONNECT = 0x76a,
  EVENT_PM_BATT_TEMP_OUT_OF_RANGE = 0x76b,
  EVENT_PM_CHARGE_STATE_CHANGED = 0x76c,
  EVENT_PM_CHARGE_DONE = 0x76d,
  EVENT_PM_CHARGE_FAIL = 0x76e,
  EVENT_PM_VBAT_DET = 0x76f,
  EVENT_PM_AUTO_CHARGING_EOC_ITERM_SW_TIMER = 0x770,
  EVENT_PM_AUTO_CHARGING_EOC_DETECTION_SW_TIMER = 0x771,
  EVENT_PM_AUTO_CHARGING_MAX_CHARGE_SW_TIMER = 0x772,
  EVENT_PM_AUTO_CHARGING_RESUME_CHARGE_DETECTION_TIMER = 0x773,
  EVENT_PM_OVP_TRIGGERED = 0x774,
  EVENT_PM_GEN_VERIFY_REGISTER_DEFAULTS = 0x775,
  EVENT_1X_FPC_MODE = 0x776,
  EVENT_1X_FPC_RATCHET = 0x777,
  EVENT_1X_FSCH_ASSIGN = 0x778,
  EVENT_1X_RSCH_ASSIGN = 0x779,
  EVENT_1X_RSCH_REQUEST = 0x77a,
  EVENT_1X_ADV_FL_SB_STATUS = 0x77b,
  EVENT_1X_ADV_RL_SB_STATUS = 0x77c,
  EVENT_1X_ADV_N2M_CHANGE = 0x77d,
  EVENT_PM_COMMON_TEST = 0x77e,
  EVENT_PM_COMMON_TEST_PAYLOAD = 0x77f,
  EVENT_CGPS_PREEMPTION_FLAG = 0x780,
  EVENT_CM_LTE_BAND_PREF  = 0x781,
  EVENT_PS_ARBITRATION = 0x782,
  EVENT_SD_AVOIDANCE_LOG = 0x783,
  EVENT_HSPAPLUS_DC = 0x784,
  EVENT_HS_SCCH_ORDER = 0x785,
  EVENT_DSM_POOL_LEVEL = 0x786,
  EVENT_GPSONEXTRA_DATA_ACK = 0x787,
  EVENT_MCS_MSGR_SEND = 0x788,
  EVENT_MCS_MSGR_RECV = 0x789,
  EVENT_WCDMA_TO_LTE_RESELECTION_START = 0x78A,
  EVENT_WCDMA_TO_LTE_RESELECTION_END = 0x78B,
  EVENT_WCDMA_EOOS_SKIP_FULL_SCAN_TIMER_START = 0x78C,
  EVENT_WCDMA_EOOS_SKIP_FULL_SCAN_TIMER_STOP = 0x78D,
  EVENT_WCDMA_EOOS_SKIP_FULL_SCAN_TIMER_EXPIRED = 0x78E,
  EVENT_LTE_RRC_CELL_BLACKLIST_IND = 0x78F,
  EVENT_LTE_RRC_AS_RESET = 0x790,
  EVENT_WCDMA_OL_MTD_CONFIG_ACTION = 0x791,
  EVENT_LTE_ML1_PHR_REPORT = 0x792,
  EVENT_SMS_COMMAND_PROCESSED = 0x793,
  EVENT_SMS_MT_NOTIFY = 0x794,
  EVENT_GPSXTRA_T_CONN_BEGIN = 0x795,
  EVENT_GPSXTRA_T_CONN_CONNECT = 0x796,
  EVENT_GPSXTRA_T_CONN_DISCONNET = 0x797,
  EVENT_GPSXTRA_T_CONN_DONE = 0x798,
  EVENT_GPSXTRA_T_CONN_FAILURE = 0x799,
  EVENT_GNSS_TLE_SRV_CHANGE_C = 0x79A,
  EVENT_GNSS_TLE_CELL_CHANGE_C = 0x79B,
  EVENT_GNSS_TLE_POS_UPDATE_C = 0x79C,
  EVENT_GNSS_TLE_TIME_TAG_C  = 0x79D,
  EVENT_GNSS_TLE_DELETE_C = 0x79E,
  EVENT_GNSS_TLE_TIME_UPDATE_C = 0x79F,
  EVENT_GNSS_TLE_POSITION_REPORT = 0x7A0,
  EVENT_WCDMA_TO_LTE_REDIRECTION_START = 0x7A1,
  EVENT_WCDMA_TO_LTE_REDIRECTION_END = 0x7A2,
  EVENT_WCDMA_DED_PRIORITIES_VALIDITY_TIMER_START = 0x7A3,
  EVENT_WCDMA_DED_PRIORITIES_VALIDITY_TIMER_EXPIRED = 0x7A4,
  EVENT_LTE_RRC_IRAT_RTT_FROM_EUTRAN_START = 0x7A5,
  EVENT_LTE_RRC_IRAT_RTT_FROM_EUTRAN_END = 0x7A6,
  EVENT_WCDMA_RRC_DSIM_TUNEAWAY_START = 0x7A7,
  EVENT_WCDMA_RRC_DSIM_TUNEAWAY_STOP = 0x7A8,
  EVENT_SMS_ETWS_PRIM_RECEIVED = 0x7A9,
  EVENT_SMS_CBS_MSG_RECEIVED = 0x7AA,
  EVENT_LTE_ML1_TX_POWER_BACKOFF = 0x7AB,
  EVENT_WCDMA_CELL_EFACH_EPCH_ERACH_SUPPORT = 0x7AC,
  EVENT_GPSXTRA_T_CONN_TYPE = 0x7AD,
  EVENT_LTE_EMM_OTA_INCOMING_MSG = 0x7AE,
  EVENT_LTE_EMM_OTA_OUTGOING_MSG = 0x7AF,
  EVENT_LTE_ESM_OTA_INCOMING_MSG = 0x7B0,
  EVENT_LTE_ESM_OTA_OUTGOING_MSG = 0x7B1,
  EVENT_LTE_RRC_PLMN_SEARCH_START = 0x7B2,
  EVENT_LTE_RRC_PLMN_SEARCH_STOP = 0x7B3,
  EVENT_LTE_RRC_BG_TO_FG_PLMN_SEARCH = 0x7B4,
  EVENT_LTE_RRC_LATE_PLMN_SEARCH_RESPONSE = 0x7B5,
  EVENT_WCDMA_RRC_CRNTI = 0x7B6,
  EVENT_WCDMA_RRC_URNTI = 0x7B7,
  EVENT_DS_EPC_PDN = 0x7B8,
  EVENT_DS_EPC_SRAT_CLEANUP_TIMER = 0x7B9,
  EVENT_DS_EPC_EDCT_TIMER = 0x7BA,
  EVENT_GERAN_GRR_PLMN_LIST_RAT_SEARCH_STARTED = 0x7BB,
  EVENT_GERAN_GRR_PLMN_LIST_RAT_SEARCH_COMPLETED = 0x7BC,
  EVENT_GERAN_GRR_PLMN_LIST_RAT_SEARCH_ABORTED = 0x7BD,
  EVENT_GERAN_GRR_REDIR_STARTED = 0x7BE,
  EVENT_GERAN_GRR_REDIR_COMPLETED = 0x7BF,
  EVENT_GERAN_GRR_REDIR_ABORTED = 0x7C0,
  EVENT_1X_FSCH0_BURST_ASSIGN = 0x7C1,
  EVENT_1X_RSCH0_BURST_ASSIGN = 0x7C2,
  EVENT_SENSOR_STATE = 0x7C3,
  EVENT_SENSOR_TIMELINE = 0x7C4,
  EVENT_GSM_TO_LTE_RESEL_STARTED = 0x7C5,
  EVENT_GSM_TO_LTE_RESEL_ENDED = 0x7C6,
  EVENT_GSM_TO_LTE_REDIR_STARTED = 0x7C7,
  EVENT_GSM_TO_LTE_REDIR_ENDED = 0x7C8,
  EVENT_SD_SS_TIMER_LOG = 0x7C9,
  EVENT_LTE_RRC_STATE_CHANGE_TRIGGER = 0x7CA,
  EVENT_LTE_RRC_RADIO_LINK_FAILURE_STAT = 0x7CB,
  EVENT_TDSCDMA_MAC_UL_AMR_RATE	=	0x7CC,
  EVENT_TDSCDMA_MAC_HS_T1_EXPIRY	=	0x7CD,
  EVENT_TDSCDMA_RLC_MAX_RESET	=	0x7CE,
  EVENT_TDSCDMA_RLC_RESETS	=	0x7CF,
  EVENT_TDSCDMA_RLC_OPEN_CLOSE	=	0x7D0,
  EVENT_TDSCDMA_RLC_MRW	=	0x7D1,
  EVENT_TDSCDMA_RLC_CONFIG	=	0x7D2,
  EVENT_TDSCDMA_RLC_TIMER_DISCARD_EXPIRY	=	0x7D3,
  EVENT_TDSCDMA_RRC_INTER_RAT_HANDOVER_START	=	0x7D4,
  EVENT_TDSCDMA_RRC_INTER_RAT_HANDOVER_END	=	0x7D5,
  EVENT_TDSCDMA_RRC_TDSCDMA_TO_GSM_RESELECTION_START	=	0x7D6,
  EVENT_TDSCDMA_RRC_TDSCDMA_TO_GSM_RESELECTION_END	=	0x7D7,
  EVENT_TDSCDMA_RRC_CELL_SELECTED	=	0x7D8,
  EVENT_TDSCDMA_RRC_PAGE_RECEIVED	=	0x7D9,
  EVENT_TDSCDMA_RRC_RL_FAILURE	=	0x7DA,
  EVENT_TDSCDMA_RRC_STATE	=	0x7DB,
  EVENT_TDSCDMA_RRC_OUT_OF_SERVICE	=	0x7DC,
  EVENT_TDSCDMA_RRC_RB0_SETUP_FAILURE	=	0x7DD,
  EVENT_TDSCDMA_RRC_PHYCHAN_EST_FAILURE	=	0x7DE,
  EVENT_TDSCDMA_RRC_GSM_TO_TDSCDMA_HANDOVER_START	=	0x7DF,
  EVENT_TDSCDMA_RRC_MODE	=	0x7E0,
  EVENT_TDSCDMA_RRC_PHYCHAN_CFG_CHANGED	=	0x7E1,
  EVENT_TDSCDMA_RRC_CONN_REL_CAUSE	=	0x7E2,
  EVENT_TDSCDMA_RRC_CONN_REQ_CAUSE	=	0x7E3,
  EVENT_TDSCDMA_RRC_TIMER_EXPIRED	=	0x7E4,
  EVENT_TDSCDMA_RRC_UOOS_TIMER_USED	=	0x7E5,
  EVENT_TDSCDMA_RRC_UOOS_TIMER_START	=	0x7E6,
  EVENT_TDSCDMA_RRC_UOOS_TIMER_STOP	=	0x7E7,
  EVENT_TDSCDMA_RRC_CSP_SCAN_START	=	0x7E8,
  EVENT_TDSCDMA_RRC_CELL_SELECTION_FAIL	=	0x7E9,
  EVENT_TDSCDMA_RRC_BPLMN_START	=	0x7EA,
  EVENT_TDSCDMA_RRC_BPLMN_END	=	0x7EB,
  EVENT_TDSCDMA_RRC_BPLMN_SCAN_START	=	0x7EC,
  EVENT_TDSCDMA_RRC_BPLMN_SCAN_END	=	0x7ED,
  EVENT_TDSCDMA_RRC_EOOS_SKIP_FULL_SCAN_TIMER_START	=	0x7EE,
  EVENT_TDSCDMA_RRC_EOOS_SKIP_FULL_SCAN_TIMER_STOP	=	0x7EF,
  EVENT_TDSCDMA_RRC_EOOS_SKIP_FULL_SCAN_TIMER_EXPIRED	=	0x7F0,
  EVENT_TDSCDMA_RRC_MESSAGE_SENT	=	0x7F1,
  EVENT_TDSCDMA_RRC_MESSAGE_RECEIVED	=	0x7F2,
  EVENT_LTE_ML1_UL_SPS_REPORT = 0x7F8,
  EVENT_LTE_ML1_DL_SPS_REPORT = 0x7F9,
  EVENT_TDSCDMA_RRC_CSP_STATE	=	0x7FA,
  EVENT_TDSCDMA_RRC_HSDPA_START	=	0x7FB,
  EVENT_TDSCDMA_RRC_HSDPA_STOP	=	0x7FC,
  EVENT_TDSCDMA_RRC_HSUPA_START	=	0x7FD,
  EVENT_TDSCDMA_RRC_HSUPA_STOP	=	0x7FE,
  EVENT_TDSCDMA_RRC_SIB_RCVD	=	0x7FF,
  EVENT_RSVD_START = 0x0800,
  EVENT_RSVD_END   = 0x083F,
  EVENT_TDSCDMA_RRC_LLC_SUBSTATE	=	0x840,
  EVENT_TDSCDMA_RRC_MCM_SUBSTATE	=	0x841,
  EVENT_TDSCDMA_RRC_CCM_SUBSTATE	=	0x842,
  EVENT_TDSCDMA_RRC_CU_SUBSTATE	    =	0x843,
  EVENT_TDSCDMA_RRC_NV_RRC_VERSION	=	0x844,
  EVENT_TDSCDMA_RRC_LLC_TOC_USAGE	=	0x845,
  EVENT_TDSCDMA_RRC_LLC_OC_STATUS	=	0x846,
  EVENT_TDSCDMA_RRC_OOS_TRIGGERED	=	0x847,
  EVENT_WCDMA_RRC_RELEASE_CAUSE     =   0x848,
  EVENT_WCDMA_RRC_REJECT_CAUSE      =   0x849,
  EVENT_DS_MO_SMS_STATUS            =   0x84A,
  EVENT_DS_MO_SMS_RETRY_ATTEMPT     =   0x84B,
  EVENT_DS_MT_SMS_NOTIFY            =   0x84C,
  EVENT_DS_WMS_SEARCH_REQUEST       =   0x84D,
  EVENT_DS_SMS_COMMAND_PROCESSED    =   0x84E,
  EVENT_DS_SMS_MT_NOTIFY            =   0x84F,
  EVENT_DS_SMS_CBS_MSG_RECEIVED     =   0x850,
  EVENT_DS_SMS_ETWS_PRIM_RECEIVED   =   0x851,
  EVENT_TDSCDMA_RRC_ACQUISITION_COMPLETE = 0x852,
  EVENT_TDSCDMA_L1_STATE = 0x853,
  EVENT_TDSCDMA_NEW_SERVING_CELL = 0x854,
  EVENT_TDSCDMA_L1_SUSPEND = 0x855,
  EVENT_TDSCDMA_L1_RESUME = 0x856,
  EVENT_TDSCDMA_L1_STOPPED = 0x857,
  EVENT_TDSCDMA_L1_ACQ_SUBSTATE = 0x858,
  EVENT_TDSCDMA_PHYCHAN_CFG_CHANGED = 0x859,
  EVENT_TDSCDMA_RACH_STATUS = 0x85A,
  EVENT_TDSCDMA_MEASUREMENT_EVENT = 0x85B,
  EVENT_GNSS_FAST_TCAL_END          = 0x85C,
  EVENT_IMS_CODEC_INITIALIZATION                    =   0x85D,
  EVENT_IMS_CODEC_RATE_CHANGE_EVENT                 =   0x85E,
  EVENT_T_HDR_CSNA_DUP_3G1X_SRV_MSG		            =	0x85F,
  EVENT_T_HDR_IDLE_CONNECT_REASON		            =	0x860,
  EVENT_T_HDR_IDLE_CONNECTION_DENIED_REASON		    =	0x861,
  EVENT_T_HDR_IDLE_CONNECTION_SETUP_FAIL_REASON		=	0x862,
  EVENT_T_HDR_OVHD_PRE_REG_IS_ALLOWED		        =	0x863,
  EVENT_T_HDR_SAP_TUNNEL_IS_ENABLED		            =	0x864,
  EVENT_T_HDR_SAP_CONNECTION_IS_OPEN		        =	0x865,
  EVENT_T_HDR_ALMP_LTE_RESELECT		                =	0x866,
  EVENT_T_HDR_OVHD_ORNL_IS_UPDATED		            =	0x867,
  EVENT_T_HDR_LTE_RESEL_STATUS		                =	0x868,
  EVENT_LTE_TO_1X_TT               = 0x869,
  EVENT_LTE_TO_1X_HO               = 0x86A,
  EVENT_LTE_TO_1X_DL_GCSNA_MSG     = 0x86B,
  EVENT_LTE_TO_1X_UL_GCSNA_MSG       = 0x86C,
  EVENT_LTE_RRC_UL_MSG_MEAS_REPORT = 0x86D,
  EVENT_IMS_QIPCALL_SIP_SESSION_EVENT = 0x86E,
  EVENT_SIP_REG_START_EVENT           = 0x86F,
  EVENT_SIP_REG_REQ_SENT_EVENT        = 0x870,
  EVENT_SIP_REG_RESP_RCVD_EVENT       = 0x871,
  EVENT_SIP_REG_END_EVENT             = 0x872,
  EVENT_SIP_REG_DREG_START_EVENT      = 0x873,
  EVENT_SIP_REG_DREG_END_EVENT        = 0x874,
  EVENT_WCDMA_FACH_DRX_ACTION         = 0x875,
  EVENT_RRC_COMPLETE_SIB_RECEIVED     = 0x876,
  EVENT_1X_TO_LTE_RESEL_DONE          = 0x877,
  EVENT_SSR_SUBSYS_PWR_DOWN           = 0x878,
  EVENT_SSR_SUBSYS_PWR_UP             = 0x879,
  EVENT_DS_DSD_PREFERRED_RADIO        = 0x87A,
  EVENT_DS_DSD_TIMER_STARTED          = 0x87B,
  EVENT_DS_DSD_TIMER_STOPPED          = 0x87C,
  EVENT_DS_DSD_TIMER_EXPIRED          = 0x87D,
  EVENT_DS_DSD_RADIO_THROTTLE         = 0x87E,
  EVENT_DS_DSD_RADIO_UNTHROTTLE       = 0x87F,
  EVENT_TDSCDMA_TO_TDSCDMA_RESELECTION_START = 0x880,
  EVENT_TDSCDMA_TO_TDSCDMA_RESELECTION_END   = 0x881,
  EVENT_GSM_CS_CALL_ESTABLISHMENT_FAILURE    = 0x882,
  EVENT_GSM_CS_CALL_DROPPED                  = 0x883,
  EVENT_GSM_LOST_SERVICE                     = 0x884,
  EVENT_GSM_IN_SERVICE                       = 0x885,
  EVENT_GSM_PLMN_LIST_GSM_SUB_SEARCH_ENDED   = 0x886,
  EVENT_SD_WRLF_2MIN_TIMER_EXPIRY     = 0x887,
  EVENT_CM_RLF_RECOVERY_START         = 0x888,
  EVENT_CM_RLF_RECOVERY_END           = 0x889,
  EVENT_T_HDR_CONN_ATTEMPT_ENDED      = 0x88A,
  EVENT_T_HDR_CONN_TERMINATED         = 0x88B,
  EVENT_MM_TIMER_EXPIRY               = 0x88C,
  EVENT_GNSS_RESET_LOCATION_SERVICE_RECEIVED = 0x88D,
  EVENT_GNSS_RESET_LOCATION_SERVICE_DONE     = 0x88E,
  EVENT_TDSCDMA_HANDOVER_START        = 0x88F,
  EVENT_TDSCDMA_HANDOVER_END          = 0x890,
  EVENT_TDSCDMA_TS0_JDS_UPDATE        = 0x891,
  EVENT_TDSCDMA_NONTS0_JDS_UPDATE     = 0x892,
  EVENT_TDSCDMA_UE_PAGED              = 0x893,
  EVENT_TDSCDMA_DRX_REACQ             = 0x894,
  EVENT_TDSCDMA_DL_SYNC_STATUS        = 0x895,
  EVENT_TDSCDMA_HSDPA_STATUS          = 0x896,
  EVENT_TDSCDMA_HSUPA_STATUS          = 0x897,
  EVENT_GHDI_MVS_STATE                = 0x898,
  EVENT_LTE_RRC_CELL_RESEL_STARTED    = 0x899,
  EVENT_WCDMA_CM_STATE_CHANGE         = 0x89A,
  EVENT_TDSCDMA_TO_LTE_RESELECTION_START   = 0x89B,
  EVENT_TDSCDMA_TO_LTE_RESELECTION_END     = 0x89C,
  EVENT_TDSCDMA_TO_LTE_REDIRECTION_START   = 0x89D,
  EVENT_TDSCDMA_TO_LTE_REDIRECTION_END     = 0x89E,
  EVENT_TDSCDMA_DED_PRIORITIES_VALIDITY_TIMER_START   = 0x89F,
  EVENT_TDSCDMA_DED_PRIORITIES_VALIDITY_TIMER_EXPIRED = 0x8A0,
  EVENT_LTE_RRC_IRAT_CCO_FROM_EUTRAN_START = 0x8A1,
  EVENT_LTE_RRC_IRAT_CCO_FROM_EUTRAN_END   = 0x8A2,
  EVENT_HOCM_TRANSMITTED                   = 0x8A3,
  EVENT_RESERVED_000                  = 0x8A4,
  EVENT_RESERVED_001                  = 0x8A5,
  EVENT_RESERVED_002                  = 0x8A6,
  EVENT_RESERVED_003                  = 0x8A7,
  EVENT_RESERVED_004                  = 0x8A8,
  EVENT_RESERVED_005                  = 0x8A9,
  EVENT_RESERVED_006                  = 0x8AA,
  EVENT_RESERVED_007                  = 0x8AB,
  EVENT_RESERVED_008                  = 0x8AC,
  EVENT_RESERVED_009                  = 0x8AD,
  EVENT_DS_DSD_ATTACH_PDN_CHANGE      = 0x8AE,
  EVENT_TDSCDMA_INTER_RAT_PFR_START       = 0x8AF,
  EVENT_TDSCDMA_INTER_RAT_PFR_END         = 0x8B0,
  EVENT_TDSCDMA_RRC_INTER_RAT_CCO_START   = 0x8B1,
  EVENT_WCDMA_RRC_CSFB_SIB_CONTAINER      = 0x8B2,
  EVENT_LTE_RRC_EMBMS_OOS_WARN_IND        = 0x8B3,
  EVENT_LTE_RRC_EMBMS_ACT_TMGI_LIST_IND   = 0x8B4,
  EVENT_LTE_RRC_EMBMS_AVAIL_TMGI_LIST_IND = 0x8B5,
  EVENT_LTE_RRC_MCCH_CHANGE_NOTIFICATION  = 0x8B6,
  EVENT_RPM_MCC_MNC_VALUE                   = 0x8B7,
  EVENT_RPM_ENABLED                         = 0x8B8,
  EVENT_CPDP_COUNTERS_UPDATE                = 0x8B9,
  EVENT_RPM_START_LR3_TIMER                 = 0x8BA,
  EVENT_RPM_LR3_TIMER_EXPIRED               = 0x8BB,
  EVENT_RPM_BACKOFF_TIMER_VAL               = 0x8BC,
  EVENT_RPM_READ_ALL_PARAM                  = 0x8BD,
  EVENT_RPM_BACKOFF_TIMER_EXP               = 0x8BE,
  EVENT_LTE_SCELL_CONFIGURATION             = 0x8BF,
  EVENT_LTE_SCELL_ACTIVATION_COMMAND        = 0x8C0,
  EVENT_LTE_SCELL_DEACTIVATION_TIMER_EXPIRY = 0x8C1,
  EVENT_SCELL_STATE_CHANGE                  = 0x8C2,
  EVENT_GNSS_SPECTRUM_ANALYZER_STATUS       = 0x8C3,
  EVENT_GSM_TO_TDSCDMA_RESEL_START          = 0x8C4,
  EVENT_GSM_TO_TDSCDMA_RESEL_END            = 0x8C5,
  EVENT_AUDIO_MWS_DIAG_SERVICE              = 0x8C6,
  EVENT_WCDMA_ASF_TIMER_EXPIRED             = 0x8C7,
  EVENT_ASF_SCAN_START                      = 0x8C8,
  EVENT_ASF_SCAN_END                        = 0x8C9,
  EVENT_TDSCDMA_RRC_INTER_RAT_CCO_END       = 0x8CA,
  EVENT_SECAPITEST_TESTCASE_START           = 0x8CB,
  EVENT_SECAPITEST_TESTCASE_END             = 0x8CC,
  EVENT_TDSCDMA_RXD_STATE                   = 0x8CD,
  EVENT_WCDMA_RRC_ASF_TIMER_STATUS          = 0x8CE,
  EVENT_WCDMA_RRC_CSG_DETECTED              = 0x8CF,
  EVENT_CM_SSAC_CALL                        = 0x8D0,
  EVENT_CM_SSAC_TIME                        = 0x8D1,
  EVENT_PAGE_MATCH                          = 0x8D2,
  EVENT_CSFB_CALL_TYPE                      = 0x8D3,
  EVENT_OEM_START_ID                        = 0x8D4,
  /*Reserved for OEM . Range: 2260 - 2459 */
  EVENT_OEM_END_ID                          = 0x99B,
  EVENT_WCDMA_ASF_MEAS_REQ_RCVD             = 0x99C,
  EVENT_SECAPITEST_TESTCASE_SUCCESS         = 0x99D,
  EVENT_SECAPITEST_TESTCASE_FAILURE         = 0x99E,
  EVENT_HDR_MSG_RX_PAGE                     = 0x99F,
  EVENT_TDSCDMA_RRC_HANDOVER_START          = 0x9A0,
  EVENT_TDSCDMA_RRC_HANDOVER_END            = 0x9A1,
  EVENT_CODEC_INIT                          = 0x9A2,
  EVENT_CODEC_RATE_CHANGE                   = 0x9A3,
  EVENT_GPSXTRA_T_POSITION_INJECTION        = 0x9A4,
  EVENT_TDSCDMA_RRC_X_TO_TDSCDMA_HANDOVER_START = 0x9A5,
  EVENT_WCDMA_ENH_L1_STATE                  = 0x9A6,
  EVENT_CM_CALL_EVENT_ORIG_2                = 0x9A7,
  EVENT_CM_CALL_EVENT_CONNECT_2             = 0x9A8,
  EVENT_CM_CALL_EVENT_END_2                 = 0x9A9,
  EVENT_TDSCDMA_RRC_X_TO_TDSCDMA_HANDOVER_END = 0x9AA,
  EVENT_1X_TO_LTE_RESEL_START               = 0x9AB,
  EVENT_DS_DORMANCY_STATUS_UM_QUEUE_STATS   = 0x9AC,
  EVENT_DS_DORMANCY_STATUS_RM_QUEUE_STATS   = 0x9AD,
  EVENT_DS_CALL_STATUS                      = 0x9AE,
  EVENT_LTE_RRC_IRAT_HO_FROM_EUTRAN_END     = 0x9AF,
  EVENT_LTE_RRC_IRAT_HO_TO_EUTRAN_START     = 0x9B0,
  EVENT_LTE_RRC_UL_MSG_MEAS_REPORT_MEASOBJ_MODE = 0x9B1,
  EVENT_TDSCDMA_RRC_ACQUISITION_START       = 0x9B2,
  EVENT_LTE_SCELL_STATE_CHANGE_ENHANCED     = 0x9B3,
  EVENT_SD_EVENT_ACTION_HYBR2               = 0x9B4,
  EVENT_UMTS_NAS_CB_CTCH_START              = 0x9B5,
  EVENT_UMTS_NAS_CB_BMC_MSG_DECODE_FAIL     = 0x9B6,
  EVENT_WCDMA_RLC_UL_CONFIG                 = 0x9B7,
  EVENT_NAS_CB_CTCH_STOP                    = 0x9B8,
  EVENT_GPS_PD_COMM_NW_CONNECTING           = 0x9B9,
  EVENT_GPS_PD_COMM_NW_CONNECTED            = 0x9BA,
  EVENT_GPS_PD_COMM_SERVER_CONNECTING       = 0x9BB,
  EVENT_GPS_PD_COMM_SERVER_CONNECTED        = 0x9BC,
  EVENT_BROADCAST_CYCLE_DISABLED            = 0x9BD,
  EVENT_BROADCAST_CYCLE_ENABLED             = 0x9BE,
  EVENT_WCDMA_ASDIV_TYPE1_SWITCH_RSCP_BASED = 0x9BF,
  EVENT_WCDMA_ASDIV_TYPE1_SWITCH_TXAGC_BASED             = 0x9C0,
  EVENT_WCDMA_ASDIV_TYPE1_SWITCHBACK_MTPL_BASED          = 0x9C1,
  EVENT_WCDMA_ASDIV_TYPE1_SWITCHBACK_TXAGC_BASED         = 0x9C2,
  EVENT_WCDMA_ASDIV_TYPE2_SWITCH_RSCP_BASED              = 0x9C3,
  EVENT_WCDMA_ASDIV_TYPE2_SWITCH_MTPL_BASED              = 0x9C4,
  EVENT_WCDMA_ASDIV_TYPE2_SWITCH_RSCP_LOOPBACK_BASED     = 0x9C5,
  EVENT_WCDMA_ASDIV_TYPE2_SWITCHBACK_DEGRADATION_BASED   = 0x9C6,
  EVENT_WCDMA_ASDIV_PROBE_BOTH_ANTENNAS    = 0x9C7,
  EVENT_TDSCDMA_RRC_HSUPA_SUPPORT_STATUS   = 0x9C8,
  EVENT_WCDMA_HS_RACH_OP                   = 0x9C9,
  EVENT_WLAN_FW_SLM_CONS_BCN_MISS          = 0x9CA,
  EVENT_WLAN_FW_SLM_RSSI_MONITOR           = 0x9CB,
  EVENT_SD_HYBR2_BSR_START                 = 0x9CC,
  EVENT_SD_HYBR2_BSR_END                   = 0x9CD,
  EVENT_IWLAN_S2B_CALL_TRACER_LOGGING      = 0x9CE,
  EVENT_WCDMA_RRC_FAILURE                  = 0x9CF,
  EVENT_WCDMA_CME_STATE                    = 0x9D0,
  EVENT_WCDMA_RxD_STATE                    = 0x9D1,
  EVENT_WCDMA_RACH_ABORT_CAUSE             = 0x9D2,
  EVENT_CM_PH_DYN_SWITCH                   = 0x9D3,
  EVENT_TDSCDMA_RRC_DMCR_ENABLED           = 0x9D4,
  EVENT_ASDIV_TESTMODE1_SWITCH             = 0x9D5,
  EVENT_PM_UE_MODE                         = 0x9D6,
  EVENT_TDSCDMA_SELF_HOSTING               = 0x9D7,
  EVENT_LTE_ML1_SEARCH_IDLE                = 0x9D8,
  EVENT_GSTK_BIP_STATUS                    = 0x9D9,
  EVENT_TDSCDMA_RRC_DSDS_TA_BLOCK          = 0x9DA,
  EVENT_PS_SYSTEM_STATUS                   = 0x9DB,
  EVENT_QMI_SYSTEM_STATUS                  = 0x9DC,
  EVENT_PS_SYSTEM_STATUS_EX                = 0x9DD,
  EVENT_QMI_SYSTEM_STATUS_EX               = 0x9DE,
  EVENT_ASDIV_IDLE_SWITCH_FACH_INIT        = 0x9DF,
  EVENT_ASDIV_IDLE_SWITCHBACK_FACH_INIT    = 0x9E0,
  EVENT_ASDIV_IDLE_SWITCH_OOS_TIMER        = 0x9E1,
  EVENT_ASDIV_IDLE_SWITCHBACK_OOS_TIMER    = 0x9E2,
  EVENT_ASDIV_IDLE_SWITCH_RACH_NACK        = 0x9E3,
  EVENT_ASDIV_IDLE_SWITCHBACK_RACH_NACK    = 0x9E4,
  EVENT_ASDIV_ACQ_SWITCH                   = 0x9E5,
  EVENT_ASDIV_ACQ_SWITCHBACK               = 0x9E6,
  EVENT_LTE_RRC_PAGING_UE_ID               = 0x9E7,
  EVENT_LTE_ML1_ASDIV                      = 0x9E8,
  EVENT_LTE_RRC_CGI_START_INDI             = 0x9E9,
  EVENT_DS_UM_QUEUE_STATS_EX               = 0x9EA,
  EVENT_HSPA                               = 0x9EB,
  EVENT_TDSCDMA_TO_GSM_REDIRECTION_START   = 0x9EC,
  EVENT_TDSCDMA_TO_GSM_REDIRECTION_END     = 0x9ED,
  EVENT_LTE_RRC_EMBMS_AVAIL_SAI_LIST_IND   = 0x9EE,
  EVENT_LTE_RRC_EMBMS_INTEREST_IND         = 0x9EF,
  EVENT_LTE_RRC_EMBMS_SIGNAL_STRENGTH_REPORTING = 0x9F0,
  EVENT_GSM_VS_INTERFACE_TYPE_1            = 0x9F1,
  EVENT_GSM_VS_INTERFACE_TYPE_2            = 0x9F2,
  EVENT_GSM_VS_INTERFACE_GHDI_CONTROL      = 0x9F3,
  EVENT_WCDMA_EUL_REL7_FEAT_OP             = 0x9F4,
  EVENT_WCDMA_RRC_CU_STATUS                = 0x9F5,
  EVENT_WCDMA_HS_SCCH_ORDER_VER2           = 0x9F6,
  EVENT_AUDIO_PATH_DELAY_CHANGE            = 0x9F7,
  EVENT_LTE_RRC_CAMPED_CELL_STATUS         = 0x9F8,
  EVENT_DATAMODEM_IPA_DROP_PKT_DL          = 0x9F9,
  EVENT_DATAMODEM_IPA_DROP_PKT_UL          = 0x9FA,
  EVENT_NAS_LAU                            = 0x9FB,
  EVENT_NAS_ATTACH                         = 0x9FC,
  EVENT_NAS_RAU                            = 0x9FD,
  EVENT_NAS_MO_DETACH                      = 0x9FE,
  EVENT_NAS_MT_DETACH                      = 0x9FF,
  EVENT_NAS_TAU                            = 0xA00,
  EVENT_GSM_CELL_SELECTED                  = 0xA01,
  EVENT_GSM_DS_CELL_SELECTED               = 0xA02,
  EVENT_WLAN_QCA_BMISS                     = 0xA03,
  EVENT_WLAN_QCA_RSSI_MONITOR              = 0xA04,
  EVENT_DS_EPC_PDN_EX                                   = 0xA05,
  EVENT_DS_EPC_SRAT_CLEANUP_TIMER_EX                    = 0xA06,
  EVENT_DS_DSD_PREFERRED_RADIO_INFO                     = 0xA07,
  EVENT_LTE_RRC_NEW_CELL_IND_EXT_EARFCN                 = 0xA08,
  EVENT_LTE_RRC_CELL_RESEL_FAILURE_EXT_EARFCN           = 0xA09,
  EVENT_LTE_RRC_HO_FAILURE_EXT_EARFCN                   = 0xA0A,
  EVENT_LTE_RRC_SIB_READ_FAILURE_EXT_EARFCN             = 0xA0B,
  EVENT_LTE_RRC_CELL_RESEL_STARTED_EXT_EARFCN           = 0xA0C,
  EVENT_LTE_RRC_MCCH_CHANGE_NOTIFICATION_EXT_EARFCN     = 0xA0D,
  EVENT_LTE_RRC_CELL_BLACKLIST_IND_EXT_EARFCN           = 0xA0E,
  EVENT_WCDMA_RRC_INTER_FREQ_HHO_STATUS                 = 0xA0F,
  EVENT_DS_DSD_TIMER                               = 0xA10,
  EVENT_TDSCDMA_TO_TDSCDMA_REDIRECTION_START       = 0xA11,
  EVENT_TDSCDMA_TO_TDSCDMA_REDIRECTION_END         = 0xA12,
  EVENT_WLAN_QCA_HEARTBEAT_FAILURE                 = 0xA13,
  EVENT_DS_3GPP_SVC_THROTTLE                       = 0xA14,
  EVENT_DS_3GPP_PDN_THROTTLE                       = 0xA15,
  EVENT_GSM_TO_TDSCDMA_REDIR_STARTED               = 0xA16,
  EVENT_GSM_TO_TDSCDMA_REDIR_ENDED                 = 0xA17,
  EVENTS_DS_GSM_PAGE_MISSED                        = 0xA18,
  EVENT_GSM_TO_WCDMA_REDIR_STARTED                 = 0xA19,
  EVENT_GSM_TO_WCDMA_REDIR_ENDED                   = 0xA1A,
  EVENT_DS_RADIO_STACK_STATE                       = 0xA1B,
  EVENT_DS_CALL_CONTROL_STATE                      = 0xA1C,
  EVENT_DS_BEARER_CONTROL_STATE                    = 0xA1D,
  EVENT_LTE_SCELL_STATE_CHANGE_ENHANCED2           = 0xA1E,
  EVENT_LTE_SCELL_PCELL_TIME_DRIFT                 = 0xA1F,
  EVENT_LTE_SCELL_WB_NB_TIME_DRIFT                 = 0xA20,
  EVENTS_TDSCDMA_RRC_SIB_RCVD_V2                   = 0xA21,
  EVENT_LTE_ML1_EPHR_REPORT                        = 0xA22,
  EVENT_LTE_TO_LTE_REDIRECTION                     = 0xA23,
  EVENT_TDSCDMA_DA_BACKOFF_TIMER_START             = 0xA24,
  EVENT_TDSCDMA_DA_BACKOFF_TIMER_STOP              = 0xA25,
  EVENT_TDSCDMA_DA_BACKOFF_TIMER_EXPIRED           = 0xA26,
  EVENT_WCDMA_TO_GSM_REDIRECTION_START             = 0xA27,
  EVENT_WCDMA_TO_GSM_REDIRECTION_END               = 0xA28,
  EVENT_WCDMA_TO_WCDMA_REDIRECTION_START           = 0xA29,
  EVENT_WCDMA_TO_WCDMA_REDIRECTION_END             = 0xA2A,
  EVENT_GNSS_RCVR_STATE                            = 0xA2B,
  EVENT_GSM_SI_CACHE_UPDATE_ENTRY                  = 0xA2C,
  EVENT_GSM_SI_CACHE_RETRIEVE                      = 0xA2D,
  EVENT_GSM_SI_CACHE_FLUSH                         = 0xA2E,
  EVENT_NAS_PLMN_FOUND                             = 0xA2F,
  EVENT_LTE_SUB_FRAME_NUMBER	                   = 0xA30,
  EVENT_GNSS_POSITION_INCONSISTENCY                = 0xA31,
  EVENT_IPV6_EXT_ADDRESS                           = 0xA32,
  EVENT_DS3G_COEX_FLOW_CONTROL                     = 0xA33,
  EVENT_NAS_LTE_IRAT_SHORT_TIMER_STARTED           = 0xA34,
  EVENT_NAS_LTE_IRAT_LONG_TIMER_STARTED            = 0xA35,
  EVENT_NAS_LTE_IRAT_SEARCH_STARTED                = 0xA36,
  EVENT_NAS_HPLMN_IRAT_SEARCH_STARTED              = 0xA37,
  EVENT_NAS_LTE_IRAT_NOTAVAIL_TO_AVAIL             = 0xA38,
  EVENT_NAS_LTE_IRAT_AVAIL_TO_NOTAVAIL             = 0xA39,
  EVENT_DIAG_DROP_DEBUG                            = 0xA3A,
  EVENT_FULL_SRV_EMC_CALL                          = 0xA3B,
  EVENT_LTE_EMM_TIMER_STOP                         = 0xA3C,
  EVENT_LTE_ESM_TIMER_STOP                         = 0xA3D,
  EVENT_MCS_TRM_ASDIV                              = 0xA3E,
  EVENTS_DS_GSM_MESSAGE_SENT			   = 0xA3F,
  EVENTS_DS_GSM_COUNTER_EXPIRED                    = 0xA40,
  EVENTS_DS_GPRS_MAC_RESELECT_IND                  = 0xA41,
  EVENTS_DS_GPRS_CELL_UPDATE_START                 = 0xA42,
  EVENTS_DS_GPRS_CELL_UPDATE_END                   = 0xA43,
  EVENTS_DS_PACKET_RANDOM_ACCESS_REQ               = 0xA44,
  EVENTS_DS_GSM_L1_SUBSTATE	                   = 0xA45,
  EVENTS_DS_GSM_TO_WCDMA_HANDOVER_START            = 0xA46,
  EVENTS_DS_GSM_L1_VOCODER_INITIALIZE              = 0xA47,
  EVENTS_DS_GSM_L1_VOCODER_ENABLED                 = 0xA48,
  EVENTS_DS_GPRS_MAC_IDLE_IND                      = 0xA49,
  EVENTS_DS_GPRS_ACCESS_REJECT                     = 0xA4A,
  EVENTS_DS_GERAN_GRR_PLMN_LIST_RAT_SEARCH_STARTED = 0xA4B,
  EVENTS_DS_GERAN_GRR_PLMN_LIST_RAT_SEARCH_COMPLETED = 0xA4C,
  EVENTS_DS_GERAN_GRR_PLMN_LIST_RAT_SEARCH_ABORTED = 0xA4D,
  EVENTS_DS_GERAN_GRR_REDIR_STARTED                = 0xA4E,
  EVENTS_DS_GERAN_GRR_REDIR_COMPLETED              = 0xA4F,
  EVENTS_DS_GERAN_GRR_REDIR_ABORTED                = 0xA50,
  EVENTS_DS_GSM_TO_LTE_RESEL_STARTED               = 0xA51,
  EVENTS_DS_GSM_TO_LTE_RESEL_ENDED                 = 0xA52,
  EVENTS_DS_GSM_TO_LTE_REDIR_STARTED               = 0xA53,
  EVENTS_DS_GSM_TO_LTE_REDIR_ENDED                 = 0xA54,
  EVENTS_DS_GSM_CS_CALL_ESTABLISHMENT_FAILURE      = 0xA55,
  EVENTS_DS_GSM_CS_CALL_DROPPED                    = 0xA56,
  EVENTS_DS_GSM_LOST_SERVICE                       = 0xA57,
  EVENTS_DS_GSM_IN_SERVICE                         = 0xA58,
  EVENTS_DS_GSM_PLMN_LIST_GSM_SUB_SEARCH_ENDED     = 0xA59,
  EVENTS_DS_GSM_TO_TDSCDMA_RESEL_START             = 0xA5A,
  EVENTS_DS_GSM_TO_TDSCDMA_RESEL_END               = 0xA5B,
  EVENTS_DS_GSM_VS_INTERFACE_TYPE_1                = 0xA5C,
  EVENTS_DS_GSM_VS_INTERFACE_TYPE_2                = 0xA5D,
  EVENTS_DS_GSM_VS_INTERFACE_GHDI_CONTROL          = 0xA5E,
  EVENTS_DS_GSM_TO_TDSCDMA_REDIR_STARTED           = 0xA5F,
  EVENTS_DS_GSM_TO_TDSCDMA_REDIR_ENDED             = 0xA60,
  EVENTS_DS_GSM_TO_WCDMA_REDIR_STARTED             = 0xA61,
  EVENTS_DS_GSM_TO_WCDMA_REDIR_ENDED               = 0xA62,
  EVENT_GNSS_GDT_SESS_START_REQ                    = 0xA63,
  EVENT_GNSS_GDT_SESS_START_RESP                   = 0xA64,
  EVENT_GNSS_GDT_SEND_REQ                          = 0xA65,
  EVENT_GNSS_GDT_SEND_RESP                         = 0xA66,
  EVENT_GNSS_GDT_SESS_END_REQ                      = 0xA67,
  EVENT_GNSS_GDT_SESS_END_RESP                     = 0xA68,
  EVENT_GNSS_GDT_SESS_OPEN_REQ                     = 0xA69,
  EVENT_GNSS_GDT_SESS_CLOSE_REQ                    = 0xA6A,
  EVENT_GNSS_GTP_TDP_MEASURMENT_ENABLE             = 0xA6B,
  EVENT_GNSS_GTP_TDP_MEASURMENT_DISABLE            = 0xA6C,
  EVENT_GNSS_GTP_TDP_MEASURMENT_REQUEST            = 0xA6D,
  EVENT_GNSS_GTP_TDP_MEASURMENT_RCVD               = 0xA6E,
  EVENT_GPS_GTP_TDP_SESS_START                     = 0xA6F,
  EVENT_GPS_GTP_TDP_SESS_END                       = 0xA70,
  EVENT_WLAN_MAC_RESET                             = 0xA71,
  EVENT_PM_RAT_CHANGE                              = 0xA72,
  EVENT_DTM_ENABLED_UE                             = 0xA73,
  EVENT_DTM_ENABLED_CELL                           = 0xA74,
  EVENTS_DS_DTM_ENABLED_CELL                       = 0xA75,
  EVENT_LTE_RRC_UL_MSG_MEAS_REPORT_EXT             = 0xA76,
  EVENT_WCDMA_RRC_HSPA_RNTI                        = 0xA77,
  EVENT_WCDMA_CSFB_CALL_OPT                        = 0xA78,
  EVENT_GSM_RESELECT_END_V2                        = 0xA79,
  EVENT_GSM_CAMP_ATTEMPT_START_V2                  = 0xA7A,
  EVENTS_DS_GSM_RESELECT_END_V2                    = 0xA7B,
  EVENTS_DS_GSM_CAMP_ATTEMPT_START_V2              = 0xA7C,
  EVENT_CM_MSIM_INFO_CHG                           = 0xA7D,
  EVENT_LTE_TIMING_ADVANCE_V2                      = 0xA7E,
  EVENT_LTE_UL_OUT_OF_SYNC_V2                      = 0xA7F,
  EVENT_LTE_SPS_DEACTIVATED_V2                     = 0xA80,
  EVENT_LTE_RACH_ACCESS_START_V2                   = 0xA81,
  EVENT_LTE_RACH_ACCESS_RESULT_V2                  = 0xA82,
  EVENT_LTE_RACH_RAID_MATCH_V2                     = 0xA83,
  EVENT_LTE_MAC_RESET_V2                           = 0xA84,
  EVENT_LTE_BSR_SR_REQUEST_V2                      = 0xA85,
  EVENT_LTE_MAC_TIMER_V2                           = 0xA86,
  EVENT_CM_PLMN_BLOCK_REQ                          = 0xA87,
  EVENT_NAS_CC                                     = 0xA88,
  EVENT_NAS_SS                                     = 0xA89,
  EVENT_NAS_SMS                                    = 0xA8A,
  EVENT_NAS_SM                                     = 0xA8B,
  EVENT_NAS_ESM                                    = 0xA8C,
  EVENT_WLAN_EAPOL                                 = 0xA8D,
  EVENT_WLAN_EXTSCAN_FEATURE_STARTED               = 0xA8E,
  EVENT_WLAN_EXTSCAN_FEATURE_CHANNEL_CONFIG        = 0xA8F,
  EVENT_WLAN_EXTSCAN_CYCLE_STARTED                 = 0xA90,
  EVENT_WLAN_EXTSCAN_CYCLE_COMPLETED               = 0xA91,
  EVENT_WLAN_EXTSCAN_BUCKET_STARTED                = 0xA92,
  EVENT_WLAN_EXTSCAN_BUCKET_COMPLETED              = 0xA93,
  EVENT_WLAN_ROAM_SCAN_STARTED                     = 0xA94,
  EVENT_WLAN_ROAM_SCAN_COMPLETE                    = 0xA95,
  EVENT_WLAN_ROAM_CANDIDATE_FOUND                  = 0xA96,
  EVENT_WLAN_ROAM_SCAN_CONFIG                      = 0xA97,
  EVENT_WLAN_BT_COEX_BT_SCO_START                  = 0xA98,
  EVENT_WLAN_BT_COEX_BT_SCO_STOP                   = 0xA99,
  EVENT_WLAN_BT_COEX_BT_SCAN_START                 = 0xA9A,
  EVENT_WLAN_BT_COEX_BT_SCAN_STOP                  = 0xA9B,
  EVENT_WIFI_BT_COEX_BT_HID_START                  = 0xA9C,
  EVENT_WIFI_BT_COEX_BT_HID_STOP                   = 0xA9D,
  EVENT_IMS_FAR_END_RAT_TYPE_DETECTED              = 0xA9E,
  EVENT_IMS_RTP_REDUN_START                        = 0xA9F,
  EVENT_IMS_RTP_REDUN_END                          = 0xAA0,
  EVENT_SRVCC_SYNC_IND_SENT                        = 0xAA1,
  EVENT_WLAN_WAKE_LOCK                             = 0xAA2,
  EVENT_WLAN_EXTSCAN_FEATURE_STOP                  = 0xAA3,
  EVENT_WLAN_EXTSCAN_RESULTS_AVAILABLE             = 0xAA4,
  EVENT_WLAN_EXTSCAN_CAPABILITIES                  = 0xAA5,
  EVENT_WLAN_BEACON_RECEIVED                       = 0xAA6,
  EVENT_WLAN_LOG_COMPLETE                          = 0xAA7,
  EVENT_LTE_SCELL_STATE_CHANGE_ENHANCED3           = 0xAA8,
  EVENT_GSM_PAGE_MISSED                            = 0xAA9,
  EVENT_LTE_ML1_SLEEP                              = 0xAAA,
  EVENT_DS_NAS_CB_PAGE_RECEIVED                    = 0xAAB,
  EVENT_DS_SM_PDP_STATE                            = 0xAAC,
  EVENT_DS_SMGMM_REQUEST_SENT                      = 0xAAD,
  EVENT_DS_SMGMM_REJECT_RECEIVED                   = 0xAAE,
  EVENT_DS_SMS_STATISTICS                          = 0xAAF,
  EVENT_DS_UMTS_CALLS_STATISTICS                   = 0xAB0,
  EVENT_DS_GHDI_MVS_STATE                          = 0xAB1,
  EVENT_DS_MVS_STATE                               = 0xAB2,
  EVENT_WLAN_STATUS_V2                             = 0xAB3,
  EVENT_VS_VOCODER_STATE                           = 0xAB4,
  EVENT_WLAN_TDLS_TEARDOWN                         = 0xAB5,
  EVENT_WLAN_TDLS_ENABLE_LINK                      = 0xAB6,
  EVENT_WLAN_SUSPEND_RESUME                        = 0xAB7,
  EVENT_WLAN_OFFLOAD_REQ                           = 0xAB8,
  EVENT_WLAN_TDLS_SCAN_BLOCK                       = 0xAB9,
  EVENT_WLAN_TX_RX_MGMT                            = 0xABA,
  EVENT_WLAN_LOW_RESOURCE_FAILURE                  = 0xABB,
  EVENT_WLAN_FW_CONN_MGMT_SEND                         = 0xABC,
  EVENT_WLAN_FW_CONN_MGMT_SEND_FAILED                  = 0xABD,
  EVENT_WLAN_FW_CONN_MGMT_RECV                         = 0xABE,
  EVENT_WLAN_FW_EAPOL_SEND                             = 0xABF,
  EVENT_WLAN_FW_EAPOL_SEND_FAILED                      = 0xAC0,
  EVENT_WLAN_FW_KEY_INSTALL                            = 0xAC1,
  EVENT_WLAN_FW_DISCONNECT                             = 0xAC2,
  EVENT_WLAN_FW_ACTIVE_CONNECTIONS                     = 0xAC3,
  EVENT_WLAN_FW_BEACON_MISS                            = 0xAC4,
  EVENT_WLAN_FW_PEER_UPDATE                            = 0xAC5,
  EVENT_WLAN_FW_CONCURRENCY_UPDATE                     = 0xAC6,
  EVENT_WLAN_FW_SCAN_START                             = 0xAC7,
  EVENT_WLAN_FW_SCAN_END                               = 0xAC8,
  EVENT_WLAN_FW_ROAM_STATUS                            = 0xAC9,
  EVENT_WLAN_FW_ROAM_FAIL                              = 0xACA,
  EVENT_WLAN_FW_P2P_KEEPALIVE_START                    = 0xACB,
  EVENT_WLAN_FW_QUIET_START                            = 0xACC,
  EVENT_WLAN_FW_QUIET_END                              = 0xACD,
  EVENT_WLAN_FW_BEACON_START                           = 0xACE,
  EVENT_WLAN_FW_RTT_MEAS_REQ                           = 0xACF,
  EVENT_WLAN_FW_RTT_MEAS_ERR                           = 0xAD0,
  EVENT_WLAN_FW_RTT_INIT_TX_ERR                        = 0xAD1,
  EVENT_WLAN_FW_RTT_RESP_RX_FTMR                       = 0xAD2,
  EVENT_WLAN_FW_RTT_RESP_TX_ERR_FTM                    = 0xAD3,
  EVENT_WLAN_FW_RTT_INIT_RX_FTM                        = 0xAD4,
  EVENT_WLAN_FW_RTT_CH_REQ_ERROR                       = 0xAD5,
  EVENT_WLAN_LPI_SNOOP_CFG                             = 0xAD6,
  EVENT_WLAN_LPI_START_SCAN                            = 0xAD7,
  EVENT_WLAN_LPI_SCAN_RESULT                           = 0xAD8,
  EVENT_WLAN_LPI_STATUS                                = 0xAD9,
  EVENT_WLAN_FW_TDLS_LINK_STATE                        = 0xADA,
  EVENT_WLAN_FW_TDLS_TEARDOWNTRIGGER                   = 0xADB,
  EVENT_WLAN_FW_TDLS_INFO                              = 0xADC,
  EVENT_WLAN_FW_TDLS_TXFAIL_INFO                       = 0xADD,
  EVENT_WLAN_FW_RADARDETECT                            = 0xADE,
  EVENT_WLAN_FW_CAC_START                              = 0xADF,
  EVENT_WLAN_FW_CAC_END                                = 0xAE0,
  EVENT_WLAN_FW_WOW_ENTER                              = 0xAE1,
  EVENT_WLAN_FW_WOW_WAKE_UP                            = 0xAE2,
  EVENT_WLAN_FW_WOW_EXIT                               = 0xAE3,
  EVENT_WLAN_FW_OFFLOAD_NLO_CFG                        = 0xAE4,
  EVENT_WLAN_FW_OFFLOAD_CSA_RECV_BEACON_ACTION         = 0xAE5,
  EVENT_WLAN_FW_OFFLOAD_CSA_TIMEOUT_HANDLE_ERROR       = 0xAE6,
  EVENT_WLAN_FW_OFFLOAD_GTK_CFG                        = 0xAE7,
  EVENT_WLAN_FW_OFFLOAD_GTK_RECV_REKEY                 = 0xAE8,
  EVENT_WLAN_FW_OFFLOAD_ARP_NS_CFG                     = 0xAE9,
  EVENT_WLAN_FW_OFFLOAD_ARP_NS_RECV_HANDLE_ERROR       = 0xAEA,
  EVENT_WLAN_FW_OFFLOAD_CHATTER_SWITCH_ERROR           = 0xAEB,
  EVENT_WLAN_FW_OFFLOAD_CHATTER_HANDLE_ERROR           = 0xAEC,
  EVENT_WLAN_FW_STA_POWER_STATEUPDATE                  = 0xAED,
  EVENT_WLAN_FW_POWER_VOTERCVD                         = 0xAEE,
  EVENT_WLAN_FW_STA_POWER_STATE_EVENT                  = 0xAEF,
  EVENT_WLAN_FW_STA_POWERDOWN_ABORT                    = 0xAF0,
  EVENT_WLAN_FW_EBT                                    = 0xAF1,
  EVENT_WLAN_FW_COMMONSS_STATE                         = 0xAF2,
  EVENT_WLAN_MAC_WPM_STATE                             = 0xAF3,
  EVENT_WLAN_FW_WPM_STATE_EVENT                        = 0xAF4,
  EVENT_WLAN_FW_WPM_VOTE                               = 0xAF5,
  EVENT_WLAN_FW_WHAL_POWER_MODE                        = 0xAF6,
  EVENT_WLAN_FW_PEER_CREATE                            = 0xAF7,
  EVENT_WLAN_FW_PEER_DELETE                            = 0xAF8,
  EVENT_WLAN_FW_AST_ADD                                = 0xAF9,
  EVENT_WLAN_FW_AST_REMOVE                             = 0xAFA,
  EVENT_WLAN_FW_CHANNEL_CHANGE_START                   = 0xAFB,
  EVENT_WLAN_FW_SWBA                                   = 0xAFC,
  EVENT_WLAN_FW_TBTT_OFFSET_UPDATE                     = 0xAFD,
  EVENT_WLAN_FW_CHAINMASK_UPDATE                       = 0xAFE,
  EVENT_WLAN_FW_HWQ_EMPTY                              = 0xAFF,
  EVENT_WLAN_FW_MAC_RESET                              = 0xB00,
  EVENT_WLAN_FW_DESC_BIN_WM_HIT                        = 0xB01,
  EVENT_WLAN_FW_MAC_WDOG                               = 0xB02,
  EVENT_WLAN_FW_MODE_CHANGE                            = 0xB03,
  EVENT_LTE_ML1_MODE_SWITCH                            = 0xB04,
  EVENT_WLAN_PROXY_SM                                  = 0xB05,
  EVENT_WCDMA_DL_L1_RLF_IND                            = 0xB06,
  EVENT_WLAN_PNO_START_STOP                            = 0xB07,
  EVENT_LTE_RAR_TIMING_ADVANCE                         = 0xB08,
  EVENT_IWLAN_S2B_SM                                   = 0xB09,
  EVENT_QDR_SENSOR_DATA_PRODUCED                       = 0xB0A,
  EVENT_QDR_SENSOR_DATA_CONSUMED                       = 0xB0B,
  EVENT_QDR_A6DOF_NF_STATE_REPORT                      = 0xB0C,
  EVENT_QDR_NAVLITE_START                              = 0xB0D,
  EVENT_QDR_NAVLITE_END                                = 0xB0E,
  EVENT_IPA_FLTR_INSTALLED_NOTIF_REQ_UL                = 0xB0F,
  EVENT_IPA_FLTR_INSTALL_REQ_DL                        = 0xB10,
  EVENT_IPA_FLOW_CONTROL                               = 0xB11,
  EVENT_IPA_QUOTA_SET                                  = 0xB12,
  EVENT_IPA_QUOTA_REACH                                = 0xB13,
  EVENT_IPA_TCP_ACK_COAL_SET                           = 0xB14,


  EVENT_LAST_ID    = 0xB14,
  EVENT_NEXT_UNUSED_EVENT,

  EVENT_MAX_ID     = 0x0FFF
} event_id_enum_type;

/*===========================================================================

                        TYPE DEFINITIONS FOR EVENTS

          NOTE: Type definitions for new events are now added by
                the responsible engineer in the target VU:

                (Do not add payload definitions to this file).

===========================================================================*/
typedef unsigned char event_band_class_change_type;
typedef unsigned short event_cdma_ch_change_type;
typedef unsigned char event_bs_p_rev_change_type;
typedef unsigned char event_p_rev_in_use_change_type;
typedef unsigned short event_sid_change_type;
typedef unsigned short event_nid_change_type;
typedef unsigned short event_pzid_change_type;
typedef unsigned char event_op_mode_change_type;

typedef PACK(struct)
{
   unsigned char channel;
   unsigned char msg_id;
} event_message_type;

typedef unsigned char event_timer_expired_type;
typedef unsigned char event_counter_threshold_type;

typedef PACK(struct)
{
   unsigned char old_state;
   unsigned char new_state;
} event_call_processing_state_change_type;

typedef unsigned char event_call_control_instantiated_type;

typedef PACK(struct)
{
   unsigned con_ref      : 8;
   unsigned old_substate : 4;
   unsigned new_substate : 4;
} event_call_control_state_change_type;

typedef unsigned char event_call_control_terminated_type;
typedef unsigned short event_reg_zone_change_type;
typedef unsigned char event_slotted_mode_operation_type;
typedef unsigned char event_qpch_in_use_type;
typedef unsigned short event_idle_handoff_type;
typedef unsigned short event_ms_access_handoff_type;
typedef unsigned short event_ms_access_probe_handoff_type;

typedef PACK(struct)
{
  unsigned short pn[6];
  PACK(struct)
  {
    unsigned s1       : 1;
    unsigned s2       : 1;
    unsigned s3       : 1;
    unsigned s4       : 1;
    unsigned s5       : 1;
    unsigned s6       : 1;
    unsigned reserved : 10;
  } in_sch_aset;
} event_ms_handoff_type;

typedef unsigned long event_tmsi_assigned_type;
typedef unsigned char event_end_rev_supp_burst_assgn_type;
typedef unsigned char event_ch_ind_type;

typedef PACK(struct)
{
   unsigned char pagech;
   unsigned short pn_offset;
} event_pch_acquired_type;

typedef PACK(struct)
{
   unsigned char bcch;
   unsigned short pn_offset;
} event_bcch_acquired_type;

typedef unsigned char event_registration_performed_type;

typedef PACK(struct)
{
   unsigned char ecio;
   unsigned char ps;
} event_system_reselection_type;

/*!
@endcond
*/
#endif /* EVENT_DEFS_H */
