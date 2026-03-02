#ifndef DIAGDIAG_H
#define DIAGDIAG_H
/*==========================================================================

                      Diagnostic Packet Definitions

  Description: Packet definitions between the diagnostic subsystem
  and the external device.

  !!! NOTE: All member structures of diag packets must be PACKED.

  !!! WARNING: Each command code number is part of the externalized
  diagnostic command interface.  This number *MUST* be assigned
  by a member of QCT's tools development team.

# Copyright (c) 2007-2011, 2014, 2016 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
===========================================================================*/

/* <EJECT> */
/*===========================================================================

                            Edit History

  $Header: //depot/asic/msmshared/services/diag/DIAG_7K/diagdiag.h#2 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ      Changes for CBSP2.0
05/17/05   as      Added Dual processor Diag support.
06/15/04   gr      Added support for getting and setting the event mask.
05/18/04   as      Removed support for DIAG_USER_CMD_F & DIAG_PERM_USER_CMD_F
01/07/03   djm     add RPC support for WCDMA_PLT
08/20/02   lad     Moved DIAG_DLOAD_F packet def to dloaddiag.h.
                   Moved DIAG_SERIAL_CHG_F definition to diagcomm_sio.c.
01/28/02   as      Support for DIAG_LOG_ON_DEMAND_F (log on demand).
09/18/01   jal     Support for DIAG_CONTROL_F (mode reset/offline)
08/20/01   jal     Support for Diag packet: DIAG_TS_F (timestamp),
                   DIAG_SPC_F (service programming code), DIAG_DLOAD_F
		   (start downloader), DIAG_OUTP_F/DIAG_OUTPW_F (IO
		   port byte/word output), DIAG_INP_F/DIAG_INPW_F (IO
		   port byte/word input)
06/27/01   lad     Use of constants in message response packet.
                   Added packet definition for DIAG_LOG_CONFIG_F.  This
                   replaces extended logmask processing.
                   Cleaned up DIAG_STREAMING_CONFIG_F and added subcmd to
                   get diagbuf size.
04/06/01   lad     Added packet definitions for the following:
                   Peek/Poke
                   DIAG_STREAMING_CONFIG_F
                   Debug messages
                   Log services
                   Event services
02/23/01   lad     Created file.

===========================================================================*/

#include "diagcmd.h"
#include "diagpkt.h"
#include "log_codes.h"
//#include "feature.h"
#include "diag.h"
#include "diagi.h"

/* Type to hold security code.  The security code is fixed length and is   */
/* stored as ASCII string.                                                 */

#define  DIAGPKT_SEC_CODE_SIZE 6               /* 6 digit security code */

typedef PACK(struct {
  byte      digits[DIAGPKT_SEC_CODE_SIZE];     /* Security code array */
}) diagpkt_sec_code_type;


/* -------------------------------------------------------------------------
** Packet Definitions
** ------------------------------------------------------------------------- */

/*==========================================================================

PACKET   DIAG_RPC_F

PURPOSE  RPC Processing Request

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_RPC_F )

  /* It's just the command code */

DIAGPKT_REQ_END

DIAGPKT_DEFINE_RSP_AS_REQ( DIAG_RPC_F )

/*==========================================================================

PACKETS  DIAG_PEEKB_F
         DIAG_PEEKW_F
         DIAG_PEEKD_F
PURPOSE  Sent from the DM to the DMSS to request a read of a block of data.
         The cmd_code specifies byte, word or dword.

============================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_PEEKB_F)

  byte *ptr;                   /* starting address for peek operation */
  word length;                 /* number of bytes, words, or dwords  to be
                                  returned */
DIAGPKT_REQ_END

typedef DIAG_PEEKB_F_req_type DIAG_PEEKW_F_req_type;
typedef DIAG_PEEKB_F_req_type DIAG_PEEKD_F_req_type;
typedef DIAG_PEEKB_F_req_type diag_peek_req_type;

#define DIAG_MAX_PEEK_B 16
  /* Maximum number of bytes that can be requested in one Peekb request */

DIAGPKT_RSP_DEFINE(DIAG_PEEKB_F)

  byte *ptr;                   /* starting address for peek operation */
  word length;                 /* number of bytes to be returned */
  byte data[DIAG_MAX_PEEK_B];  /* bytes from memory */

DIAGPKT_RSP_END

#define DIAG_MAX_PEEK_W  8
  /* Maximum number of words that can be requested in one Peekw request */

/* Peekw Response type */
DIAGPKT_RSP_DEFINE(DIAG_PEEKW_F)

  word *ptr;                   /* starting address for peek operation */
  word length;                 /* number of words to be returned */
  word data[DIAG_MAX_PEEK_W];  /* words from memory */

DIAGPKT_RSP_END

#define DIAG_MAX_PEEK_D  4
  /* Maximum number of dwords that can be requested in one Peekd request */

DIAGPKT_RSP_DEFINE(DIAG_PEEKD_F)

  dword *ptr;                  /* starting address for peek operation */
  word length;                 /* number of dwords to be returned */
  dword data[DIAG_MAX_PEEK_D]; /* dwords from memory */

DIAGPKT_RSP_END

