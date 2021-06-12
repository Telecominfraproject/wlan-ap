/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "log.h"
#include "evsched.h"

#include "timer.h"

static int tv_diff(struct timeval *t1, struct timeval *t2)
{
	return
			(t1->tv_sec - t2->tv_sec) * 1000 +
			(t1->tv_usec - t2->tv_usec) / 1000;
}

static void gettime(struct timeval *tv)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
}

int timeout_set(struct timeout *timeout, int msecs)
{
	if (!timeout) {
		LOGE("%s No timer data", __func__);
		return -1;
	}
	struct timeval *time = &timeout->time;

	if (timeout->pending)
		timeout->pending = false;

	gettime(time);

	time->tv_sec += msecs / 1000;
	time->tv_usec += (msecs % 1000) * 1000;

	if (time->tv_usec > 1000000) {
		time->tv_sec++;
		time->tv_usec -= 1000000;
	}
	timeout->pending = true;
	return 0;
}

void timer_expiry_check(struct timeout *t)
{
	struct timeval tv;
	gettime(&tv);
	if (t->pending && tv_diff(&t->time, &tv) <= 0) {
		t->pending = false;
		LOGI("%s Timer Expired..Executing callback", __func__);
		if (t->cb)
			t->cb(t);
	}
}
