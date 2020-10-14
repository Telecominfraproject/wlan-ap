/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#ifdef CMAKE_BUILD
#include <lws_config.h>
#endif

#include <websocket/libwebsockets.h>
#include "redirtask.h"
#include "redirclient.h"
#include "redirmessagedef.h"
#include "redirintercomm.h"
#include "redirmsgdispatch.h"

int max_poll_elements;
struct pollfd *pollfds;
int *fd_lookup;
int count_pollfds;

static int  fd[2];

int redirInitInterComm()
{
	pipe(fd);
	max_poll_elements = getdtablesize();
	pollfds = malloc(max_poll_elements * sizeof(struct pollfd));
	fd_lookup = malloc(max_poll_elements * sizeof(int));
	if (pollfds == NULL || fd_lookup == NULL) {
		return -1;
	}
	return 0;
}

int redirGetReceiveFd()
{
	return fd[0];
}

int redirGetWriteFd()
{
    return fd[1];
}

static unsigned int cnSendSeqNum = 0;

int redirSendNotification( redirNotify_t * cNotify )
{
    ssize_t ret;
    int wfd = -1;

    cNotify->seqnum = cnSendSeqNum++;
    wfd = redirGetWriteFd();
    ret = write( wfd, cNotify, sizeof( redirNotify_t ) );
    if( ret < 0 )
        return -1;
    return 0;
}

int redirReceiveNotificationFromOtherEngines(struct lws_context *context, int rfd)
{
	char    readbuffer[200];

	read(rfd, readbuffer, sizeof(readbuffer));
	redirNotifyTxMessageIsReady(context, 0);

	return 0;
}

int redirAddPOnePollFd(int fd, int events)
{
	if (count_pollfds >= max_poll_elements) {
		return -1;
	}

	fd_lookup[fd] = count_pollfds;
	pollfds[count_pollfds].fd = fd;
	pollfds[count_pollfds].events = events;
	pollfds[count_pollfds++].revents = 0;
	return 0;
}

int redirRemoveOnePollFd(int fd)
{
	int temp;

	if (--count_pollfds <= 0) {
		return -1;
	}

	temp = fd_lookup[fd];
	/* have the last guy take up the vacant slot */
	pollfds[temp] = pollfds[count_pollfds];
	fd_lookup[pollfds[count_pollfds].fd] = temp;
	return 0;
}

int redirChangeModePollFd(int fd, int events)
{
	pollfds[fd_lookup[fd]].events = events;
	return 0;
}

int redirInitializePollFd()
{
	int rfd = -1;

	rfd = redirGetReceiveFd();
	fd_lookup[rfd] = count_pollfds;
	pollfds[count_pollfds].fd = rfd;
	pollfds[count_pollfds].events = POLLIN;
	pollfds[count_pollfds++].revents = 0;
	return 0;
}

int redirWaitforFds(struct lws_context *context, int timeout)
{
	int n = 0;

	// wait for 5 seconds.
	n = poll(pollfds, count_pollfds, timeout);
	if (n < 0)
		return -1;

	if (n) {
		if (pollfds[0].revents & POLLIN) {
			redirReceiveNotificationFromOtherEngines(context, pollfds[0].fd);
		}
	}

	for (n = 1; n < count_pollfds; n++) {
		if (pollfds[n].revents) {
			/*
			* returns immediately if the fd does not
			* match anything under libwebsockets
			* control
			*/
			if (lws_service_fd(context, &pollfds[n]) < 0) {

			}
		}
	}

	return 0;
}