typedef DIAG_PEEKB_F_rsp_type diag_peekb_rsp_type;
typedef DIAG_PEEKW_F_rsp_type diag_peekw_rsp_type;
typedef DIAG_PEEKD_F_rsp_type diag_peekd_rsp_type;


/*===========================================================================

PACKET   DIAG_POKEB_F
         DIAG_POKEW_F
         DIAG_POKED_F
PURPOSE  Sent by the DM to the DMSS to request that a block of data be
         written starting at a specified address.

============================================================================*/
#define DIAG_MAX_POKE_B 4
  /* Maximum number of bytes allowed for one pokeb request */

DIAGPKT_REQ_DEFINE(DIAG_POKEB_F)
  byte *ptr;                   /* starting address for poke operation */
  byte length;                 /* number of bytes to be poked */
  byte data[DIAG_MAX_POKE_B];  /* bytes to be placed at the addr address */
DIAGPKT_REQ_END

typedef DIAG_POKEB_F_req_type DIAG_POKEB_F_rsp_type;


#define DIAG_MAX_POKE_W 2
  /* Maximum number of words allowed for one pokew request */

DIAGPKT_REQ_DEFINE(DIAG_POKEW_F)
  word *ptr;                   /* starting address for poke operation */
  byte length;                 /* number of words to be poked */
  word data[DIAG_MAX_POKE_W];  /* words to be placed at the addr address */
DIAGPKT_REQ_END

typedef DIAG_POKEW_F_req_type DIAG_POKEW_F_rsp_type;

#define DIAG_MAX_POKE_D 2
  /* Maximum number of dwords allowed for one poked request this is 2 to
     allow possible growth to handle qwords. */

DIAGPKT_REQ_DEFINE(DIAG_POKED_F)
  dword *ptr;                  /* starting address for poke operation */
  byte length;                 /* number of dwords to be poked */
  dword data[DIAG_MAX_POKE_D]; /* dword to be placed at the addr address */
DIAGPKT_REQ_END

typedef DIAG_POKED_F_req_type DIAG_POKED_F_rsp_type;


/*===========================================================================

PACKET   DIAG_BAD_CMD_F
         DIAG_BAD_PARM_F
         DIAG_BAD_LEN_F
         DIAG_BAD_VOC_F
         DIAG_BAD_MODE_F
         DIAG_BAD_SPC_MODE_F

PURPOSE  Sent by DMSS when it detects an erroneous packet from DM. Errors
         include command code out of bounds, bad length...  Includes the
         first DIAG_MAX_ERR bytes of the offending input packet.
         Also includes when an nv_read/write is attempted before the correct
         SPC has been entered.

============================================================================*/
#define DIAGPKT_MAX_ERR 16
  /* maximum number of bytes (starting with the first byte of the packet )
     from the received packet which will be echoed back to the
     Diagnostic Monitor if an error is detected in the packet */

/* Type to communicate an error in a received packet */
DIAGPKT_RSP_DEFINE(DIAG_BAD_CMD_F)
  byte pkt[DIAGPKT_MAX_ERR];   /* first 16 bytes of received packet */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_DIAG_VER_F
PURPOSE  Sent by DM to request the version of the diag

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_DIAG_VER_F)
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_DIAG_VER_F)
  word ver;                    /* diag version */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_PASSWORD_F
PURPOSE  Sent by external device to enter the Security Password,
         to then allow operations protected by security.  This response
         indicates whether the correct Password was given or not.

NOTE     If the incorrect password is entered, DIAG will POWER DOWN
         the phone.

===========================================================================*/
#define DIAG_PASSWORD_SIZE 8
DIAGPKT_REQ_DEFINE(DIAG_PASSWORD_F)
  byte password[DIAG_PASSWORD_SIZE];  /* The security password */
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_PASSWORD_F)
  boolean password_ok;        /* TRUE if Security Password entered correctly */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_ERR_CLEAR_F
PURPOSE  Sent by the DM to clear the requested buffered error records

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_ERR_CLEAR_F)
  byte rec; /* record id, or -1 (0xFF) for all */
DIAGPKT_REQ_END

typedef DIAG_ERR_CLEAR_F_req_type DIAG_ERR_CLEAR_F_rsp_type;

/*===========================================================================

PACKET   DIAG_ERR_READ_F
PURPOSE  Sent by the DM to request the buffered error records

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_ERR_READ_F)
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_ERR_READ_F)
  word              log_cnt;            /* how many logged */
  word              ignore_cnt;         /* how many ignored */

  PACK(struct {
    byte      address;      /* Storage address 0 to       */
                            /* ERR_MAX_LOG-1              */
    byte      err_count;    /* Number of occurances       */
                            /* (0=empty,FF=full)          */
    byte      file_name[8];
                            /* File name string */
    word      line_num;     /* Line number in file */
    boolean   fatal;        /* TRUE if fatal error */
  }) err_logs[ 20 ];
DIAGPKT_RSP_END


/* Logging Services */

