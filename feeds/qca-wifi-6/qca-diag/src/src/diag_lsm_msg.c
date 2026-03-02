/*===========================================================================
Diag Legacy Service Mapping Layer Implementation for Debug Message
(F3 Message) Service, and Optimized Debug Message Service
(Also known as QSHRINK Message)

GENERAL DESCRIPTION
   API definitons for Debug Message Service Mapping Layer.

EXTERNALIZED FUNCTIONS
Note: These functions should not be used directly, use the MSG_* macros instead.
   msg_send
   msg_send_1
   msg_send_2
   msg_send_3
   msg_send_var
   msg_sprintf
Note: These functions or the relevant macros (QSR_MSG*) should not be called directly.
	  	MSG* macros are converted to QSR_MSG* with a text replacement before build.
	  	qsr_msg_send
	  	qsr_msg_send_1
	  	qsr_msg_send_2
	  	qsr_msg_send_3
	  	qsr_msg_send_var


INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2007-2011, 2013-2015 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
===========================================================================*/

/*===========================================================================
                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header:

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   SJ	   Changes for CBSP 2.0
05/01/08   JV      Added support to update the copy of run-time masks in the
                   msg_mask_tbl in this process during initialization and also
				   on mask change
11/29/07   mad     Created File
===========================================================================*/

#include "diagsvc_malloc.h"
#include "diag_lsm_msg_i.h"
#include "msg.h"
#include "msg_qsr.h"
#include "msgcfg.h"
#include "diag_lsmi.h"
#include "diag_lsm.h"
#include "diagcmd.h"
#include "diag_shared_i.h" /* for definition of diag_data struct, and diag datatypes. */
#include "msg_pkt_defs.h"
#include "msg_arrays_i.h"

#include <sys/ioctl.h>
#include <unistd.h>
#include "errno.h"
#include "stdio.h"
#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ts_linux.h"
#include <string.h>
#include <stdint.h>

/* internal datatypes and defines */
typedef struct {
	uint32 args[10];
} msg_large_args;

typedef union {
	msg_ext_type ext;
} msg_sprintf_desc_type;

#define MSG_LARGE_ARGS(X)	(((msg_large_args *)X)->args)
#define MSG_TS_TYPE		0
#define MSG_TIME_FORMAT		3

#ifndef MSG_FMT_STR_ARG_SIZE
/* 280 is guess, close enough to accomodate QCRIL messages upto 252 bytes long */
#define MSG_FMT_STR_ARG_SIZE	280
#endif

static uint32 msg_drop_delta;	/* number of dropped messages */
static int gnDiag_LSM_Msg_Initialized = 0;

/* Internal function declarations */
static boolean msg_get_ssid_rt_mask(int ssid, uint32* mask);
static byte *msg_send_prep(const msg_const_type *const_blk, unsigned int num_args,
			   unsigned int *pLength, uint64 timestamp, boolean ts_valid);
static byte *msg_sprintf_prep(const msg_const_type *const_blk, unsigned int num_args,
			      unsigned int *pLength);
static byte *qsr_msg_send_prep(const msg_qsr_const_type *const_blk, unsigned int num_args,
			       unsigned int *pLength);
static const char *msg_format_filename2(const char *filename);

/*---------------------------------------------------------------------------------------------------
                                    Externalised functions
(Do not call any of these functions directly, use the Macros defined in msg.h instead.)
---------------------------------------------------------------------------------------------------*/

/*===========================================================================
FUNCTION MSG_SEND

DESCRIPTION
   This function sends out a debug message with no arguments across DiagPkt CS interface.
   Do not call directly; use macro MSG_* defined in msg.h

DEPENDENCIES
   DiagPkt handle should be initialised.
===========================================================================*/
void msg_send(const msg_const_type *const_blk)
{
	byte *pMsg = NULL;
	const unsigned int num_args = 0;
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE)
	    return;

	pMsg = msg_send_prep(const_blk, num_args, &nLength, 0, FALSE);
	if (!pMsg)
		return;

	write_len = write(diag_fd, (const void *)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);

	}

	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SEND_TS

