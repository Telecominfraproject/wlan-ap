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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "sm.h"
#include "os_time.h"
#include "log.h"


#define DIFF(a, b)    ((b >= a) ? (b - a) : (a - b))
#define MAX_DRIFT     (2000)  /* milliseconds */


void sm_sanity_check_report_timestamp(
        const char *log_prefix,
        uint64_t   timestamp_ms,  /* Report's timestamp (real time) to check. */
        uint64_t   *reporting_timestamp,  /* Base timestamp (real time) */
        uint64_t   *report_ts   /* Base timestamp (monotonic time) */
)
{
    uint64_t real_ms;
    uint64_t mono_ms;
    uint64_t diff_real_ms;

    real_ms = clock_real_ms();
    mono_ms = clock_mono_ms();

    diff_real_ms = DIFF(real_ms, timestamp_ms);
    if (diff_real_ms > MAX_DRIFT)
    {
        LOG(WARN, "%s: Report timestamp %"PRIu64" ms drifting for %"PRIu64" ms "
                  "from system wall clock. (Exceeding max drift %u ms). "
                  "Adjusting reporting's base timestamps. Effective with next report.",
                  log_prefix, timestamp_ms, diff_real_ms, MAX_DRIFT);

        *reporting_timestamp = real_ms;
        *report_ts = mono_ms;
    }
}