/*===========================================================================

PACKET   DIAG_LOG_CONFIG_F
PURPOSE  Sent by the DM to set the equipment ID logging mask in the DMSS.
         This is necessary to use logging services with MS Equip ID != 1.

!!!Note that the log mask is now sanely ordered LSB to MSB using little endian
32-bit integer arrays.  This is not the same way the mask was done in
DIAG_EXT_LOGMASK_F.

TERMINOLOGY:
  'equipment ID' - the 4-bit equipment identifier
  'item ID' - the 12-bit ID that specifies the log item within this equip ID
  'code' - the entire 16-bit log code (contains both equip and item ID)

===========================================================================*/
typedef enum {
  LOG_CONFIG_DISABLE_OP = 0,
  LOG_CONFIG_RETRIEVE_ID_RANGES_OP = 1,
  LOG_CONFIG_RETRIEVE_VALID_MASK_OP = 2,
  LOG_CONFIG_SET_MASK_OP = 3,
  LOG_CONFIG_GET_LOGMASK_OP = 4
} log_config_command_ops_enum_type;

/* Logging config return status types.
 * (*) denotes applicable to all commands
 */
typedef enum {
  LOG_CONFIG_SUCCESS_S = 0,          /* Operation Sucessful */
  LOG_CONFIG_INVALID_EQUIP_ID_S = 1, /* (*) Specified invalid equipment ID */
  LOG_CONFIG_NO_VALID_MASK_S = 2     /* Valid mask not available for this ID */
} log_config_status_enum_type;

/* Operation data */
/* DISABLE OP: LOG_CONFIG_DISAPLE_OP -no no supporting data */

/* These member structures are not packed intentionally.  Each data member will
 * align on a 32-bit boundary.
 */
typedef PACK(struct) {
  uint32 equip_id;

  uint32 last_item;

} log_config_range_type;

typedef PACK(struct) {
  log_config_range_type code_range; /* range of log codes */

  byte mask[1]; /* Array of 8 bit masks of size (num_bits + 7) / 8 */
} log_config_mask_type;

/* ID_RANGE_OP  response type */
typedef PACK(struct) {
  uint32 last_item[16]; /* The last item for each of the 16 equip IDs */
} log_config_ranges_rsp_type;

/* VALID_MASK_OP request type */
typedef PACK(struct) {
  uint32 equip_id;
} log_config_valid_mask_req_type;

/* VALID_MASK_OP response type */
typedef log_config_mask_type log_config_valid_mask_rsp_type;

/* SET_MASK_OP request type */
typedef log_config_mask_type log_config_set_mask_req_type;

/* GET_MASK_OP response type */
typedef log_config_mask_type log_config_get_mask_rsp_type;

/* SET_MASK_OP response type */
typedef log_config_mask_type log_config_set_mask_rsp_type;

/* This is not packed.  We use uint32 which is always aligned */
typedef PACK(union) {
  /* LOG_CONFIG_DISABLE_OP */
  /* no additional data */

  /* LOG_CONFIG_RETRIEVE_ID_RANGES_OP */
  /* no additional data */

  /* LOG_CONFIG_RETRIEVE_VALID_MASK_OP */
  log_config_valid_mask_req_type valid_mask;

  /* LOG_CONFIG_SET_MASK_OP */
  log_config_set_mask_req_type set_mask;

  /* LOG_CONFIG_GET_MASK_OP */
  /* no additional data */

} log_config_op_req_type;

typedef PACK(union) {
  /* LOG_CONFIG_DISABLE_OP */
  /* no additional data */

  /* LOG_CONFIG_RETRIEVE_ID_RANGES_OP */
  log_config_ranges_rsp_type ranges;

  /* LOG_CONFIG_RETRIEVE_VALID_MASK_OP */
  log_config_valid_mask_rsp_type valid_mask;

  /* LOG_CONFIG_SET_MASK_OP */
  log_config_set_mask_rsp_type set_mask;

  /* LOG_CONFIG_GET_MASK_OP */
  log_config_get_mask_rsp_type get_mask;

} log_config_op_rsp_type;


DIAGPKT_REQ_DEFINE(DIAG_LOG_CONFIG_F)

  byte pad[3]; /* Force following items to be on 32-bit boundary */

  uint32 operation;  /* See log_config_command_ops_enum_type */

  uint32 op_data[1]; /* Pointer to operation data */

DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_LOG_CONFIG_F)

  byte pad[3]; /* Force following items to be on 32-bit boundary */

  uint32 operation;  /* See log_config_command_ops_enum_type */

  uint32 status;

  uint32 op_data[1]; /* Pointer to operation data */

DIAGPKT_RSP_END


/* Number of bits in a log mask.
*/
#define DIAG_EXT_LOGMASK_NUM_BITS (LOG_1X_LAST_C & 0x0FFF)

/* Max # of bytes in a valid log mask.
*/
#define DIAG_EXT_LOGMASK_NUM_BYTES ((DIAG_EXT_LOGMASK_NUM_BITS / 8) + 1)

/*===========================================================================

PACKET   DIAG_EXT_LOGMASK_F
PURPOSE  Sent by the DM to set the logging mask in the DMSS.  This is
         necessary for logmasks > 32 bits.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_EXT_LOGMASK_F)
  word  num_bits;                 /* Number of valid bits */
  byte  mask[DIAG_EXT_LOGMASK_NUM_BYTES]; /* mask to use          */
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_EXT_LOGMASK_F)
  word  num_valid_bits;                     /* Number of valid bits    */
  byte  valid_mask[DIAG_EXT_LOGMASK_NUM_BYTES]; /* mask of valid log codes */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_LOG_F
