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

#include <unistd.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/syscall.h>

#include "log.h"
#include "target.h"

static void
logger_traceback_log(logger_t *l, logger_msg_t *msg)
{
    static char msgs[32][1024];
    static size_t cur = 0;
    size_t i, j;

    if (msg->lm_module == LOG_MODULE_ID_TRACEBACK)
        return;

    if (msg->lm_severity <= LOG_SEVERITY_WARNING) {
        for (i = 0; i < ARRAY_SIZE(msgs); i++) {
            j = (cur + i) % ARRAY_SIZE(msgs);
            if (*msgs[j])
                mlog(msg->lm_severity, LOG_MODULE_ID_TRACEBACK, "%s", msgs[j]);
            *msgs[j] = 0;
        }
        return;
    }

    snprintf(msgs[cur], sizeof(msgs[cur]), "%s: %s", msg->lm_tag, msg->lm_text);
    cur++;
    cur %= ARRAY_SIZE(msgs);
}

static bool
logger_traceback_match(log_severity_t sev, log_module_t module)
{
    /* Traceback is intended to collect all possible
     * messages in a ring buffer to provide context for
     * errors with the need to increase log verbosity. The
     * goal is to make debugging easier.
     */
    return true;
}

bool
logger_traceback_new(logger_t *l)
{
    memset(l, 0, sizeof(*l));
    l->logger_fn = logger_traceback_log;
    l->match_fn = logger_traceback_match;
    return true;
}
