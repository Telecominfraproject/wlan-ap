#ifndef LOG_H
#define LOG_H

/*===========================================================================

                      Logging Service Header File

General Description
  This file contains the API for the logging service.

  The logging service allows clients to send information in the form of a 
  log record to the external device that is collecting logs (i.e., QXDM).
  
!!! Important usage note:
  The logging service uses a memory management system for logging outbound 
  information.  Due to limited resources, this memory management system is a
  FIFO queueing system with no garbage collection.  Queue insertion occurs at
  the time a logging buffer is allocated, not when it is commmited.  When you
  allocate a buffer, that buffer blocks the emptying of the FIFO until that 
  log record is commmited.  Therefore, if you hold onto a buffer for a long 
  time, no other log records can be sent until you commit (or free) your 
  buffer.  If you need to accumulate data for a log record, you must 
  accumulate it in your own memory space, not the memory allocated by the 
  logging service.  When ready to send, call log_submit().
  
General usage:  
  ptr = log_alloc(code, length);
  
  if (ptr) {
    //Fill in log record here
    
    log_commit(ptr);
  } 
  

# Copyright (c) 2007-2011, 2014, 2016 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
===========================================================================*/

/*===========================================================================

                             Edit History

$Header: //depot/asic/msmshared/services/diag/Diag_1.5/Diag_LSM/log.h#1 $
   
when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ      Changes for CBSP2.0
01/16/08   JV      Modified comments and descriptions as per Diag 1.5A (WM)
09/18/02   lad     Created file from old version.  Content has been removed
                   from this file, leaving only the logging service API.
===========================================================================*/

#include "log_codes.h"

/* -------------------------------------------------------------------------
   Definitions and Declarations
   ------------------------------------------------------------------------- */

/* Log code type.  Currently this is 16 bits. */
typedef uint16 log_code_type;

/* Log Record Header Type:
   Currently, all log records structure definitions must begin with
   log_hdr_type.  This place holder is needed for the internal 
   implementation of the logging service to function properly.
   
   !!! Notice: Do not reference this header directly.  In planned future
   versions of this service, this type will be typedef void, and the header
   will be transparant to the user of this service.  Any direct reference
   to this type will not compile when this enhancement is implemented. */
   
#if !defined(FEATURE_LOG_EXPOSED_HEADER)

typedef PACK(struct
{
  unsigned char header[12]; /* A log header is 12 bytes long */
}) log_hdr_type;

#else

/* Some clients, for legacy reasons, reference the log header.  Until those
   references are cleaned up, the logging service must expose the header
   to avoid compilation failure. */

typedef PACK(struct
{
  word len;         /* Specifies the length, in bytes of the
                   entry, including this header. */

  word code;            /* Specifies the log code for the entry as
                   enumerated above. Note: This is
                   specified as word to guarantee size. */
// removed AMSS specific code
  //qword ts;          The system timestamp for the log entry. The 
                   /*upper 48 bits represent elapsed time since
                   6 Jan 1980 00:00:00 in 1.25 ms units. The
                   low order 16 bits represent elapsed time
                   since the last 1.25 ms tick in 1/32 chip
                   units (this 16 bit counter wraps at the
                   value 49152). */
  uint32 ts_lo; /* Time stamp */
  uint32 ts_hi;
})
log_hdr_type;

#endif         /* !FEATURE_LOG_EXPOSED_HEADER */

/* Indicates which type of time stamp to use when setting a time stamp. */
typedef enum
{
  LOG_TIME_IND_CDMA_E,
  LOG_TIME_IND_MSP_E,
  LOG_TIME_IND_WCDMA_E
}
log_time_indicator_type;