PURPOSE  Encapsulates a log record.

===========================================================================*/

typedef struct
{
  uint8 cmd_code;
  uint8 more;   /* Indicates how many log entries, not including the one
                   returned with this packet, are queued up in the Mobile
                   Station.  If DIAG_DIAGVER >= 8, this should be set to 0 */
  uint16 len;   /* Indicates the length, in bytes, of the following log entry */
  uint8 log[1]; /* Contains the log entry data. */
}
diag_log_rsp_type;


/* -------------------------------------------------------------------------
** Legacy (but still supported) packet definitions for logging services.
** ------------------------------------------------------------------------- */
/*===========================================================================

PACKET   DIAG_LOGMASK_F
PURPOSE  Sent by the DM to set the 32-bit logging mask in the DMSS.
         Note: this is the legacy logging mask format.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_LOGMASK_F)
  uint32 mask; /* 32-bit log mask  */
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_LOGMASK_F)
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_SUBSYS_CMD_F
PURPOSE  This is a wrapper command to dispatch diag commands to various
         subsystems in the DMSS.  This allows new diag commands to be
         developed in those subsystems without requiring a change to diag
         code.

         The use of this command allows subsystems to manage their own
         diagnostics content.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_SUBSYS_CMD_F)

  uint8  subsys_id;
  uint16 subsys_cmd_code;

  uint8 pkt[1]; /* The subsystem's request.  Variable length. */

DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_SUBSYS_CMD_F)

  uint8  subsys_id;
  uint16 subsys_cmd_code;

  uint8 pkt[1]; /* The subsystem's response.  Variable length. */

DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_FEATURE_QUERY_F
PURPOSE  Sent by external device to query the phone for a bit mask detailing
         which phone features are turned on.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_FEATURE_QUERY_F)
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_FEATURE_QUERY_F)
  word feature_mask_size;                 /* Size of the following Mask */
  byte feature_mask[FEATURE_MASK_LENGTH]; /* Space for the largest possible
                                             feature mask */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_EVENT_REPORT_F
PURPOSE  Sent by the DM to configure static event reporting in the DMSS.

===========================================================================*/
/*--------------------------------------
  Special bit flags in the event ID.
--------------------------------------*/
#define EVENT_PAY_LENGTH   0x3
#define EVENT_PAY_TWO_BYTE 0x2
#define EVENT_PAY_ONE_BYTE 0x1
#define EVENT_PAY_NONE     0x0

/* Bitfields may not be ANSI, but all our compilers
** recognize it and *should* optimize it.
** Not that bit-packed structures are only as long as they need to be.
** Even though we call it uint32, it is a 16 bit structure.
*/
typedef struct
{
  uint16 id              : 12;
  uint16 reserved        : 1;
  uint16 payload_len     : 2; /* payload length (0, 1, 2, see payload) */
  uint16 time_trunc_flag : 1;
} event_id_type;

typedef PACK(struct)
{
  uint16 id; /* event_id_type id; */
  //qword ts;
  // removed AMSS code for CBSP 2.0
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;

} event_type;

/* Used in lieu of event_type if 'time_trunc_flag' is set in event_id_type */
typedef PACK(struct)
{
  uint16 id; /* event_id_type id; */
  uint16 trunc_ts;
} event_trunc_type;

/* The event payload follows the event_type structure */
typedef struct
{
  uint8 length;
  uint8 payload[1]; /* 'length' bytes */
} event_payload_type;

typedef PACK(struct)
{
  uint16 id; /* event_id_type id; */
    // removed AMSS code for CBSP 2.0
  //qword ts;
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;
  uint32 drop_cnt;
} event_drop_type;

typedef struct
{
  event_id_type id;
  uint16 ts;
  uint32 drop_cnt;
} event_drop_trunc_type;

typedef struct
{
  uint8 cmd_code;
  uint8 enable;
//  uint16  watermark;   /* Maximum size (in bytes) of a event report         */
//  uint16  stale_timer; /* Time (in ms) to allow event buffer to accumulate  */

} event_cfg_req_type;

typedef PACK(struct)
{
  uint8  cmd_code;
  uint16 length;

} event_cfg_rsp_type;

typedef PACK(struct)
{
  uint8  cmd_code;
  uint16 length;    /* Number of bytes to follow */
  uint8  events[1]; /* Series of 'event_type' structures, 'length' bytes long */
} event_rpt_type;

/*===========================================================================

PACKET   DIAG_STREAMING_CONFIG_F
PURPOSE  Sent by the DM to configure and tweak streaming diag output services.

===========================================================================*/
typedef enum {
  DIAG_READ_NICE_C  = 0, /* Read "Nice" values for LOG and MSG services */
  DIAG_WRITE_NICE_C = 1, /* Write "Nice" values for LOG and MSG services */
  DIAG_READ_PRI_C   = 2, /* Read "priority" values for LOG and MSG services */
  DIAG_WRITE_PRI_C  = 3, /* Write "priority" values for LOG and MSG services */
  DIAG_BUF_SIZE_C   = 4  /* Return size (in bytes) of output buffer */

} diag_streaming_config_subcommand_enum_type;

