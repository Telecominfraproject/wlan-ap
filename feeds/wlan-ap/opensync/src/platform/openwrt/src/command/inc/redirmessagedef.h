/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __REDIR_MSG_DEF_H__
#define __REDIR_MSG_DEF_H__

#include "redirdebug.h"

#define GCC_PACKED __attribute__((packed))

typedef struct
{
	unsigned char  msgId;
	unsigned char  seqNum;
	unsigned short msgLen;  // asn.1 message length
	unsigned char  buf[0];
} GCC_PACKED redirGenMsgHeader_t;

#endif // __REDIR_MSG_DEF_H__

