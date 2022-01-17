#ifndef MSG_H
#define MSG_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
         
                EXTENDED DIAGNOSTIC MESSAGE SERVICE HEADER FILE

GENERAL DESCRIPTION

  All the declarations and definitions necessary to support the reporting 
  of messages for errors and debugging.  This includes support for the 
  extended capabilities as well as the legacy messaging scheme.

# Copyright (c) 2007-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

$Header: //depot/asic/msmshared/services/diag/Diag_1.5/Diag_LSM/msg.h#1 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
01/02/08   mad     Enabled real diag F3 messages for FEATURE_WINCE, 
                   instead of retail messages.
09/01/04   eav     Code reorganization
07/08/04   eav     Moved F3 trace saving typedefs to diagdebug.h
06/02/04   eav     Added "debug_mask".  Determines whether or not to save
                   the F3 trace to the RAM buffer.
04/21/03   as      Added MSG_SPRINTF macros for 4,5,6,7 &8 arguments.
03/12/04   eav     Added msg_copy_to_efs_check prototype to be called from
                   tmc.c. 
02/20/04   eav     Added savetime and savevar flags to msg_const_type struct. 
09/23/03   as      defined (TARGET_UNIX) to support the DSPE - a Linux based 
                   phone emulator used by data team in testing and debugging.
07/24/02   lad     Updated to reflect requirements changes and final 
                   implementation of extended message service.
02/06/02   igt     Created module.

===========================================================================*/

/*===========================================================================

                     INCLUDE FILES FOR MODULE

===========================================================================*/

#ifdef FEATURE_L4_KERNEL

extern int printf (const char * format, ...);
#define MSG_HIGH(str, a, b, c) printf("HIGH: " str, a, b, c)
#define MSG_MED(str, a, b, c) printf("HIGH: " str, a, b, c)
#define MSG_LOW(str, a, b, c) printf("HIGH: " str, a, b, c)

#else

#include <stdint.h>
#include "msg_pkt_defs.h"          /* Packet definitions */
/*===========================================================================
            LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE
===========================================================================*/

/*---------------------------------------------------------------------------
  These are the masks that are used to identify a message as belonging to
  group "i" of a particular Subsystem. This allows the user to selectively
  turn groups of messages within a Subsystem to ON or OFF. The legacy
  messages will continue to be supported thru the legacy masks.
---------------------------------------------------------------------------*/
#define MSG_MASK_0   (0x00000001)
#define MSG_MASK_1   (0x00000002)
#define MSG_MASK_2   (0x00000004)
#define MSG_MASK_3   (0x00000008)
#define MSG_MASK_4   (0x00000010)
#define MSG_MASK_5   (0x00000020)
#define MSG_MASK_6   (0x00000040)
#define MSG_MASK_7   (0x00000080)
#define MSG_MASK_8   (0x00000100)
#define MSG_MASK_9   (0x00000200)
#define MSG_MASK_10  (0x00000400)
#define MSG_MASK_11  (0x00000800)
#define MSG_MASK_12  (0x00001000)
#define MSG_MASK_13  (0x00002000)
#define MSG_MASK_14  (0x00004000)
#define MSG_MASK_15  (0x00008000)
#define MSG_MASK_16  (0x00010000)
#define MSG_MASK_17  (0x00020000)
#define MSG_MASK_18  (0x00040000)
#define MSG_MASK_19  (0x00080000)
#define MSG_MASK_20  (0x00100000)
#define MSG_MASK_21  (0x00200000)
#define MSG_MASK_22  (0x00400000)
#define MSG_MASK_23  (0x00800000)
#define MSG_MASK_24  (0x01000000)
#define MSG_MASK_25  (0x02000000)
#define MSG_MASK_26  (0x04000000)
#define MSG_MASK_27  (0x08000000)
#define MSG_MASK_28  (0x10000000)
#define MSG_MASK_29  (0x20000000)
#define MSG_MASK_30  (0x40000000)
#define MSG_MASK_31  (0x80000000)