typedef PACK(struct) {
    uint16 code; /* MSG_LVL value or Log code */

    int16  val;

} diagpkt_streaming_config_entry_type;

typedef PACK(struct) {

  /* Number of entries in the following array */
  uint8 num_entries;

  /* # of log codes + 5 Message Levels is the number max $ in this array */
  diagpkt_streaming_config_entry_type entry[1];

} diagpkt_streaming_config_entry_list_type;


/*==========================================================================

PACKET   DIAG_TS_F

PURPOSE  Sent from the DM to the DMSS to request the IS-95/IS-2000 time.

============================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_TS_F)

  /* It's just the command code */

DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_TS_F)

  //qword ts;        /* Time Stamp */
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;
DIAGPKT_RSP_END


/*==========================================================================

PACKET   DIAG_SPC_F

PURPOSE  Request sent from the DM to the DMSS to enter the Service
         Programming Code (SPC), enabling service programming.  Response
         indicates whether or not the SPC was accepted as correct.

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_SPC_F )

  diagpkt_sec_code_type sec_code;

DIAGPKT_REQ_END


DIAGPKT_RSP_DEFINE( DIAG_SPC_F )

  boolean sec_code_ok;

DIAGPKT_RSP_END



/*==========================================================================

PACKET   DIAG_OUTP_F

PURPOSE  Request sent from the DM to the DMSS to send a byte to an
         IO port

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_OUTP_F )

  word port;                   /* number of port to output to */
  byte data;                   /* data to write to port */

DIAGPKT_REQ_END

DIAGPKT_DEFINE_RSP_AS_REQ( DIAG_OUTP_F )


/*==========================================================================

PACKET   DIAG_OUTPW_F

PURPOSE  Request sent from the DM to the DMSS to send a 16-bit word to an
         IO port

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_OUTPW_F )

  word port;                   /* number of port to output to */
  word data;                   /* data to write to port */

DIAGPKT_REQ_END

DIAGPKT_DEFINE_RSP_AS_REQ( DIAG_OUTPW_F )



/*==========================================================================

PACKET   DIAG_INP_F

PURPOSE  Request sent from the DM to the DMSS to read a byte to an
         IO port

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_INP_F )

  word port;                   /* number of port to output to */

DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE( DIAG_INP_F )

  word port;                   /* number of port to output to */
  byte data;                   /* data to write to port */

DIAGPKT_REQ_END


/*==========================================================================

PACKET   DIAG_INPW_F

PURPOSE  Request sent from the DM to the DMSS to read a 16-bit word from an
         IO port

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_INPW_F )

  word port;                   /* number of port to output to */

DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE( DIAG_INPW_F )

  word port;                   /* number of port to output to */
  word data;                   /* data to write to port */

DIAGPKT_REQ_END


/*===========================================================================

PACKET   diag_dipsw_req_type

ID       DIAG_DIPSW_F

PURPOSE  Sent by DM to retreive the current dip switch settings

RESPONSE DMSS performs the test, then responds.

===========================================================================*/
typedef PACK(struct)
{
  byte cmd_code;
  word switches;
} diag_dipsw_req_type;

typedef diag_dipsw_req_type diag_dipsw_rsp_type;


/*==========================================================================

PACKET   DIAG_LOG_ON_DEMAND_F

PURPOSE  Request sent from the user to register a function pointer and
         log_code with the diagnostic service for every log that needs
         logging on demand support.

============================================================================*/
DIAGPKT_REQ_DEFINE( DIAG_LOG_ON_DEMAND_F )

   uint16 log_code;             /* The log_code to be sent */

DIAGPKT_REQ_END


DIAGPKT_RSP_DEFINE( DIAG_LOG_ON_DEMAND_F )

  uint16 log_code;             /* The log_code sent */
  uint8  status;               /* status returned from the function pointer */

DIAGPKT_RSP_END

/* Diagnostic extensions */
/*
** The maximum number of properties and callback functions. These are not
** used to determine the size of any data structure; they are used merely
** to guard against infinite loops caused by corruption of the callback
** and properties tables.
*/
#define DIAG_MAX_NUM_PROPS 20
#define DIAG_MAX_NUM_FUNCS 20

typedef void (*diag_cb_func_type) (
                                   unsigned char  *data_ptr,
                                   unsigned short  data_len,
                                   unsigned char  *rsp_ptr,
                                   unsigned short *rsp_len_ptr
);

typedef struct
{
   char             *name;
   diag_cb_func_type address;
} diag_cb_struct_type;

typedef struct
{
   char *name;
   void *address;
} diag_prop_struct_type;

typedef enum
{
  DIAG_EXTN_INVALID_GUID = 1,
  DIAG_EXTN_INVALID_SIZE,
  DIAG_EXTN_INVALID_ADDRESS,
  DIAG_EXTN_INVALID_NAME,
  DIAG_EXTN_INVALID_DATA
} diag_extn_err_type;


/*===========================================================================

PACKET   DIAG_GET_PROPERTY_F

PURPOSE  Sent by the DM to retrieve a specified number of bytes from
         memory starting at a specified location.

===========================================================================*/
#define DIAG_MAX_PROPERTY_SIZE 800
typedef unsigned long * diag_guid_type[4];