/* -------------------------------------------------------------------------
     Function Declarations
   ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif

/*===========================================================================

FUNCTION LOG_ALLOC

DESCRIPTION
  This function allocates a buffer of size 'length' for logging data.  The 
  specified length is the length of the entire log, including the log
  header.  This operation is inteneded only for logs that do not require 
  data accumulation.
  
  !!! The header is filled in automatically by this routine.

DEPENDENCIES:
  CS needs to be initialized. 
  log_commit() or log_free() must be called ASAP after this call.
  
RETURN VALUE
  A pointer to the allocated buffer is returned on success.
  Expect a NULL when gnDiagSvcMalloc_Initialized is not initialized
  or if heap is full.

SIDE EFFECTS       
  Since this allocation is made from a shared resource pool, log_commit() 
  or log_free() must be called as soon as possible and in a timely fashion.  
  
  If you need to log accumulated data, store the accumulated data in your 
  own memory space and use log_submit() to log the data.  
===========================================================================*/
  void *log_alloc (log_code_type code, unsigned int length);
  #define log_alloc_ex(a, b) log_alloc (a, b)

/*===========================================================================

FUNCTION LOG_SHORTEN

DESCRIPTION
  This function shortens the length of a previously allocated logging buffer in
  legacy code. This is used when the size of the record is not known at allocation
  time.Now that diagbuf is not used in the LSM layer and we just use memory from
  a pre-allocated pool, calling log_shorten, does not free the excess memory, it just
  updates the length field.

DEPENDENCIES
  This must be called prior to log_commit().

RETURN VALUE
  None.
  
===========================================================================*/
  void log_shorten (void *log_ptr, unsigned int length);

/*===========================================================================

FUNCTION LOG_COMMIT

DESCRIPTION
  This function commits a log buffer allocated by log_alloc().  Calling this
  function tells the logging service that the user is finished with the
  allocated buffer.
  
DEPENDENCIES
  'ptr' must point to the address that was returned by a prior call to 
  log_alloc().

RETURN VALUE
  None.
  
SIDE EFFECTS
  Since this allocation is made from a shared resource pool, this must be 
  called as soon as possible after a log_alloc call.  This operation is not 
  intended for logs that take considerable amounts of time ( > 0.01 sec ).

===========================================================================*/
  void log_commit (void *ptr);

/*===========================================================================

FUNCTION LOG_FREE

DESCRIPTION
  This function frees the buffer in pre-allocated memory.

DEPENDENCIES
  'ptr' must point to a log entry that was allocated by log_alloc().

===========================================================================*/
  void log_free (void *ptr);

/*===========================================================================

FUNCTION LOG_SUBMIT

DESCRIPTION
  This function is called to log an accumlated log entry. If logging is
  enabled for the entry by the external device, then the entry is copied 
  into the diag allocation manager and commited immediately.

  This function essentially does the folliwng:
  log = log_alloc ();
  memcpy (log, ptr, log->len);
  log_commit (log);
  
  
  
RETURN VALUE
  Boolean indicating success.

===========================================================================*/
  boolean log_submit (void *ptr);

/*===========================================================================

FUNCTION LOG_SET_LENGTH

DESCRIPTION
  This function sets the length field in the given log record.
  
  !!! Use with caution.  It is possible to corrupt a log record using this
  command.  It is intended for use only with accumulated log records, not
  buffers returned by log_alloc().

===========================================================================*/
  void log_set_length (void *ptr, unsigned int length);

/*===========================================================================

FUNCTION LOG_SET_CODE

DESCRIPTION
  This function sets the logging code in the given log record.

===========================================================================*/
  void log_set_code (void *ptr, log_code_type code);

/*===========================================================================

FUNCTION LOG_SET_TIMESTAMP

DESCRIPTION
  This function captures the system time and stores it in the given log record.

===========================================================================*/
  void log_set_timestamp (
#ifdef FEATURE_ZREX_TIME
    void *ptr, log_time_indicator_type time_type
#else
    void *ptr
#endif
    );

/*===========================================================================

FUNCTION LOG_GET_LENGTH

DESCRIPTION
  This function returns the length field in the given log record.


RETURN VALUE
  An unsigned int, the length

===========================================================================*/
  unsigned int log_get_length (void *ptr);

/*===========================================================================

FUNCTION LOG_GET_CODE

DESCRIPTION
  This function returns the log code field in the given log record.



RETURN VALUE
  log_code_type, the code
===========================================================================*/
  log_code_type log_get_code (void *ptr);

