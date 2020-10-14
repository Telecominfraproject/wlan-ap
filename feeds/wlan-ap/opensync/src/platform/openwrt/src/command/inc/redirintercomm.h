/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REDIRINTERCOMM_H_
#define _REDIRINTERCOMM_H_

typedef struct _redirNotify {
	unsigned int	seqnum;
} redirNotify_t;

extern int redirInitInterComm();
extern int redirGetReceiveFd();
extern int redirGetWriteFd();
extern int redirAddPOnePollFd(int fd, int events);
extern int redirRemoveOnePollFd(int fd);
extern int redirChangeModePollFd(int fd, int events);
extern int redirInitializePollFd();
extern int redirWaitforFds(struct lws_context *context, int timeout);

#endif /* _REDIRINTERCOMM_H_ */