/*---------------------------------------------------------------------------
  These masks are to be used for support of all legacy messages in the sw.
  The user does not need to remember the names as they will be embedded in 
  the appropriate macros.
---------------------------------------------------------------------------*/
#define MSG_LEGACY_LOW      MSG_MASK_0
#define MSG_LEGACY_MED      MSG_MASK_1
#define MSG_LEGACY_HIGH     MSG_MASK_2
#define MSG_LEGACY_ERROR    MSG_MASK_3
#define MSG_LEGACY_FATAL    MSG_MASK_4

/*---------------------------------------------------------------------------
  Legacy Message Priorities 
---------------------------------------------------------------------------*/
#define MSG_LVL_FATAL   (MSG_LEGACY_FATAL)
#define MSG_LVL_ERROR   (MSG_LEGACY_ERROR | MSG_LVL_FATAL)
#define MSG_LVL_HIGH    (MSG_LEGACY_HIGH | MSG_LVL_ERROR)
#define MSG_LVL_MED     (MSG_LEGACY_MED | MSG_LVL_HIGH)
#define MSG_LVL_LOW     (MSG_LEGACY_LOW | MSG_LVL_MED)

#define MSG_LVL_NONE    0

#include "msgcfg.h" /* Header file listing SSIDs, build masks, etc. */
#include "msg_qsr.h"  /* Optimized F3 Debug messages */

/* If MSG_FILE is defined, use that as the filename, and allocate a single
   character string to contain it.  Note that this string is shared with the
   Error Services, to conserve ROM and RAM.

   With ADS1.1 and later, multiple uses of __FILE__ or __MODULE__ within the
   same file do not cause multiple literal strings to be stored in ROM. So in
   builds that use the more recent versions of ADS, it is not necessary to
   define the static variable msg_file. Note that __MODULE__ gets replaced
   with the filename portion of the full pathname of the file. */
/* Note, if this symbol is activated, the file is being compiled with legacy
   message service defines.  Older compilers may need the static const
   variable to avoid multiple copies of the filename string in ROM. */

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 110000)
/* Check for ARM version first.  Defining static const char msg_file[] will
   generate a compiler warning if the file doesn't actually use a message. */

#define msg_file __MODULE__

#elif defined (MSG_FILE)

static const char msg_file[] = MSG_FILE;
// Added for Linux
#elif defined (TARGET_UNIX) || defined (__GNUC__)
#define msg_file __FILE__

#else

static const char msg_file[] = __FILE__;

#endif

/*---------------------------------------------------------------------------
  The extended message packet is defined to be processed as efficiently as
  possible in the caller's context.  Therefore, the packet is divided into
  logical blocks that are aligned w/out declaring the structure as PACKED.
  
  A header, static constant block, and argument list are defined to minimize
  the work done by the caller's task.
   
   1. Header
   2. Constant variable length data (format string and filename string, etc).
   3. Arguments ("Variable" variable-length data)
   
  The data is delivered in this order in the packet to simplify the runtime 
  processing of each message.  All constant data is handled in DIAG task 
  context, leaving the caller's task to only process variable data at runtime.
   
  The phone will never process the constant data directly, except to copy 
  format and filename strings. 
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  All constant information stored for a message.
   
  The values for the fields of this structure are known at compile time. 
  So this is to be defined as a "static " in the MACRO, so it ends up
  being defined and initialized at compile time for each and every message 
  in the software. This minimizes the amount of work to do during run time.
  
  So this structure is to be used in the "caller's" context. "Caller" is the
  client of the Message Services.
---------------------------------------------------------------------------*/
typedef struct
{
  msg_desc_type desc;       /* ss_mask, line, ss_id */
  const char *fmt;      /* Printf style format string */
  const char *fname;        /* Pointer to source file name */

#if defined (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE)
  boolean do_save;              /* If TRUE, save msg to RAM buffer */
#endif
}
msg_const_type;