/*===========================================================================

FUNCTION LOG_STATUS

DESCRIPTION
  This function returns whether a particular code is enabled for logging.

===========================================================================*/
  boolean log_status (log_code_type code);

/*===========================================================================

FUNCTION LOG_PROCESS_LSM_MASK_REQ
DESCRIPTION
  Handles requests from LSM to transfer the event mask.
============================================================================*/

//int log_process_LSM_mask_req (unsigned char* mask, int maskLen, int * maskLenReq); 
/* Not required here, not an external API */


/*===========================================================================


NOTE: No function pointer support in diag1.5A. These 2 functions are just stubs 

FUNCTION TYPE LOG_ON_DEMAND

DESCRIPTION
  This function, provided via reference by the caller, indicates a trigger
  for the specified log code issued from the external device.  This routine
  must return status, which is send to the external device.
  
DEPENDENCIES
  None.

RETURN VALUE
  'log_on_demand_status_enum_type'

SIDE EFFECTS
  None.

===========================================================================*/
typedef enum
{
  LOG_ON_DEMAND_SENT_S = 0,
  LOG_ON_DEMAND_ACKNOWLEDGE_S,
  LOG_ON_DEMAND_DROPPED_S,
  LOG_ON_DEMAND_NOT_SUPPORTED_S,
  LOG_ON_DEMAND_FAILED_ATTEMPT_S
}
log_on_demand_status_enum_type;

typedef log_on_demand_status_enum_type (*log_on_demand) (log_code_type
  log_code);

/*===========================================================================

FUNCTION LOG_ON_DEMAND_REGISTER

DESCRIPTION
  This function registers a function pointer to be associated with a 
  log code for logging on demand.  If the external device sends a request
  to trigger this log code, the function will be called.  The logging 
  must be performed by the client of this service.  It will not be 
  performed by the logging service itself.

===========================================================================*/
  boolean log_on_demand_register (log_code_type log_code,
    log_on_demand log_on_demand_ptr);

/*===========================================================================

FUNCTION LOG_ON_DEMAND_UNREGISTER

DESCRIPTION
  This function unregisters the log function 

===========================================================================*/
  boolean log_on_demand_unregister (log_code_type log_code);


/*===========================================================================
MACRO LOG_RECORD_DEFINE
MACRO LOG_RECORD_END
      
DESCRIPTION
  These macros were defined to provide and enforce naming
  conventions for declaring packets.  However, these macros
  make editor tags useless and add consufion.  The use of
  these macros has been deprecated, but is included here for
  compatibility with existing usage.  The naming convention
  enforced by these macros is not required for use of this
  service.
  
  !!! It is not recommended to continue use of these macros.
  
  All that is required for defining a log structure is to place
  a member of 'log_hdr_type' at the top of the structure.  Do not
  access this member directly as this type is slated to be type-cast
  to 'void' when extending the logging service beyond 16-bit log codes.

  The naming convention enforced by these macros is outlined
  below:
  
  Log codes use the naming convention LOG_xxx_F.
  
  This macro expands the name of the defined structure to be:
  LOG_xxx_C_type
   
===========================================================================*/
#ifdef FEATURE_LOG_EXPOSED_HEADER
#define LOG_RECORD_DEFINE( xx_code )           \
  typedef struct xx_code##_tag xx_code##_type; \
  PACK(struct) xx_code##_tag {                \
    log_hdr_type hdr;
#else
#define LOG_RECORD_DEFINE( xx_code )           \
  typedef struct xx_code##_tag xx_code##_type; \
  PACK(struct) xx_code##_tag {                \
    log_hdr_type xx_hdr;
#endif

#define LOG_RECORD_END };

#if defined(FEATURE_DIAG_PACKET_COUPLING)

/* In legacy versions, log.h contains log packet definitions.  Those 
   definitions have been moved to a separate file to isolate coupling. */
#include "log_dmss.h"

#endif


#ifdef __cplusplus
}
#endif
#endif              /* LOG_H */
