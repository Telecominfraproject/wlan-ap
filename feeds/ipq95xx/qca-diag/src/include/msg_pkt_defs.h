#ifndef MSG_PKT_DEFS_H
#define MSG_PKT_DEFS_H

/*!
  @ingroup diag_message_service
  @file msg_pkt_defs.h
  @brief
  Diagnostic Debug Message Service Packet Definitions and structures

  @par OVERVIEW:
			   The debug message service provides printf()-style debugging
			   with build-time and run-time granularity.  This granularity
			   is achieved by assigning various technology areas to a
			   subsystem within the service.  Each subsystem defines up to
			   32 categories (denoted by a 32-bit mask) by which debug
			   messages may be identified.  The external device may
			   configure each subsystem individually using a 32-bit mask
			   for each subsystem.


  @par TERMINOLOGY:
               Subsystem ID (SSID):
               Unique identifer given to each subsystem within the target.

               @par Subsystem Mask:
               32-bit value with each bit denotes a category assigned by
               the technology area assigned to the subsystem ID.
               '1' denotes the category is enabled.
               '0' denotes the category is disabled.
               This mask is specified with the definition of each message.

               @par Build Mask:
               A 32-bit mask specified at compile-time to determine which
               messages are to be compiled in for each subsystem.  This
               build-time granularity is available to enable build managers
               to compile out certain categories of messages to save ROM.
               During compilation, a bitwise AND ('&') is performed between
               the Build Mask and the Subsystem Mask of each message.  If
               the '&' operation is non-zero, the message is compiled in.

               @par Run-Time Mask (RT Mask):
               A 32-bit mask specified at by the external device at run-time
               to configure messages for a subsystem.  At run-time, the
               message's Subsystem Mask the subsystem's RT Mask are compared
               with a bitwise AND ('&').  If the result if non-zero, the
               message service attempts to send the message.

               @par Dropped Message:
               If insuffient resources exist to send a message that is
               enabled, the message is dropped.  The next successful message
               sent will contain a count of the number of messages dropped
               since the last successful message.


          @note
		  These packet definitions are part of an externalized
          diagnostic command interface, defined in 80-V1294-1
		  (CDMA Dual-Mode Subscriber Station Serial Data Interface Control
		  Document). These definitions must not be changed.
*/
/*
Copyright (c) 2002-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
*/

/*===========================================================================

                            Edit History

  $Header: //source/qcom/qct/core/api/services/diag/main/latest/msg_pkt_defs.h#2 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
05/27/10   mad     Doxygenated.
01/23/09   sg      Moved msg_desc_type, msg_hdr_type here
05/15/09   mad     Moved msg_desc_type, msg_hdr_type to msg.h. Included msg.h.
10/03/08   vg      Updated code to use PACK() vs. PACKED
04/23/07   as      Enabled pragma pack support for WINCE targets
11/04/04   as      Added GW field for WCDMA phones in msg_ts_type
07/23/02   lad     Updated to reflect requirements changes and final
                   implementation.
03/22/02   igt     Created file.
===========================================================================*/
#include "comdef.h"

/* --------------------------------------------------------------------------
   Definitions and Declarations
   ----------------------------------------------------------------------- */
/*!
@ingroup diag_message_service
  This structure is stored in ROM and is copied blindly by the phone.
  The values for the fields of this structure are known at compile time.
  So this is to be defined as a "static const" in the MACRO, so it ends up
  being defined and initialized at compile time for each and every message
  in the software. This minimizes the amount of work to do during run time.

  So this structure is to be used in the "caller's" context. "Caller" is the
  client of the Message Services.
*/
typedef struct {
	uint16 line;			/*!< Line number in source file */
	uint16 ss_id;			/*!< Subsystem ID               */
	uint32 ss_mask;			/*!< Subsystem Mask             */
} msg_desc_type;

/*!
@ingroup diag_message_service
  This is the message HEADER type. It contains the beginning fields of the
  packet and is of fixed length.  These fields are filled by the calling task.
*/
typedef struct {
	uint8 cmd_code;		/*!< Command code */
	uint8 ts_type;		/*!< Time stamp type */
	uint8 num_args;		/*!< Number of arguments in message */
	uint8 drop_cnt;		/*!< number of messages dropped since last successful message */
	uint32 ts_lo; /* Time stamp */
	uint32 ts_hi;
} msg_hdr_type;

/*!
@ingroup diag_message_service
This structure defines the Debug message packet :command-code 125, DIAG_MSG_EXT_F.
Provides debug messages with NULL terminated filename and format fields, a
variable number of 32-bit parameters, an originator-specific timestamp
format and filterability via subsystem ids and masks.
@par
For simplicity, only 'long' arguments are supported.  This packet
allows for N arguments, though the macros support a finite number.
@note
This is the structure that is used to represent the final structure that is
sent to the external device.  'msg_ext_store_type' is expanded to this
structure in DIAG task context at the time it is sent to the communication
layer.
*/
typedef struct	{
	msg_hdr_type hdr;	/*!< Header */
	msg_desc_type desc;	/*!< line number, SSID, mask */
	uint32 args[1];		/*!< Array of long args, specified by 'hdr.num_args' */
				/*!< followed by NULL terminated format and file strings */
} msg_ext_type;

/*---------------------------------------------------------------------------
  This is the structure that is used to represent the final structure that
  is sent to the external device.  'msg_ext_store_type' is expanded to this
  structure in DIAG task context at the time it is sent to the comm layer.
---------------------------------------------------------------------------*/
typedef struct
{
	msg_hdr_type hdr;
	msg_desc_type desc;
	uint32 msg_hash;
	uint32 args[1];
} msg_qsr_type;

#endif		/* MSG_PKT_DEFS_H */