/*---------------------------------------------------------------------------
  This is the structure that is stored by the caller's task in msg_send ().
  The DIAG task will expand the constant data into the final packet before
  sending to the external device.
---------------------------------------------------------------------------*/
typedef struct
{
  msg_hdr_type hdr;
  const msg_const_type *const_data_ptr; /* desc, file_name, fmt */
  uint32 args[1];
}
msg_ext_store_type;


#if defined (FEATURE_MSG_IFACE_VIOLATION)
/*===========================================================================
    Message Store Type.
===========================================================================*/
  typedef struct
  {
    byte level;         /* Severity level / Priority of this message. */
    word line;          /* Line number into source file */
    const char *file_ptr;   /* Pointer to source file name */
    const char *xx_fmt;     /* Printf style format string */
  } 
  msg_store_type;

  /* BREW still references this old-style message format */
  void msg_put (const msg_store_type * fmt_ptr, dword code1, dword code2,
    dword code3);
#endif              /* FEATURE_MSG_IFACE_VIOLATION */

/* Enabling real diag F3 messages, Diag 1.5 */

/*---------------------------------------------------------------------------
  The purpose of this macro is to define the constant part of the message
  that can be initialized at compile time and stored in ROM. This will 
  define and initialize a msg_const_type for each call of a message macro.
  The "static" limits the scope to the file the macro is called from while
  using the macro in a do{}while() guarantees the uniqueness of the name.
---------------------------------------------------------------------------*/
#if defined (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE)
  #define XX_MSG_CONST(xx_ss_id, xx_ss_mask, xx_fmt) \
              XX_MSG_CONST_SAVE(xx_ss_id, xx_ss_mask, xx_fmt, TRUE)

  #define XX_MSG_CONST_SAVE(xx_ss_id, xx_ss_mask, xx_fmt, do_save) \
    static const msg_const_type xx_msg_const = { \
      {__LINE__, (xx_ss_id), (xx_ss_mask)}, (xx_fmt), msg_file, do_save}

#else
  #define XX_MSG_CONST(xx_ss_id, xx_ss_mask, xx_fmt) \
    static const msg_const_type xx_msg_const = { \
      {__LINE__, (xx_ss_id), (xx_ss_mask)}, (xx_fmt), msg_file}

#endif /* (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE) */


/*---------------------------------------------------------------------------
  This Macro is used when format string xx_fmt is passed as a variable.at
  runtime instead of a literal.
---------------------------------------------------------------------------*/
#define XX_MSG_CONST_FMT_VAR(xx_ss_id, xx_ss_mask, xx_fmt) \
    const msg_const_type xx_msg_const = { \
      {__LINE__, (xx_ss_id), (xx_ss_mask)}, (xx_fmt), msg_file}

/*---------------------------------------------------------------------------
  These are the message macros that support messages with  variable number
  of parameters and message text of over 40 characters.
  This is the macro for messages with no params but only a text string.
---------------------------------------------------------------------------*/
#define MSG(xx_ss_id, xx_ss_mask, xx_fmt) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send (&xx_msg_const); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

#define MSG_TS(xx_ss_id, xx_ss_mask, xx_fmt, xx_ts) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST(xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_ts(&xx_msg_const, xx_ts); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \

/*---------------------------------------------------------------------------
  Macro for messages with 1 parameter.
---------------------------------------------------------------------------*/
#define MSG_1(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_1 (&xx_msg_const, (uint32) (xx_arg1)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  Macro for messages with 2 parameters.
---------------------------------------------------------------------------*/
#define MSG_2(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_2 (&xx_msg_const, (uint32)(xx_arg1), (uint32)(xx_arg2)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 3 parameters.
---------------------------------------------------------------------------*/
#define MSG_3(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_3 (&xx_msg_const, (uint32) (xx_arg1), (uint32) (xx_arg2), \
                                 (uint32) (xx_arg3)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

