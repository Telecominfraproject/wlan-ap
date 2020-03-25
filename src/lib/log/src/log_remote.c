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

#ifdef BUILD_REMOTE_LOG

#include <unistd.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/syscall.h>

#include "log.h"
#include "target.h"
#include "qm_conn.h"

extern log_module_entry_t log_module_remote[LOG_MODULE_ID_LAST];
extern bool log_remote_enabled;

void logger_remote_log(logger_t *self, logger_msg_t *msg)
{
    static bool inside_log = false;
    char msg_str[1024];

    if (!log_remote_enabled) return;

    if (inside_log) return; // prevent recursion

    inside_log = true;

    snprintf(msg_str, sizeof(msg_str), "[%5ld] %s %s: %s: %s\n",
            syscall(SYS_gettid),
            msg->lm_timestamp,
            log_get_name(),
            msg->lm_tag,
            msg->lm_text);

    qm_conn_send_log(msg_str, NULL);
    // ignore send errors

    inside_log = false;
}

bool logger_remote_match(log_severity_t sev, log_module_t module)
{
    return sev <= log_module_remote[module].severity;
}

bool logger_remote_new(logger_t *self)
{
    memset(self, 0, sizeof(*self));
    self->logger_fn = logger_remote_log;
    self->match_fn = logger_remote_match;
    return true;
}

#endif