DESCRIPTION
   This function sends out a debug message with no arguments, and uses
    timestamp passed in by client. Do not call directly; use macro MSG_*
	 defined in msg.h

DEPENDENCIES
   diag driver handle should be initialised.
===========================================================================*/
void msg_send_ts(const msg_const_type *const_blk, uint64 timestamp)
{
	byte *pMsg = NULL;
	const unsigned int num_args = 0;
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE)
		return;

	pMsg = msg_send_prep(const_blk, num_args, &nLength, timestamp, TRUE);
	if (!pMsg)
		return;

	write_len = write(diag_fd, (const void *) pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);

	}

	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SEND_1

DESCRIPTION
   This function sends out a debug message with 1 argument across DiagPkt CS interface.
   Do not call directly; use macro MSG_* defined in msg.h

DEPENDENCIES
   DiagPkt handle should be initialised.
===========================================================================*/
void msg_send_1(const msg_const_type *pconst_blk, uint32 xx_arg1)
{
	byte *pMsg = NULL;
	msg_ext_type *pTemp = NULL;
	uint32 *args = NULL;
	unsigned int nLength = 0;
	const unsigned int num_args = 1;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE)
	    return;

	pMsg = msg_send_prep(pconst_blk, num_args, &nLength, 0, FALSE);
	if (!pMsg)
		return;

	pTemp = (msg_ext_type*)(pMsg + DIAG_REST_OF_DATA_POS);
	args = pTemp->args;
	args[0] = xx_arg1;

	write_len = write(diag_fd, (const void*)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SEND_2

DESCRIPTION
   This function sends out a debug message with 2 arguments across DiagPkt CS interface.
   Do not call directly; use macro MSG_* defined in msg.h

DEPENDENCIES
   DiagPkt handle should be initialised.
===========================================================================*/
void msg_send_2(const msg_const_type *pconst_blk, uint32 xx_arg1, uint32 xx_arg2)
{
	byte *pMsg = NULL;
	msg_ext_type *pTemp = NULL;
	uint32 *args = NULL;
	unsigned int nLength = 0;
	const unsigned int num_args = 2;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE)
		return;

	pMsg = msg_send_prep(pconst_blk, num_args, &nLength, 0, FALSE);
	if (!pMsg)
		return;

	pTemp = (msg_ext_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	args = MSG_LARGE_ARGS(pTemp->args);
	args[0] = xx_arg1;
	args[1] = xx_arg2;
	write_len = write(diag_fd, (const void*)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SEND_3

DESCRIPTION
This function sends out a debug message with 2 arguments across DiagPkt CS interface.
Do not call directly; use macro MSG_* defined in msg.h

DEPENDENCIES
DiagPkt handle should be initialised.
===========================================================================*/
void msg_send_3(const msg_const_type *pconst_blk, uint32 xx_arg1, uint32 xx_arg2, uint32 xx_arg3)
{
	byte *pMsg = NULL;
	msg_ext_type *pTemp = NULL;
	uint32 *args = NULL;
	unsigned int nLength = 0;
	const unsigned int num_args = 3;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE)
		return;

	pMsg = msg_send_prep(pconst_blk, num_args, &nLength, 0, FALSE);
	if (!pMsg)
		return;

	pTemp = (msg_ext_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	args = MSG_LARGE_ARGS(pTemp->args);
	args[0] = xx_arg1;
	args[1] = xx_arg2;
	args[2] = xx_arg3;
	write_len = write(diag_fd, (const void*)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SEND_VAR

DESCRIPTION
   This function sends out a debug message with variable number of arguments
   across DiagPkt CS interface.
   Do not call directly; use macro MSG_* defined in msg.h

DEPENDENCIES
   DiagPkt handle should be initialised.

===========================================================================*/

void msg_send_var (const msg_const_type *pconst_blk, uint32 num_args, ...)
{
	byte *pMsg = NULL;
	msg_ext_type *pTemp = NULL;
	uint32 *args = NULL;
	unsigned int nLength = 0;
	int write_len = 0;
	va_list arg_list;
	unsigned int i;

	if (diag_fd == DIAG_INVALID_HANDLE)
		return;

	pMsg = msg_send_prep(pconst_blk, num_args, &nLength, 0, FALSE);
	if (!pMsg)
		return;
	pTemp = (msg_ext_type*)(pMsg + DIAG_REST_OF_DATA_POS);
	args = MSG_LARGE_ARGS(pTemp->args);
	/* Initialize variable arguments */
	va_start (arg_list, num_args);
	/* Store arguments from variable list. */
	for (i = 0; i < num_args; i++)
		args[i] = va_arg(arg_list, uint32);
	/* Reset variable arguments */
	va_end(arg_list);
	write_len = write(diag_fd, (const void*)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION MSG_SPRINTF

DESCRIPTION

   This function sends out a debug message with variable number of arguments
   across DiagPkt CS interface.
   This will build a message sprintf diagnostic Message with var #
   of parameters.
   Do not call directly; use macro MSG_SPRINTF_* defined in msg.h

DEPENDENCIES
   DiagPkt handle should be initialised.
===========================================================================*/
void msg_sprintf(const msg_const_type *const_blk, ...)
{
	int write_len = 0;
	byte *pMsg = NULL;
	const char* abb_filename = NULL;
	msg_sprintf_desc_type *pTemp = NULL;
	unsigned int int_cnt = 0;		/* Calculate the # args, to allocate buffer */
	unsigned int fname_length = 0;		/* Stores the file name along with '\0' */
	unsigned int fmt_length = 0;		/* Stores the fmt length,'\0' and arg size */
	unsigned int total_allocated = 0;	/* Total buffer allocated */
	char *str = NULL;			/* Used to copy the file name and fmt string to the msg */
	va_list arg_list;			/* ptr to the variable argument list */
	unsigned int fmt_len_available = 0;	/* Remaining buffer for format string */

	if (diag_fd == DIAG_INVALID_HANDLE || !const_blk)
		return;

	abb_filename = msg_format_filename2(const_blk->fname);
	fname_length = strlen(abb_filename) + 1;
	fmt_length = strlen(const_blk->fmt) + 1 + MSG_FMT_STR_ARG_SIZE;

	/* Calculate # of arguments to ensure enough space is allocated. */
	int_cnt = sizeof(msg_desc_type) - FSIZ(msg_ext_store_type, const_data_ptr) + fmt_length + fname_length;

	/* Calculates number of uint32s required */
	int_cnt = (int_cnt + sizeof(uint32) - 1) / sizeof(uint32);

	/* Allocates the buffer required, fills in the header  */
	pMsg = msg_sprintf_prep(const_blk, int_cnt, &total_allocated);
	if (!pMsg)
		return;

	pTemp = (msg_sprintf_desc_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	/* Queue a debug message in Extended Message Format. */
	pTemp->ext.hdr.cmd_code = DIAG_EXT_MSG_F;
	/*
	 * This function embedds the argument in the string itself. Hence the
	 * num_args is assigned 0
	 */
	pTemp->ext.hdr.num_args = 0;
	pTemp->ext.desc = const_blk->desc;
	/*
	 * Copy the format string where the argument list would
	 * start. Since there are no arguments, the format string
	 * starts in the 'args' field.
	 */
	str = (char *)pTemp->ext.args;
	/* Calculate the buffer left to copy the format string */
	fmt_len_available = total_allocated - (FPOS (msg_ext_type, args) + fname_length);
	if (fmt_len_available < fmt_length)
		fmt_length = fmt_len_available;

	/* Initialize variable argument list */
	va_start(arg_list, const_blk);
	/* Copy the format string with arguments */
	(void)vsnprintf(str, fmt_length, const_blk->fmt, arg_list);
	str[fmt_length - 1] = '\0';
	/* Reset variable arguments */
	va_end(arg_list);
	/*
	 * Move the str pass the format string, strlen excludes the terminal
	 * NULL hence 1 is added to include NULL.
	 */
	str += strlen((const char *)str) + 1;
	/* Copy the filename */
	snprintf(str, fname_length, "%s", abb_filename);
	/*
	 * Move the str pass the filename, strlen excludes the terminal
	 * NULL hence 1 is added to include NULL.
	 */
	str += strlen((const char *)str) + 1;
	/*
	 * str is now pointing to the byte after the last valid byte.
	 * str - msg gives the total length required.
	 */
	write_len = write(diag_fd, (const void *)pMsg, (uint32)(str - (char *)pMsg));
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
	return;
}

/*===========================================================================
FUNCTION qsr_msg_send

DESCRIPTION
   This function sends out a debug message with no arguments.
   This function, or the macro QSR_MSG should not be called directly,
   MSG macros are converted to QSR_MSG macro by text-replacement.

DEPENDENCIES
   diag driver should be initialized with a Diag_LSM_Init() call.
===========================================================================*/
void qsr_msg_send(const msg_qsr_const_type *const_blk)
{
	byte *pMsg = NULL;
	const unsigned int num_args = 0;  /* # of message arguments */
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE || !const_blk)
		return;

	pMsg = qsr_msg_send_prep(const_blk, num_args, &nLength);
	if (!pMsg)
		return;
	write_len = write(diag_fd, (const void *)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION qsr_msg_send_1

DESCRIPTION
   This function sends out a debug message with no arguments.
   This function, or the macro QSR_MSG_1 should not be called directly,
   MSG* macros are converted to QSR_MSG* macros by text-replacement.

DEPENDENCIES
   diag driver should be initialized with a Diag_LSM_Init() call.
===========================================================================*/

void qsr_msg_send_1(const msg_qsr_const_type *const_blk, uint32 xx_arg1)
{
	byte* pMsg = NULL;
	const unsigned int num_args = 1;
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE || !const_blk)
		return;

	pMsg = qsr_msg_send_prep (const_blk, num_args, &nLength);
	if (!pMsg)
		return;
	msg_qsr_type *pTemp = (msg_qsr_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	pTemp->args[0] = xx_arg1;
	write_len = write(diag_fd, (const void *)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION qsr_msg_send_2

DESCRIPTION
   This function sends out a debug message with no arguments.
   This function, or the macro QSR_MSG_2 should not be called directly,
   MSG* macros are converted to QSR_MSG* macros by text-replacement.

DEPENDENCIES
   diag driver should be initialized with a Diag_LSM_Init() call.
===========================================================================*/

void qsr_msg_send_2(const msg_qsr_const_type * const_blk, uint32 xx_arg1, uint32 xx_arg2)
{
	byte* pMsg = NULL;
	const unsigned int num_args = 2;
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE || !const_blk)
		return;

	pMsg = qsr_msg_send_prep (const_blk, num_args, &nLength);
	if (!pMsg)
		return;
	msg_qsr_type *pTemp = (msg_qsr_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	pTemp->args[0] = xx_arg1;
	pTemp->args[1] = xx_arg2;
	write_len = write(diag_fd, (const void *)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION qsr_msg_send_3

DESCRIPTION
   This function sends out a debug message with no arguments.
   This function, or the macro QSR_MSG_3 should not be called directly,
   MSG* macros are converted to QSR_MSG* macros by text-replacement.

DEPENDENCIES
   diag driver should be initialized with a Diag_LSM_Init() call.
===========================================================================*/

void qsr_msg_send_3(const msg_qsr_const_type *const_blk, uint32 xx_arg1, uint32 xx_arg2, uint32 xx_arg3)
{
	byte* pMsg = NULL;
	const unsigned int num_args = 3;
	unsigned int nLength = 0;
	int write_len = 0;

	if (diag_fd == DIAG_INVALID_HANDLE || !const_blk)
		return;

	pMsg = qsr_msg_send_prep (const_blk, num_args, &nLength);
	if (!pMsg)
		return;
	msg_qsr_type *pTemp = (msg_qsr_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	pTemp->args[0] = xx_arg1;
	pTemp->args[1] = xx_arg2;
	pTemp->args[2] = xx_arg3;
	write_len = write(diag_fd, (const void *)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*===========================================================================
FUNCTION qsr_msg_send_var

DESCRIPTION
   This function sends out a debug message with no arguments.
   This function, or the macro QSR_MSG_* should not be called directly,
   MSG* macros are converted to QSR_MSG* macros by text-replacement.

DEPENDENCIES
   diag driver should be initialized with a Diag_LSM_Init() call.
===========================================================================*/
void qsr_msg_send_var(const msg_qsr_const_type *const_blk, uint32 num_args, ...)
{
	byte *pMsg = NULL;
	msg_qsr_type *pTemp = NULL;
	uint32 *args = NULL;
	unsigned int nLength = 0;
	int write_len = 0;
	va_list arg_list;
	unsigned int i;

	if (diag_fd == DIAG_INVALID_HANDLE)
		return;

	pMsg = qsr_msg_send_prep(const_blk, num_args, &nLength);
	if (!pMsg)
		return;
	pTemp = (msg_qsr_type *)(pMsg + DIAG_REST_OF_DATA_POS);
	args = MSG_LARGE_ARGS(pTemp->args);
	/* Initialize variable arguments */
	va_start (arg_list, num_args);
	/* Store arguments from variable list. */
	for (i = 0; i < num_args; i++)
		args[i] = va_arg(arg_list, uint32);
	/* Reset variable arguments */
	va_end(arg_list);
	write_len = write(diag_fd, (const void*)pMsg, nLength);
	if (write_len) {
		DIAG_LOGE("Diag_LSM_Msg: Write failed in %s, bytes written: %d, error: %d\n",
			  __func__, write_len, errno);
	}
	DiagSvc_Free(pMsg, GEN_SVC_ID);
}

/*----------------------------------------------------------------------------
  Internal functions
 -----------------------------------------------------------------------------*/
static uint32 Diag_LSM_Msg_ComputeMaskSize(void)
{
	uint32 msg_mask_size = 0;
	int i = 0;
	for (i = 0; i < MSG_MASK_TBL_CNT; i++) {
		msg_mask_size += sizeof(uint16) + sizeof(uint16);
		msg_mask_size += (msg_mask_tbl[i].ssid_last - msg_mask_tbl[i].ssid_first + 1) * sizeof(uint32);
	}
	return msg_mask_size;
}

/*===========================================================================
FUNCTION Diag_LSM_Msg_Init

DESCRIPTION
Initializes the Diag Message service mapping layer.

DEPENDENCIES
None

===========================================================================*/
boolean Diag_LSM_Msg_Init (void)
{
	if (!gnDiag_LSM_Msg_Initialized) {
		Diag_LSM_Msg_ComputeMaskSize();
		gnDiag_LSM_Msg_Initialized = 1;
	}

	return TRUE;
}

/*===========================================================================
FUNCTION Diag_LSM_Msg_DeInit

DESCRIPTION
Prepares mapping layer exit for Diag message service.
Currently does nothing, just returns TRUE.
This is an internal function, to be used only by Diag_LSM module.

DEPENDENCIES
None.


===========================================================================*/
void Diag_LSM_Msg_DeInit(void)
{
	gnDiag_LSM_Msg_Initialized = 0;
}

/*===========================================================================
FUNCTION msg_update_mask

DESCRIPTION
   This function sends updates the data structure for msg masks.

DEPENDENCIES
  None
===========================================================================*/
void msg_update_mask(unsigned char *ptr, int len)
{
	int i = 0;
	int read_len = 0;
	unsigned int range = 0;
	diag_msg_mask_t *mask_ptr = (diag_msg_mask_t *)msg_mask;
	diag_msg_mask_update_t *header;

	if (!ptr || len <= (int)sizeof(diag_msg_mask_update_t) || !gnDiag_LSM_Msg_Initialized)
		return;

	for (i = 0; i < MSG_MASK_TBL_CNT && read_len < len; i++, mask_ptr++) {
		header = (diag_msg_mask_update_t *)(ptr + read_len);
		read_len += sizeof(diag_msg_mask_update_t);
		mask_ptr->ssid_first = header->ssid_first;
		mask_ptr->ssid_last = header->ssid_last;
		range = header->range;
		if (range > MAX_SSID_PER_RANGE) {
			mask_ptr->ssid_last = mask_ptr->ssid_first + MAX_SSID_PER_RANGE;
			range = MAX_SSID_PER_RANGE;
		}
		memcpy(mask_ptr->ptr, ptr + read_len, range * sizeof(uint32));
		read_len += header->range * sizeof(uint32);
	}
}

boolean msg_get_ssid_rt_mask(int ssid, uint32 *mask)
{
	int i;
	uint32 offset = 0;
	uint32 t_ssid = (uint32)ssid;
	boolean success = FALSE;
	diag_msg_mask_t *mask_ptr = NULL;

	mask_ptr = (diag_msg_mask_t *)msg_mask;
	for (i = 0; i < MSG_MASK_TBL_CNT; i++, mask_ptr++) {
		if (mask_ptr->ssid_first > t_ssid || mask_ptr->ssid_last < t_ssid)
			continue;
		offset = (t_ssid - mask_ptr->ssid_first);
		*mask = *(uint32_t *)(mask_ptr->ptr + offset);
		success = TRUE;
		break;
	}

	return success;
}

/*===========================================================================
FUNCTION msg_format_filename2

DESCRIPTION
retrieves the position of filename from full file path.

DEPENDENCIES
None.
===========================================================================*/
static const char *msg_format_filename2 (const char *filename)
{
	const char *p_front = filename;
	const char *p_end = filename + strlen (filename);

	while (p_end != p_front) {
		if ((*p_end == '\\') || (*p_end == ':') || (*p_end == '/')) {
			p_end++;
			break;
		}
		p_end--;
	}
	return p_end;
}

/*===========================================================================

FUNCTION MSG_SEND_PREP

DESCRIPTION
   Prepares the buffer needed by msg_send*().
   Allocates, fills in all data except arguments, and returns a pointer
   to the allocated message buffer.  It also handles message statisitics.

RETURN VALUE
   Returns the allocated buffer, and the length of the buffer

DEPENDENCIES
   None
===========================================================================*/

static byte *msg_send_prep(const msg_const_type *const_blk, unsigned int num_args,
			   unsigned int *pLength, uint64 timestamp, boolean ts_valid)
{
	uint32 rt_mask;
	uint32 fmt_pos;
	boolean valid_ssid = FALSE;
	byte *pMsg = NULL;
	const char* abb_filename = NULL;
	unsigned int alloc_len = 0;
	msg_ext_type* pTemp = NULL;
	diag_data* pdiag_data = NULL;

	if(!gnDiag_LSM_Msg_Initialized)
		return NULL;
	if (pLength)
		*pLength = 0;

	/* Check the runtime mask */
	valid_ssid = msg_get_ssid_rt_mask((int)const_blk->desc.ss_id, &rt_mask);
	if (!(valid_ssid && (const_blk->desc.ss_mask & rt_mask)))
		return NULL;

	abb_filename = msg_format_filename2(const_blk->fname);

	/* total number of bytes to be allocated, including dereferenced FileName and Format strings */
	alloc_len = DIAG_REST_OF_DATA_POS + FPOS (msg_ext_type, args) + num_args * FSIZ (msg_ext_type,args[0]) +
		    strlen(abb_filename) + 1 + strlen(const_blk->fmt) + 1;

	/* Get a pointer to a buffer.  If it's a NULL pointer, there is no memory in the client heap. */
	pMsg = (byte *)DiagSvc_Malloc(alloc_len, GEN_SVC_ID);
	if (!pMsg) {
		msg_drop_delta++;
		return NULL;
	}

	/* position of format string in the returned buffer. */
	pTemp = (msg_ext_type*)((byte*)pMsg + DIAG_REST_OF_DATA_POS);
	fmt_pos = DIAG_REST_OF_DATA_POS + FPOS (msg_ext_type, args) + num_args * FSIZ (msg_ext_type,args[0]);
	pdiag_data = (diag_data*) pMsg;
	pdiag_data->diag_data_type = DIAG_DATA_TYPE_F3;
	if(pLength)
		*pLength = alloc_len; /* return the number of bytes allocated. */

	/* client timestamp is valid, copy that into the header */
	if (ts_valid) {
		timestamp = timestamp * 4;
		timestamp = timestamp / 5;
		timestamp = timestamp << 16;
		memcpy((char *) (&(pTemp->hdr.ts_lo)), (char *) &(timestamp), 4);
		memcpy((char *) (&(pTemp->hdr.ts_hi)), ((char *) &(timestamp)) + 4, 4);
	} else {
		ts_get_lohi(&(pTemp->hdr.ts_lo), &(pTemp->hdr.ts_hi));
	}

	pTemp->hdr.ts_type = MSG_TS_TYPE;
	pTemp->hdr.cmd_code = DIAG_EXT_MSG_F;
	pTemp->hdr.num_args = (uint8)num_args;
	pTemp->hdr.drop_cnt = (unsigned char)((msg_drop_delta > 255) ? 255 : msg_drop_delta);
	msg_drop_delta = 0;   /* Reset delta drop count */

	/*
	 * expand it now, copy over the filename and format strings. The order
	 * is: hdr,desc,args,format string, filename. args are copied in the
	 * msg_send_1 etc... functions
	 */
	memcpy((void *)((char *)(pMsg) + DIAG_REST_OF_DATA_POS + sizeof(msg_hdr_type)), (void *)&(const_blk->desc), sizeof (msg_desc_type));
	memcpy((void *)((char *)(pMsg) + fmt_pos), (void *)(const_blk->fmt), strlen(const_blk->fmt) + 1);
	memcpy((void *)((char *)(pMsg) + fmt_pos + strlen(const_blk->fmt) + 1), (void *)(abb_filename), strlen(abb_filename) + 1);
	return pMsg;
}

/*===========================================================================
FUNCTION qsr_msg_send_prep

DESCRIPTION
   Internal function.
   Prepares the buffer that is sent to diag driver by the qsr_msg_send* functions.
   The const block is expanded in the context of the caller.
===========================================================================*/
static byte* qsr_msg_send_prep(const msg_qsr_const_type *const_blk,
			       unsigned int num_args, unsigned int *pLength)
{
	uint32 rt_mask;
	boolean valid_ssid = FALSE;
	byte *pMsg = NULL;
	unsigned int alloc_len = 0;
	msg_qsr_type *pTemp = NULL;
	diag_data *pdiag_data = NULL;

	if (!gnDiag_LSM_Msg_Initialized)
		return NULL;

	if (pLength)
		*pLength = 0;
	/* Check the runtime mask */
	valid_ssid = msg_get_ssid_rt_mask(const_blk->desc.ss_id, &rt_mask);
	if (!(valid_ssid && (const_blk->desc.ss_mask & rt_mask)))
		return NULL;

	/* total number of bytes to be allocated, including space for the hash value */
	alloc_len = DIAG_REST_OF_DATA_POS + FPOS(msg_qsr_type, args) + num_args * FSIZ(msg_qsr_type, args[0]);

	/* Get a pointer to a buffer.  If it's a NULL pointer, there is no memory in the client heap. */
	pMsg = (byte *)DiagSvc_Malloc(alloc_len, GEN_SVC_ID);
	if (!pMsg) {
		msg_drop_delta++;
		return NULL;
	}

	/* Find the position to copy in the header, const expanded values etc */
	pTemp = (msg_qsr_type *)((byte *)pMsg + DIAG_REST_OF_DATA_POS);

	/* For diag driver to recognize that this is an F3 Msg. */
	pdiag_data = (diag_data *)pMsg;
	pdiag_data->diag_data_type = DIAG_DATA_TYPE_F3;

	if (pLength)
		*pLength = alloc_len; /* return the number of bytes allocated. */

	ts_get_lohi(&(pTemp->hdr.ts_lo), &(pTemp->hdr.ts_hi));

	pTemp->hdr.ts_type = MSG_TS_TYPE;
	pTemp->hdr.cmd_code = DIAG_QSR_EXT_MSG_TERSE_F; /* cmd_code = 146 for QSR messages */
	pTemp->hdr.num_args = (uint8)num_args;
	pTemp->hdr.drop_cnt = (unsigned char)((msg_drop_delta > 255) ? 255 : msg_drop_delta);
	msg_drop_delta = 0;   /* Reset delta drop count */

	/* expand it now.
	 The order is: hdr (already done),desc,hash,args. args are copied in the qsr_msg_send* functions */
	pTemp->desc.line = const_blk->desc.line;
	pTemp->desc.ss_id = const_blk->desc.ss_id;
	pTemp->desc.ss_mask = const_blk->desc.ss_mask;
	pTemp->msg_hash = const_blk->msg_hash;

	return pMsg;
}

/*===========================================================================
FUNCTION MSG_SPRINTF_PREP

DESCRIPTION
   Prepares the buffer needed by msg_sprintf().
   Allocates, fills in all data except arguments, and returns a pointer
   to the allocated message buffer.  It also handles message statisitics.

RETURN VALUE
   Returns the allocated buffer, and the length of the buffer

DEPENDENCIES
   None
===========================================================================*/
static byte *msg_sprintf_prep(const msg_const_type *pconst_blk, unsigned int num_args,
			      unsigned int* pLength)
{
	uint32 rt_mask;
	boolean valid_ssid = FALSE;
	byte *pMsg = NULL;
	unsigned int alloc_len = DIAG_REST_OF_DATA_POS + FPOS(msg_ext_store_type, args) +
				 num_args * FSIZ(msg_ext_store_type, args[0]);

	if (!gnDiag_LSM_Msg_Initialized)
		return NULL;
	/* Check the runtime mask */
	valid_ssid = msg_get_ssid_rt_mask((int)pconst_blk->desc.ss_id, &rt_mask);
	if (!(valid_ssid && (pconst_blk->desc.ss_mask & rt_mask)))
		return NULL;

	/* Get a pointer to a buffer.  If it's a NULL pointer, there is no memory in the client heap. */
	pMsg = (byte *)DiagSvc_Malloc(alloc_len, GEN_SVC_ID);
	if (!pMsg) {
		msg_drop_delta++;
		return NULL;
	}

	msg_ext_store_type *pTemp = (msg_ext_store_type *)((byte *)pMsg + DIAG_REST_OF_DATA_POS);
	diag_data *pdiag_data = (diag_data *)pMsg;
	pdiag_data->diag_data_type = DIAG_DATA_TYPE_F3;

	if (pLength)
		*pLength = alloc_len;

	ts_get_lohi(&(pTemp->hdr.ts_lo), &(pTemp->hdr.ts_hi));
	pTemp->hdr.ts_type = MSG_TS_TYPE;
	pTemp->hdr.num_args = (uint8)num_args;
	pTemp->hdr.drop_cnt = (unsigned char)((msg_drop_delta > 255)? 255 : msg_drop_delta);
	msg_drop_delta = 0; /* Reset delta drop count */
	/* Set the pointer to the constant blk, to be expanded by msg_sprintf */
	pTemp->const_data_ptr = pconst_blk;

	return pMsg;
}

