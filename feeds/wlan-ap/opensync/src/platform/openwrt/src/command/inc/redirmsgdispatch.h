/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REDIRMSGDISPATCH_H_
#define _REDIRMSGDISPATCH_H_

#define REDIR_MSG_MAX_SIZE  8192

typedef struct _msgDispData
{
	int needToPing;
	int txMsgIsReady;
	unsigned char newCfgSeqCnt;
	unsigned char expectNewCfgSeq;

	// state machine parameters.
	int absTimeToWait;              // absolute time in second.
	int waitForMsg;
	int backOffTime;                // in seconds.
	int waitTimedOut;               // Timed out for the wait.
	int dropAllMsgBackOff;          // Drop all the frames during back off time.

	int wsIsReady;          // web socket is ready.
	int applyFailedNum;

	int applyNewCfgFailedNum;
        unsigned char smSeqCnt;
        unsigned char expectSeq;   //expected seqNum from Response message.

} msgDispData_t;


extern msgDispData_t gmsgdd;

extern int redirOpenRedirSocket( unsigned short Port );
extern void redirCloseRedirSocket( void );
extern void redirMessageRedirect( unsigned char * Msg, size_t Len );
extern void* redirMsgDispatchTaskFunc(void *x_void_ptr);
extern int redirNotifyWebSocketIsReady();


#endif /* _REDIRMSGDISPATCH_H_ */
