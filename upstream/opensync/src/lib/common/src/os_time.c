/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "os_time.h"

/**
 * Return the current time in ticks
 */
int64_t ticks(void)
{
    return clock_ticks(CLOCK_MONOTONIC);
}

/**
 * Return the current ticks are registered by clock @p clk
 */
int64_t clock_ticks(clockid_t clk)
{
    struct timespec tv;

    /* Note that CLOCK_MONOTONIC may not be available on all platforms */
    clock_gettime(clk, &tv);

    return timespec_to_ticks(&tv);
}

int64_t timespec_to_ticks(struct timespec *ts)
{
    return TICKS_S(ts->tv_sec) + TICKS_NS(ts->tv_nsec);
}

void ticks_to_timespec(ticks_t t, struct timespec *ts)
{
    ts->tv_sec  = TICKS_TO_S(t);
    ts->tv_nsec = TICKS_TO_NS(TICKS_MOD(t));
}

int64_t timeval_to_ticks(struct timeval *tv)
{
    return TICKS_S(tv->tv_sec) + TICKS_US(tv->tv_usec);
}

time_t time_monotonic()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec;
}

time_t time_real()
{
    return time(NULL);
}

int64_t clock_real_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int64_t clock_mono_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int64_t clock_mono_usec()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

double clock_mono_double()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

double clock_sleep(double tts)
{
#if (_XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L) \
    && ( (!__UCLIBC__) || (__UCLIBC_HAS_THREADS_NATIVE__ && __UCLIBC_HAS_ADVANCED_REALTIME__))

    int rc;
    struct timespec to;
    struct timespec left;

    to.tv_sec  = tts;
    to.tv_nsec = (tts - to.tv_sec) * 1000000000.0;

    rc = clock_nanosleep(
           CLOCK_MONOTONIC,
           0,
           &to,
           &left);

    if (rc == 0) return 0.0;

    return (double)left.tv_sec + ((double)left.tv_nsec / 1000000000.0);

#else

    int rc;
    rc = usleep(tts * 1000000.0);
    return rc;

#endif
}

/**
 * Convert a time_t structure to the ISO 8601 date-time format in GMT.
 *
 * Local Slovenia time 9th March 2015 CEST, 3:41:45AM ->  2015-03-09T14:41:45+0000
 */
#ifndef _XOPEN_SOURCE
extern char *strptime(const char *s, const char *format, struct tm *tm);
#endif

bool time_to_str(time_t from, char *str, size_t strsz)
{
    struct tm   ltm;

    /*
     * Preferably we would store the time in the local timezone with the TZ offset encoded in the format,
     * however, strptime() is unable to parse the timezone, therefore we store the time as UTC/GMT
     */
    if (gmtime_r(&from, &ltm) == NULL)
    {
        return false;
    }

    if (strftime(str, strsz, TIME_ISO8601_STRFMT, &ltm) == 0)
    {
        return false;
    }

    return true;
}

/**
 * Convert a date/time ISO 8601 string to to a time_t value
 *
 * This is the opposite of time_to_str()
 */
bool time_from_str(time_t *to, char *str)
{
    struct tm   ltm;

    strptime(str, TIME_ISO8601_STRFMT, &ltm);

    /* Note that timegm() is not portable */
    *to = timegm(&ltm);
    if (*to == -1)
    {
        return false;
    }

    return true;
}


// monotonic clock in ev_tstamp (double) format
double ev_clock(void)
{
    static int have_monotonic = 1;
    struct timespec ts;
    if (have_monotonic)
    {
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        {
            // MONOTONIC not available (shouldn't really happen)
            fprintf(stderr, "CLOCK_MONOTONIC not available using CLOCK_REALTIME");
            have_monotonic = 0;
            goto no_mono;
        }
    }
    else
    {
        no_mono:
        clock_gettime(CLOCK_REALTIME, &ts);
    }
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