DIAGPKT_REQ_DEFINE(DIAG_GET_PROPERTY_F)
  diag_guid_type guid;                     /* GUID for verification       */
  dword          address;                  /* Starting address in memory  */
  word           size;                     /* Number of bytes to retrieve */
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_GET_PROPERTY_F)
  diag_guid_type guid;                    /* GUID for verification       */
  dword          address;                 /* Starting address in memory  */
  word           size;                    /* Number of bytes to retrieve */
  byte           data[ DIAG_MAX_PROPERTY_SIZE ]; /* Byte values          */
DIAGPKT_RSP_END


/*===========================================================================

PACKET   DIAG_PUT_PROPERTY_F

PURPOSE  Sent by the DM to set the values of a specified number of bytes
         in memory starting at a specified location.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_PUT_PROPERTY_F)
  diag_guid_type guid;                   /* GUID for verification      */
  dword          address;                /* Starting address in memory */
  word           size;                   /* Number of bytes to set     */
  byte           data[ DIAG_MAX_PROPERTY_SIZE ]; /* Values             */
DIAGPKT_REQ_END

DIAGPKT_DEFINE_RSP_AS_REQ(DIAG_PUT_PROPERTY_F)

/*===========================================================================

PACKET   DIAG_GET_GUID_F

PURPOSE  Sent by the DM to retrieve the GUID (globally unique identifier)
         for the current build. This is stored during the build process.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_GET_GUID_F)
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_GET_GUID_F)
  diag_guid_type guid;               /* Globally unique identifier  */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_GET_PERM_PROPERTY_F

PURPOSE  Sent by the DM to retrieve the contents of a structure specified
         by name.

===========================================================================*/
#define DIAG_MAX_PROPERTY_NAME_SIZE 40

DIAGPKT_REQ_DEFINE(DIAG_GET_PERM_PROPERTY_F)
  diag_guid_type guid;                     /* GUID for verification       */
  byte           name[ DIAG_MAX_PROPERTY_NAME_SIZE ]; /* Structure name   */
  word           size;                     /* Number of bytes to retrieve */
DIAGPKT_REQ_END

DIAGPKT_RSP_DEFINE(DIAG_GET_PERM_PROPERTY_F)
  diag_guid_type guid;                     /* GUID for verification       */
  byte           name[ DIAG_MAX_PROPERTY_NAME_SIZE ]; /* Structure name   */
  word           size;                    /* Number of bytes to retrieve  */
  byte           data[ DIAG_MAX_PROPERTY_SIZE ]; /* Structure Contents    */
DIAGPKT_RSP_END

/*===========================================================================

PACKET   DIAG_PUT_PERM_PROPERTY_F

PURPOSE  Sent by the DM to fill in a structure specified by name.

===========================================================================*/
DIAGPKT_REQ_DEFINE(DIAG_PUT_PERM_PROPERTY_F)
  diag_guid_type guid;                   /* GUID for verification         */
  byte           name[ DIAG_MAX_PROPERTY_NAME_SIZE ]; /* Structure name   */
  word           size;                     /* Number of bytes of data     */
  byte           data[ DIAG_MAX_PROPERTY_SIZE ]; /* Values                */
DIAGPKT_REQ_END

DIAGPKT_DEFINE_RSP_AS_REQ(DIAG_PUT_PERM_PROPERTY_F)

/*--------------------------------------------------------------------------
  Command Codes between the Diagnostic Monitor and the mobile. These command
  codes are used for stress testing.
----------------------------------------------------------------------------*/

#define DIAGDIAG_START_STRESS_TEST_F           0x0000
#define DIAGDIAG_CMD_REQUEST_F                 0x0001
#define DIAGDIAG_ADD_EVENT_LISTENER_F          0x0002
#define DIAGDIAG_REMOVE_EVENT_LISTENER_F       0x0003
#define DIAGDIAG_ADD_LOG_LISTENER_F            0x0004
#define DIAGDIAG_REMOVE_LOG_LISTENER_F         0x0005
#define DIAGDIAG_START_STRESS_TEST_APPS_F      0x0006

#define DIAGDIAG_QSR4_FILE_OP_MODEM            0x0816
#define DIAGDIAG_QSR4_FILE_OP_APPS             0x020F
#define DIAGDIAG_QSR4_FILE_OP_WCNSS            0x141F
#define DIAGDIAG_QSR4_FILE_OP_ADSP             0x0E10
#define DIAGDIAG_QSR4_FILE_OP_SLPI             0x1A18

#define DIAGDIAG_FILE_LIST_OPERATION           0x00
#define DIAGDIAG_FILE_OPEN_OPERATION           0x01
#define DIAGDIAG_FILE_READ_OPERATION           0x02
#define DIAGDIAG_FILE_CLOSE_OPERATION          0x03