#define MSG_3_SAVE(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, do_sav) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST_SAVE (xx_ss_id, xx_ss_mask, xx_fmt, do_sav); \
      /*lint -e571 */ \
      msg_send_3 (&xx_msg_const, (uint32) (xx_arg1), (uint32) (xx_arg2), \
                                 (uint32) (xx_arg3)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 4 parameters. In this case the function
  called needs to have more than 4 parameters so it is going to be a slow 
  function call.  So for this case the  msg_send_var() uses var arg list 
  supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_4(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var (&xx_msg_const, (uint32)(4), (uintptr_t)(xx_arg1), \
               (uintptr_t) (xx_arg2), (uintptr_t) (xx_arg3), \
               (uintptr_t) (xx_arg4)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 5 parameters. msg_send_var() uses var 
  arg list supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_5(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4, xx_arg5) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var(&xx_msg_const, (uint32)(5), (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2), (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4), (uintptr_t)(xx_arg5)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 6 parameters. msg_send_var() uses var 
  arg list supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_6(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4, xx_arg5, xx_arg6) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var (&xx_msg_const, (uint32)(6), (uintptr_t)(xx_arg1), \
                (uintptr_t)(xx_arg2), (uintptr_t)(xx_arg3), \
                (uintptr_t)(xx_arg4), (uintptr_t)(xx_arg5), \
                (uintptr_t)(xx_arg6)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 7 parameters. msg_send_var() uses var 
  arg list supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_7(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4, xx_arg5, xx_arg6, \
                                            xx_arg7) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var (&xx_msg_const, (uint32)(7), (uintptr_t)(xx_arg1), \
                (uintptr_t)(xx_arg2), (uintptr_t)(xx_arg3), \
                (uintptr_t)(xx_arg4), (uintptr_t)(xx_arg5), \
                (uintptr_t)(xx_arg6), (uintptr_t)(xx_arg7)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0)                                                             \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 8 parameters. msg_send_var() uses var 
  arg list supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_8(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4, xx_arg5, xx_arg6, \
                                            xx_arg7, xx_arg8) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var (&xx_msg_const, (uint32)(8), (uintptr_t)(xx_arg1), \
                (uintptr_t)(xx_arg2), (uintptr_t)(xx_arg3), \
                (uintptr_t)(xx_arg4), (uintptr_t)(xx_arg5), \
                (uintptr_t)(xx_arg6), (uintptr_t)(xx_arg7), \
                (uintptr_t)(xx_arg8)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for messages with 9 parameters. msg_send_var() uses var 
  arg list supported by the compiler.
---------------------------------------------------------------------------*/
#define MSG_9(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                            xx_arg4, xx_arg5, xx_arg6, \
                                            xx_arg7, xx_arg8, xx_arg9) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_send_var (&xx_msg_const, (uint32)(9), (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3), \
	       (uintptr_t)(xx_arg4), (uintptr_t)(xx_arg5), \
	       (uintptr_t)(xx_arg6), (uintptr_t)(xx_arg7), \
	       (uintptr_t)(xx_arg8), (uintptr_t)(xx_arg9)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 1 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_1(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 2 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_2(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const, (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 3 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_3(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 4 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_4(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                                    xx_arg4 ); \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2), (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 5 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_5(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                                    xx_arg4, xx_arg5 ); \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4),  (uintptr_t)(xx_arg5)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 6 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_6(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                                    xx_arg4, xx_arg5, xx_arg6 ); \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4),  (uintptr_t)(xx_arg5), \
               (uintptr_t)(xx_arg6)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 7 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_7(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                                    xx_arg4, xx_arg5, xx_arg6, \
                                                    xx_arg7 ); \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4),  (uintptr_t)(xx_arg5), \
               (uintptr_t)(xx_arg6),  (uintptr_t)(xx_arg7)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 8 parameters. msg_sprintf() 
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_8(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3, \
                                                    xx_arg4, xx_arg5, xx_arg6, \
                                                    xx_arg7, xx_arg8 ); \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const,  (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3), \
               (uintptr_t)(xx_arg4),  (uintptr_t)(xx_arg5), \
               (uintptr_t)(xx_arg6),  (uintptr_t)(xx_arg7), \
               (uintptr_t)(xx_arg8)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */


/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 3 parameters. This Macro is 
  used when xx_fmt is passed at runtime.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_FMT_VAR_3(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1, xx_arg2, xx_arg3) \
  do { \
    /*lint -e506 -e774*/ \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
    /*lint +e506 +e774*/ \
      XX_MSG_CONST_FMT_VAR (xx_ss_id, xx_ss_mask, xx_fmt); \
      /*lint -e571 */ \
      msg_sprintf (&xx_msg_const, (uintptr_t)(xx_arg1), \
               (uintptr_t)(xx_arg2),  (uintptr_t)(xx_arg3)); \
      /*lint +e571 */ \
    } \
  /*lint -e717 */ \
  } while (0) \
                       /* lint +e717 */

/*---------------------------------------------------------------------------
  The following MACROs are for LEGACY diagnostic messages support.  
---------------------------------------------------------------------------*/

/*===========================================================================

MACRO MSG_FATAL, MSG_ERROR, MSG_HIGH, MSG_MED, MSG_LOW

DESCRIPTION
  Output a message to be sent to be picked up by the Diag Task.  The
  message levels are controlled by selecting the proper macro function.

    MSG_FATAL   fatal
    MSG_ERROR   error
    MSG_HIGH    high
    MSG_MED     medium
    MSG_LOW     low

  Generation of message generating code is controlled by the setting
  of the MSG_LEVEL #define (defined above and on the 'cl' command line).

PARAMETERS
  x_fmt Format string for message (printf style).  Note, this parameter
        must be a string literal (e.g. "Tuned to %lx"), not a variable.
        This is because the value is compiled into ROM, and can clearly
        not be a dynamic data type.
  a     1st parameter for format string
  b     2nd parameter for format string
  c     3rd parameter for format string

DEPENDENCIES
  msg_init() must be called prior to this macro referencing msg_put().
  This macro, is multiple C expressions, and cannot be used as if it
  is a single expression.

RETURN VALUE
  None

SIDE EFFECTS
  On the target hardware, the file and format strings are placed in ROM.

===========================================================================*/
#if defined (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE)

#define MSG_FATAL(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_FATAL, x_fmt, a, b, c, TRUE)

#define MSG_ERROR(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_ERROR, x_fmt, a, b, c, TRUE)

#define MSG_HIGH(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_HIGH, x_fmt, a, b, c, TRUE)

#define MSG_MED(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_MED, x_fmt, a, b, c, TRUE)

#define MSG_LOW(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_LOW, x_fmt, a, b, c, TRUE)

#else /* if not FEATURE_SAVE_DEBUG_TRACE */

#define MSG_FATAL(x_fmt, a, b, c) \
    MSG_3 (MSG_SSID_DFLT, MSG_LEGACY_FATAL, x_fmt, a, b, c)

#define MSG_ERROR(x_fmt, a, b, c) \
    MSG_3 (MSG_SSID_DFLT, MSG_LEGACY_ERROR, x_fmt, a, b, c)

#define MSG_HIGH(x_fmt, a, b, c) \
    MSG_3 (MSG_SSID_DFLT, MSG_LEGACY_HIGH, x_fmt, a, b, c)

#define MSG_MED(x_fmt, a, b, c) \
    MSG_3 (MSG_SSID_DFLT, MSG_LEGACY_MED, x_fmt, a, b, c)

#define MSG_LOW(x_fmt, a, b, c) \
    MSG_3 (MSG_SSID_DFLT, MSG_LEGACY_LOW, x_fmt, a, b, c)

#endif /* FEATURE_SAVE_DEBUG_TRACE */

#if defined (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE)
/*===========================================================================

MACRO MSG_FATAL_NO_SAVE, MSG_ERROR_NO_SAVE, MSG_HIGH_NO_SAVE, MSG_MED_NO_SAVE, MSG_LOW_NO_SAVE

DESCRIPTION
  Output a message to be sent to be picked up by the Diag Task.  The
  message levels are controlled by selecting the proper macro function.

    MSG_FATAL_NO_SAVE   fatal
    MSG_ERROR_NO_SAVE   error
    MSG_HIGH_NO_SAVE    high
    MSG_MED_NO_SAVE     medium
    MSG_LOW_NO_SAVE     low

  Generation of message generating code is controlled by the setting
  of the MSG_LEVEL #define (defined above and on the 'cl' command line).

  The use of these macros will cause the const type to not be saved to the
  RAM buffer.

PARAMETERS
  x_fmt Format string for message (printf style).  Note, this parameter
        must be a string literal (e.g. "Tuned to %lx"), not a variable.
        This is because the value is compiled into ROM, and can clearly
        not be a dynamic data type.
  a     1st parameter for format string
  b     2nd parameter for format string
  c     3rd parameter for format string

DEPENDENCIES
  msg_init() must be called prior to this macro referencing msg_put().
  This macro, is multiple C expressions, and cannot be used as if it
  is a single expression.

RETURN VALUE
  None

SIDE EFFECTS
  On the target hardware, the file and format strings are placed in ROM.

===========================================================================*/

#define MSG_FATAL_NO_SAVE(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_FATAL, x_fmt, a, b, c, FALSE)

#define MSG_ERROR_NO_SAVE(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_ERROR, x_fmt, a, b, c, FALSE)

#define MSG_HIGH_NO_SAVE(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_HIGH, x_fmt, a, b, c, FALSE)

#define MSG_MED_NO_SAVE(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_MED, x_fmt, a, b, c, FALSE)

#define MSG_LOW_NO_SAVE(x_fmt, a, b, c) \
    MSG_3_SAVE (MSG_SSID_DFLT, MSG_LEGACY_LOW, x_fmt, a, b, c, FALSE)

#else /* Not FEATURE_SAVE_DEBUG_TRACE */
#define MSG_FATAL_NO_SAVE(x_fmt, a, b, c) MSG_FATAL (x_fmt, a, b, c)

#define MSG_ERROR_NO_SAVE(x_fmt, a, b, c) MSG_ERROR (x_fmt, a, b, c)

#define MSG_HIGH_NO_SAVE(x_fmt, a, b, c)  MSG_HIGH (x_fmt, a, b, c)

#define MSG_MED_NO_SAVE(x_fmt, a, b, c)   MSG_MED (x_fmt, a, b, c)

#define MSG_LOW_NO_SAVE(x_fmt, a, b, c)   MSG_LOW (x_fmt, a, b, c)
#endif /* FEATURE_SAVE_DEBUG_TRACE */

/* Legacy stop-gap solutions to the need for string transport in MSG services. */
#define MSG_STR_ERROR(x_fmt, s, a, b, c)
#define MSG_STR_HIGH(x_fmt, s, a, b, c)
#define MSG_STR_MED(x_fmt, s, a, b, c)
#define MSG_STR_LOW(x_fmt, s, a, b, c)

/* These are unnecessary except for the fact that some of the legacy messages 
   out there have been compiled out for so long that they have args that are 
   invalid.  The above will cause the compiler to reference those args, even
   though they are optimized away.  Therefore, we must do the following until
   all such cases are fixed. */
        
#if !(MSG_BUILD_MASK_MSG_SSID_DFLT & MSG_LEGACY_FATAL)

  #undef MSG_FATAL
  #define MSG_FATAL(x, a, b, c)

#endif

#if !(MSG_BUILD_MASK_MSG_SSID_DFLT & MSG_LEGACY_ERROR)

  #undef MSG_ERROR
  #define MSG_ERROR(x, a, b, c)

#endif

#if !(MSG_BUILD_MASK_MSG_SSID_DFLT & MSG_LEGACY_HIGH)

  #undef MSG_HIGH
  #define MSG_HIGH(x, a, b, c)

#endif

#if !(MSG_BUILD_MASK_MSG_SSID_DFLT & MSG_LEGACY_MED)

  #undef MSG_MED
  #define MSG_MED(x, a, b, c)

#endif

#if !(MSG_BUILD_MASK_MSG_SSID_DFLT & MSG_LEGACY_LOW)

  #undef MSG_LOW
  #define MSG_LOW(x, a, b, c)

#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* -------------------------------------------------------------------------
   Function Definitions
   ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C"
{
#endif

/*===========================================================================

FUNCTION MSG_INIT

DESCRIPTION
  Initialize the message reporting services

DEPENDENCIES
  Can only be called by one task.

RETURN VALUE
  None

SIDE EFFECTS
  Sets the counts to 0, and throws away any buffered messages.

===========================================================================*/
    void msg_init (void);

/*===========================================================================

FUNCTION MSG_SEND

DESCRIPTION
  This will build a new style diagnostic Message with no parameters. 
  Do not call directly; use macro MSG_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_send (const msg_const_type * xx_msg_const_ptr);

/*===========================================================================

FUNCTION MSG_SEND_1

DESCRIPTION
  This will build a new style diagnostic Message with 1 parameters. 
  Do not call directly; use macro MSG_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_send_1 (const msg_const_type * xx_msg_const_ptr, uint32 xx_arg1);

/*===========================================================================

FUNCTION MSG_SEND_2

DESCRIPTION
  This will build a new style diagnostic Message with 2 parameters. 
  Do not call directly; use macro MSG_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_send_2 (const msg_const_type * xx_msg_const_ptr, uint32 xx_arg1,
    uint32 xx_arg2);

/*===========================================================================

FUNCTION MSG_SEND_3

DESCRIPTION
  This will build a new style diagnostic Message with 3 parameters. 
  Do not call directly; use macro MSG_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_send_3 (const msg_const_type * xx_msg_const_ptr, uint32 xx_arg1,
    uint32 xx_arg2, uint32 xx_arg3);

/*===========================================================================

FUNCTION MSG_SEND_VAR

DESCRIPTION
  This will build a new style diagnostic Message with var # (4 to 6) 
  of parameters. 
  Do not call directly; use macro MSG_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_send_var (const msg_const_type * xx_msg_const_ptr, uint32 num_args,
    ...);

/*===========================================================================

FUNCTION MSG_SPRINTF

DESCRIPTION
  This will build a message sprintf diagnostic Message with var # (1 to 6) 
  of parameters.
  Do not call directly; use macro MSG_SPRINTF_* defined in msg.h 
  
  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_sprintf(const msg_const_type * const_blk,...);

/*===========================================================================

FUNCTION MSG_SEND_TS

DESCRIPTION
  This will build a message with no parameter, and allow user to pass in a
  timestamp. Limited usage: only for Native Debug messages like WM RETAILMSG etc
  Do not call directly, use MSG_TS macro

DEPENDENCIES
  msg_init() and Diag_LSM_Init() must have been called previously.
  A free buffer must be available or the message will be ignored
  (never buffered).

===========================================================================*/
void msg_send_ts(const msg_const_type *const_blk, uint64 timestamp);


#if defined (FEATURE_ERR_EXTENDED_STORE) && defined (FEATURE_SAVE_DEBUG_TRACE)
/*===========================================================================

FUNCTION MSG_SAVE_3
 
DESCRIPTION
  Saves the F3 message to the RAM buffer.  Also called from the ERR_FATAL
  macro.

DEPENDENCIES
  None

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void msg_save_3(const msg_const_type*,
                uint32, uint32, uint32, msg_ext_store_type*);
/*===========================================================================

FUNCTION MSG_GET_NUM_ARGS

DESCRIPTION
  Determines the number of arguments in a given constant data pointer.  It
  scans the fmt field and determines the number of %'s.  The printf conversion
  of "%%" is not meant to be an argument.  Therefore, must do a check so it
  doesn't increase the count of arguments.

DEPENDENCIES
  None

RETURN VALUE
  Number of arguments required in a format string

SIDE EFFECTS
  None

===========================================================================*/
uint8 msg_get_num_args(const msg_const_type * const_data_ptr);
#endif

#ifdef __cplusplus
}
#endif
#endif /* FEATURE_L4_KERNEL */
#endif              /* MSG_H */
