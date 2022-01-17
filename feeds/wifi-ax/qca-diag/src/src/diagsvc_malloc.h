#ifndef DIAGSVCMALLOC_H
#define DIAGSVCMALLOC_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                     Mobile Diagnostic Malloc Module Header

General Description

Initialization and Sequencing Requirements

# Copyright (c) 2007-2011, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================
                          Edit History

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ      Added diag_types.h, a customized version of comdef.h for DIAG
05/19/08   JV      Added the #defines that identify which pool to allocate from
11/01/07   vs      Created
===========================================================================*/

#include "malloc.h"
#include "comdef.h"
#include "diag_shared_i.h"
#include "diag_lsm_pkt_i.h"

#define DIAGSVC_MALLOC_MAX_ITEMS	10
#define DIAGSVC_MALLOC_ITEM_SIZE	(2 * 1024)
#define DIAGSVC_MALLOC_PKT_ITEM_SIZE                             \
        (int)(DIAG_MAX_RX_PKT_SIZ + DIAG_REST_OF_DATA_POS +      \
         FPOS (diagpkt_lsm_rsp_type, rsp.pkt) + sizeof (uint16)) \

#define GEN_SVC_ID                1
#define PKT_SVC_ID                2

typedef struct
{
	void * ptr;
	int in_use;
} diagmem;

/*===========================================================================

FUNCTION DiagSvc_Malloc

DESCRIPTION
  Wrapper function for malloc. Returns a pre-malloced memory region. Does a simple
  malloc if the pre-malloced buffers have been exhausted. Uses different memory
  pools depending on the Svc_ID. Events, logs, F3s and delayed responses are
  allocated from one pool and responses are allocated from nother pool.

DEPENDENCIES:
  DiagSvcMalloc_Init should be called ONCE before this function is called
RETURN VALUE
  Same as malloc

===========================================================================*/
void *DiagSvc_Malloc(size_t size, int Svc_ID);



/*===========================================================================

FUNCTION DiagSvc_Free

DESCRIPTION
  Wrapper function for free. Frees a pre-malloced memory region, by returning
  it to the available list of buffers. This function does not invoke "free"
  for buffers that were pre-alloacted. The pool to return to is based on a
  paarmeter.

DEPENDENCIES:
  DiagSvcMalloc_Init should be called ONCE before this function is called
RETURN VALUE
  None

===========================================================================*/
void DiagSvc_Free(void *ptr, int Svc_ID);


/*===========================================================================

FUNCTION DiagSvc_Malloc_Init

DESCRIPTION
  Funciton to initialize the buffers used for DiagSvc_Malloc.

DEPENDENCIES:

RETURN VALUE
  TRUE if initialization is successful, FALSE otherwise

===========================================================================*/
boolean DiagSvc_Malloc_Init(void);


/*===========================================================================

FUNCTION DiagSvc_Malloc_Exit

DESCRIPTION
  Funciton to free the buffers used for DiagSvc_Malloc.

DEPENDENCIES:

RETURN VALUE
  None

===========================================================================*/
void DiagSvc_Malloc_Exit(void);


#endif /* DIAGSVCMALLOC_H */