/*==========================================================================

PACKET   DIAGDIAG_STRESS_TEST

PURPOSE  Request sent from the DM to the DMSS to stress test events.

============================================================================*/
typedef enum {
  DIAGDIAG_STRESS_TEST_MSG = 0,
  DIAGDIAG_STRESS_TEST_MSG_1 = 1,
  DIAGDIAG_STRESS_TEST_MSG_2 = 2,
  DIAGDIAG_STRESS_TEST_MSG_3 = 3,
  DIAGDIAG_STRESS_TEST_MSG_4 = 4,
  DIAGDIAG_STRESS_TEST_MSG_5 = 5,
  DIAGDIAG_STRESS_TEST_MSG_6 = 6,
  DIAGDIAG_STRESS_TEST_MSG_STR = 7,
  DIAGDIAG_STRESS_TEST_MSG_PSEUDO_RANDOM = 8,
  DIAGDIAG_STRESS_TEST_MSG_LOW = 9,
  DIAGDIAG_STRESS_TEST_MSG_MED = 10,
  DIAGDIAG_STRESS_TEST_MSG_HIGH = 11,
  DIAGDIAG_STRESS_TEST_MSG_ERROR = 12,
  DIAGDIAG_STRESS_TEST_MSG_FATAL = 13,
  DIAGDIAG_STRESS_TEST_ERR = 14,
  DIAGDIAG_STRESS_TEST_ERR_FATAL = 15,
  DIAGDIAG_STRESS_TEST_LOG = 16,
  DIAGDIAG_STRESS_TEST_EVENT_NO_PAYLOAD = 17,
  DIAGDIAG_STRESS_TEST_EVENT_WITH_PAYLOAD = 18,
  DIAGDIAG_STRESS_TEST_ERR_FATAL_ISR = 19, /* test panic mode from ISR */
  DIAGDIAG_STRESS_TEST_CMD_REQ = 20,
  /* Reserved space up to 39 */
  DIAGDIAG_STRESS_TEST_LOG_64 = 40,
  DIAGDIAG_STRESS_TEST_LOG_128 = 41,
  DIAGDIAG_STRESS_TEST_LOG_256 = 42,
  /* Log of 512 is test type 16 */
  DIAGDIAG_STRESS_TEST_LOG_1K = 43,
  DIAGDIAG_STRESS_TEST_LOG_2K = 44,
  DIAGDIAG_STRESS_TEST_LOG_4K = 45,
  DIAGDIAG_STRESS_TEST_LOG_6K = 46,
  /* Test cases for QSR messages */
  DIAGDIAG_STRESS_TEST_QSR_MSG = 47,
  DIAGDIAG_STRESS_TEST_QSR_MSG_1 = 48,
  DIAGDIAG_STRESS_TEST_QSR_MSG_2 = 49,
  DIAGDIAG_STRESS_TEST_QSR_MSG_3 = 50,
  DIAGDIAG_STRESS_TEST_QSR_MSG_4 = 51,
  DIAGDIAG_STRESS_TEST_QSR_MSG_5 = 52,
  DIAGDIAG_STRESS_TEST_QSR_MSG_6 = 53,
  DIAGDIAG_STRESS_TEST_QSR_MSG_LOW = 54,
  DIAGDIAG_STRESS_TEST_QSR_MSG_MED = 55,
  DIAGDIAG_STRESS_TEST_QSR_MSG_HIGH = 56,
  DIAGDIAG_STRESS_TEST_QSR_MSG_ERROR = 57,
  DIAGDIAG_STRESS_TEST_QSR_MSG_FATAL = 58,
  DIAGDIAG_STRESS_TEST_MSG_SPRINTF_1 = 59,
  DIAGDIAG_STRESS_TEST_MSG_SPRINTF_2 = 60,
  DIAGDIAG_STRESS_TEST_MSG_SPRINTF_3 = 61,
  DIAGDIAG_STRESS_TEST_MSG_SPRINTF_4 = 62,
  DIAGDIAG_STRESS_TEST_MSG_SPRINTF_5 = 63,
} diag_stress_test_type_enum_type;

typedef enum {
  EXPLICIT_PRI = 0, /* Priority given by the user */
  RELATIVE_PRI = 1  /* relative priority is relative to DIAG_PRI */
} diag_stress_pri_type_enum_type;

typedef struct {
  uint8 test_type;  /* Decides if it is log, msg or event*/
  uint8 pri_type;   /* External or relative priority */
  int16 pri;        /* Priority at which the task is created */
} diag_task_priority_type;

typedef struct {
  diag_task_priority_type priority; /* refer to diag_task_priority_type above*/
  int num_iterations; /* the number of times the test_type should be called	*/
  int sleep_duration; /* Sleep time in milliseconds */
  int num_iterations_before_sleep; /*After NUM_ITERATIONS_BEFORE_SLEEP  iterations, sleep */
} diag_per_task_test_info;

typedef struct {
  diagpkt_subsys_header_type header; /* Sub System header */
  int num_tasks;                     /* Number of tasks, to be started */
  diag_per_task_test_info test[1];   /* Place holder for per task info */
} DIAGDIAG_STRESS_TEST_req_type;

typedef DIAGDIAG_STRESS_TEST_req_type DIAGDIAG_STRESS_TEST_rsp_type;

typedef struct {
  uint16 pri_type;   /* External or relative priority */
  int16  pri;        /* Priority at which the task is created */
} diag_priority_type;


/*==========================================================================

PACKET   DIAGDIAG_CMD_REQUEST

PURPOSE  Request sent from the DM to the DMSS to test command request.
============================================================================*/

