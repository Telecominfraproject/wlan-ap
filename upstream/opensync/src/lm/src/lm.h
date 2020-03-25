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

#ifndef __LOG_MANAGER_H__
#define __LOG_MANAGER_H__

#include <stdint.h>

#include "log.h"

typedef enum
{
    LM_REASON_AW_LM_CONFIG,
} lm_reason_e;

typedef struct
{
    char     upload_location[128];     /* To store the archived log/system status info. */
    char     upload_token[64];         /* To store the archived log/system status info. */
    uint32_t periodicity;              /* Periodicity for the periodic logging function */
} lm_config_t;

typedef struct
{
    ev_timer        timer;
    struct ev_loop *loop;
    lm_config_t     config;
    const char     *log_state_file;
} lm_state_t;

int lm_ovsdb_init(void);
void lm_update_state(lm_reason_e);
bool lm_hook_init(struct ev_loop *loop);
bool lm_hook_close();
bool lm_ovsdb_set_severity(const char *logger_name, const char *severity);

#endif  /* __LOG_MANAGER_H__ */
