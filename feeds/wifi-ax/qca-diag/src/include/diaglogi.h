#ifndef DIAGLOGI_H
#define DIAGLOGI_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                          Logging Services internal header file

General Description
  Internal declarations to support data logging.

Initializing and Sequencing Requirements 
  'log_init()' must be called once during initialization prior to use.

# Copyright (c) 2007-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
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


#include "log_codes.h"
#include "./../include/log.h"
// COMMENTED OUT FOR LINUX
//#include "diagi.h"
/* -------------------------------------------------------------------------
 * Definitions and Declarations
 * ------------------------------------------------------------------------- */
typedef PACK(struct)
{
  uint16 len;  /* Specifies the length, in bytes of 
                 the entry, including this header. */

  uint16 code; /* Specifies the log code for the 
                  entry as enumerated above.       
                  Note: This is specified as word 
                  to guarantee size.               */
// removed AMSS specific code
  //qword ts;    // The system timestamp for the log entry. The upper 48 bits
                 // represent elapsed time since 6 Jan 1980 00:00:00 
                 // in 1.25 ms units. The low order 16 bits represent elapsed 
                 // time since the last 1.25 ms tick in 1/32 chip units 
                 // (this 16 bit counter wraps at the value 49152).          
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;

} log_header_type;

#ifndef EQUIP_ID_MAX
   #define EQUIP_ID_MAX 16
#endif

typedef enum
{
  LOG_INIT_STATE = 0,
  LOG_NORMAL_STATE,
  LOG_FLUSH_STATE, /* Pending flush operation. */
  LOG_PANIC_STATE /* Panic mode flush in progress */

} log_state_enum_type;

//static log_state_enum_type log_state = LOG_INIT_STATE;
static void *log_commit_last = NULL; /* Many writers, 1 reader (DIAG) */
//static void *log_flush_last = NULL; /* 1 writer, 1 reader (both DIAG) */


#define LOG_DIAGPKT_OFFSET FPOS(diag_log_rsp_type, log)

/* -------------------------------------------------------------------------
 * Definitions for last log code per equipment ID.
 * If it is undefined, it is defined to 0.  digatune.h need only to 
 * contain values for included equipment IDs.
 * ------------------------------------------------------------------------- */
#if !defined (LOG_EQUIP_ID_0_LAST_CODE)
#define LOG_EQUIP_ID_0_LAST_CODE  LOG_EQUIP_ID_0_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_1_LAST_CODE)
#define LOG_EQUIP_ID_1_LAST_CODE LOG_EQUIP_ID_1_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_2_LAST_CODE)
#define LOG_EQUIP_ID_2_LAST_CODE LOG_EQUIP_ID_2_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_3_LAST_CODE)
#define LOG_EQUIP_ID_3_LAST_CODE LOG_EQUIP_ID_3_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_4_LAST_CODE)
#define LOG_EQUIP_ID_4_LAST_CODE LOG_EQUIP_ID_4_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_5_LAST_CODE)
#define LOG_EQUIP_ID_5_LAST_CODE LOG_EQUIP_ID_5_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_6_LAST_CODE)
#define LOG_EQUIP_ID_6_LAST_CODE LOG_EQUIP_ID_6_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_7_LAST_CODE)
#define LOG_EQUIP_ID_7_LAST_CODE LOG_EQUIP_ID_7_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_8_LAST_CODE)
#define LOG_EQUIP_ID_8_LAST_CODE LOG_EQUIP_ID_8_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_9_LAST_CODE)
#define LOG_EQUIP_ID_9_LAST_CODE LOG_EQUIP_ID_9_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_10_LAST_CODE)
#define LOG_EQUIP_ID_10_LAST_CODE LOG_EQUIP_ID_10_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_11_LAST_CODE)
#define LOG_EQUIP_ID_11_LAST_CODE LOG_EQUIP_ID_11_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_12_LAST_CODE)
#define LOG_EQUIP_ID_12_LAST_CODE LOG_EQUIP_ID_12_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_13_LAST_CODE)
#define LOG_EQUIP_ID_13_LAST_CODE LOG_EQUIP_ID_13_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_14_LAST_CODE)
#define LOG_EQUIP_ID_14_LAST_CODE LOG_EQUIP_ID_14_LAST_CODE_DEFAULT
#endif

#if !defined (LOG_EQUIP_ID_15_LAST_CODE)
#define LOG_EQUIP_ID_15_LAST_CODE LOG_EQUIP_ID_15_LAST_CODE_DEFAULT
#endif


/* -------------------------------------------------------------------------
 * Logging mask implementation details.
 *
 * The logging mask stores a bit for every code within the range specified
 * in log_codes.h.   Each equipment ID has a mask that is represented
 * as an array of bytes.  All of this are listed in an array of bytes
 * of size 'LOG_MASK_SIZE'.  An offset into this array is used to determine
 * the start of the mask associated with a particular equipment ID.
 *
 * The range is inclusive, meaning the beginning (0) and end value
 * ('LOG_EQUIP_ID_xxx_LAST_ITEM') are included in the range.  Therefore, all 
 * equipment IDs have at least 1 byte (range 0-0).
 *
 * 'log_mask' is the mask of bits used to represent the configuration of all
 * log codes.  '1' denotes the code being enabled, '0' denotes disabled.
 *
 * 'log_last_item_tbl' is an array of offsets into log_mask indexed by
 * equipment ID.
 *
 * 'LOG_MASK_ARRAY_INDEX()' determine the index into the mask for a given
 * equipment ID.
 *
 * 'LOG_MASK_BIT_MASK()' gives the bit of the code within its byte in the
 * mask.
 *
 * 'LOG_GET_EQUIP_ID()' retuns the equipment ID of a given log code.
 * 
 * 'LOG_GET_ITEM_NUM()' returns the item number of a given log code.
 *
 * 'log_mask_enabled()' returns non-zero if a code is enabled.
 *
 * 'log_set_equip_id()' sets the equipment ID in a log code.
 *
 * ------------------------------------------------------------------------- */

#define LOG_MASK_ARRAY_INDEX(xx_item) ((xx_item) >> 3)

#define LOG_MASK_BIT_MASK(xx_item) (0x01 << ((xx_item) & 7))

#define LOG_GET_EQUIP_ID(xx_code) ((((log_code_type) (xx_code)) >> 12) & 0x000F)

#define LOG_GET_ITEM_NUM(xx_code) (((log_code_type) (xx_code)) & 0x0FFF)

/* This computes the number of bytes in the log mask array. */
#define MAX_EQUIP_ID 16
#define MAX_ITEMS_PER_EQUIP_ID 512
#define LOG_MASK_ITEM_SIZE (sizeof(uint8) + sizeof(unsigned int) + MAX_ITEMS_PER_EQUIP_ID)
#define LOG_MASK_SIZE (MAX_EQUIP_ID * LOG_MASK_ITEM_SIZE)
#define DCI_LOG_EQUIP_MAX_SIZE  (MAX_ITEMS_PER_EQUIP_ID + 2)
#define DCI_LOG_MASK_SIZE	(MAX_EQUIP_ID*DCI_LOG_EQUIP_MAX_SIZE)

typedef PACK(struct) {
	uint8 equip_id;
	unsigned int num_items;
	byte mask[MAX_ITEMS_PER_EQUIP_ID];
} diag_log_mask_t;

#endif  /* DIAGLOGI_H */