typedef struct {
  diagpkt_subsys_header_type header; /* Sub System header */
  uint32 length;                     /* length of the packet */
  diag_priority_type priority;
  byte req[64];                      /* Packet sent */
} diagdiag_cmd_req_type;

typedef diagdiag_cmd_req_type diagdiag_cmd_rsp_type;


/*==========================================================================

PACKET   DIAGDIAG_LOG_EVENT_LISTENER

PURPOSE  Request sent from the DM to the DMSS to test log and event listeners.
		 This type is used for adding and removing log and event listeners.
============================================================================*/

typedef struct {
  diagpkt_subsys_header_type header; /* Sub System header */
  uint32 length;
  uint32 id;      /* log code or event id */
  uint32 param;   /* Will be printed by debug message */
} diag_log_event_listener_req_type;

typedef diag_log_event_listener_req_type diag_log_event_listener_rsp_type;

/*==========================================================================

PACKET   DIAG_EVENT_MASK_GET_F

PURPOSE  Request sent from the DM to the DMSS to retrieve the event mask.
============================================================================*/

typedef struct {
  diagpkt_header_type header;
  uint8 pad;
  uint16 reserved;
} event_mask_get_req_type;

typedef struct {
  diagpkt_header_type header;
  uint8 error_code;
  uint16 reserved;
  uint16 numbits;         /* number of bits in the mask           */
  unsigned char mask[1];  /* size of this field = (numbits + 7)/8 */
} event_mask_get_rsp_type;

/*==========================================================================

PACKET   DIAG_EVENT_MASK_SET_F

PURPOSE  Request sent from the DM to the DMSS to set the event mask.
============================================================================*/

typedef struct {
  diagpkt_header_type header;
  uint8 pad;
  uint16 reserved;
  uint16 numbits;         /* number of bits in the mask           */
  unsigned char mask[1];  /* size of this field = (numbits + 7)/8 */
} event_mask_set_req_type;

typedef struct {
  diagpkt_header_type header;
  uint8 error_code;
  uint16 reserved;
  uint16 numbits;         /* number of bits in the mask           */
  unsigned char mask[1];  /* size of this field = (numbits + 7)/8 */
} event_mask_set_rsp_type;

/* Error codes for the above two packets.
 */
#define EVENT_MASK_GENERAL_FAILURE 1
#define EVENT_MASK_ARRAY_TOO_SMALL 2
#define EVENT_MASK_ARRAY_TOO_BIG   3

/* Command request response structures to retrieve Qshrink4 database.
 */

/*==========================================================================

PACKET   DIAG_QSHRINK4_FILE_LIST_REQ

PURPOSE  Request sent to get the qsr4 file list information
============================================================================*/

typedef PACK(struct) {
	uint8 cmd_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint16 version;
	uint16 opcode;
} diag_qsr_header_req;

typedef PACK(struct) {
	uint8 cmd_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint32 delayed_rsp_status;
	uint16 delayed_rsp_id;
	uint16 rsp_cnt;
	uint16 version;
	uint16 opcode;
} diag_qsr_header_rsp;

typedef PACK(struct) {
	uint8 guid[16];
	uint32 file_len;
} file_info;

typedef PACK(struct) {
	diag_qsr_header_rsp rsp_header;
	uint8 status;
	uint8 num_files;
	file_info info[0];
} diag_qsr_file_list_rsp;

/*==========================================================================

PACKET   DIAG_QSHRINK4_FILE_OPEN_REQ

PURPOSE  Request sent to open the qsr4 database file
============================================================================*/
typedef PACK(struct) {
	diag_qsr_header_req req;
	uint8 guid[16];
} diag_qsr_file_open_req;

typedef PACK(struct) {
	diag_qsr_header_rsp rsp_header;
	uint8 guid[16];
	uint16 read_file_fd;
	uint8 status;
} diag_qsr_file_open_rsp;


/*==========================================================================

PACKET   DIAG_QSHRINK4_FILE_READ_REQ

PURPOSE  Request sent to read the qsr4 database file
============================================================================*/
typedef PACK(struct) {
	diag_qsr_header_req req;
	uint16 read_file_fd;
	uint32 req_bytes;
	uint32 offset;
}
diag_qsr_file_read_req;

typedef PACK(struct) {
	uint8 cmd_code;
	uint8 subsys_id;
	uint16 subsys_cmd_code;
	uint32 delayed_rsp_status;
	uint16 delayed_rsp_id;
	uint16 rsp_cnt;
	uint16 version;
	uint16 opcode;
	uint16 read_file_fd;
	uint32 offset;
	uint32 num_read;
	uint8 status;
	uint8 data[0];
}
diag_qsr_file_read_rsp;

/*==========================================================================

PACKET   DIAG_QSHRINK4_FILE_CLOSE_REQ

PURPOSE  Request sent to open the qsr4 database file
============================================================================*/
typedef PACK(struct) {
	diag_qsr_header_req req;
	uint16 read_file_fd;
} diag_qsr_file_close_req;

typedef PACK(struct) {
	diag_qsr_header_rsp rsp_header;
	uint16 read_file_fd;
	uint8 status;
} diag_qsr_file_close_rsp;

#endif /* DIAGDIAG_H */
