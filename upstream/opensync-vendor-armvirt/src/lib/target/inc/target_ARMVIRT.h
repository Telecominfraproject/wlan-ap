/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

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

#ifndef TARGET_ARMVIRT_H_INCLUDED
#define TARGET_ARMVIRT_H_INCLUDED

#include "dpp_client.h"
#include "dpp_survey.h"

#define TARGET_CERT_PATH            "/var/run/openvswitch/certs"
#define TARGET_MANAGERS_PID_PATH    "/tmp/dmpid"
#define TARGET_OVSDB_SOCK_PATH      "/var/run/openvswitch/db.sock"
#define TARGET_LOGREAD_FILENAME     "messages"

typedef struct
{
    DPP_TARGET_CLIENT_RECORD_COMMON_STRUCT;
    dpp_client_stats_t  stats;
} target_client_record_t;

typedef struct
{
    DPP_TARGET_SURVEY_RECORD_COMMON_STRUCT;
} target_survey_record_t;

typedef void target_capacity_data_t;

/******************************************************************************
 *  MANAGERS definitions
 *****************************************************************************/
#if !defined(CONFIG_TARGET_MANAGER)
#define TARGET_MANAGER_PATH(X)      "/usr/plume/bin/"X
#endif

#define TARGET_TOOLS_PATH           "/usr/plume/tools"

#include "target_common.h"

#endif /* TARGET_ARMVIRT_H_INCLUDED */
