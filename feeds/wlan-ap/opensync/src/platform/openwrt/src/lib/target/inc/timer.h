/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <sys/time.h>

struct timeout;
typedef void (*timeout_handler)(struct timeout *t);

struct timeout {
	bool pending;
	timeout_handler cb;
	struct timeval time;
};

int timeout_set(struct timeout *timeout, int msecs);
void timer_expiry_check(struct timeout *timeout);

#endif
