/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REDIRCLIENT_H_
#define _REDIRCLIENT_H_

#define MAX_REDIR_CLIENTS   5

#define WS_PING_INTERVAL            20  // in seconds
#define WS_RESET_LINK_INTERVAL      (3*WS_PING_INTERVAL)
#define WS_DEFAULT_PORT             7681
#define REDIR_RCV_ARRAY_MAX         10

typedef struct _redirConfig
{
	int use_mirror;
	int use_ssl;
	int port;
	int loglevel;
	unsigned int size;
	unsigned int pingsize;
	int flood;
	int clients;
	int wsLinkIsReady;
	unsigned int write_options;
	unsigned int pingIntervalSec;
        unsigned int wsResetIntervalSec;
	int ietf_version;
	int cfgDataLen;
	char *pCfgData;
	char address[30];
	char protocol_name[256];
	char urlname[30];
	char ssl_certdir[1024];
} redirConfig;

typedef struct _redirStatus
{
	int				wsIsReady;
	unsigned long 	lastSentSec;     // in seconds
    unsigned long   lastRecvSec;     // in seconds
} redirStatus;


extern redirConfig   redircfg;
extern redirStatus	redirst;

extern void* redirClientTask(void *handler);
extern int redirConfigInit( void );
extern int redirNotifyTxMessageIsReady(struct lws_context *context, unsigned long lastSentSec);
extern int redirWsLinkIsNotReady();
extern void redirDisableMsgSending();

#endif /* _REDIRCLIENT_H_ */

