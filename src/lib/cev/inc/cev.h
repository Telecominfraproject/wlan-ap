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

#ifndef CEV_H_INCLUDED
#define CEV_H_INCLUDED

#include <stdbool.h>
#include <jansson.h>
#include <ev.h>

#include "crt.h"
#include "ovsdb.h"

typedef void CRT(cev_entry_t, void *data);

struct cev;

/* CEV wait object */
struct cev_wait
{
    bool                cw_pending;
    struct cev         *cw_cev;
};
typedef struct cev_wait cev_wait_t;

struct cev
{
    crt_t           cev_crt;        /* CRT context */
    void           *cev_data;       /* Data for the CRT function */
    cev_entry_t    *cev_entry;      /* CEV entry point */

    union
    {
        /* OVSDB tran call commands */
        struct
        {
            bool    is_error;       /* Result error status */
            json_t *result;         /* JSON result */
        }
        ovsdb_tran_call;

        struct
        {
            ev_timer    cev_sleep_timer;    /* Expiration timer */
            cev_wait_t  cev_sleep_wobj;     /* Wait object */
            bool        cev_sleep_expired;  /* Sleep expire? */
        };
    };
};
typedef struct cev cev_t;

#define CEV_INIT(cev, fn, data)     \
do                                  \
{                                   \
    CRT_INIT(&(cev)->cev_crt);      \
    (cev)->cev_entry = (fn);        \
    (cev)->cev_data = (data);       \
}                                   \
while (0)

#define CEV_RUN(cev) CRT_RUN(&(cev)->cev_crt, (cev)->cev_entry, (cev)->cev_data)

#define CEV_WAIT_INIT()             \
{                                   \
    .cw_cev = NULL,                 \
    .cw_wakeup = false,             \
    .cw_sleeping = false            \
}

extern void CRT(cev_wait, cev_wait_t *wait);
extern void cev_signal(cev_wait_t *wait, bool wakeup);

extern bool CRT(cev_sleep, int ms);
extern void cev_wakeup(cev_t *cev);

extern json_t *CRT(cev_ovsdb_echo, char *txt);
extern json_t *CRT(cev_ovsdb_monitor, int monid, char *table, int mon_flags, int coln, char *colv[]);
extern json_t *CRT(cev_ovsdb_transact, char *table, ovsdb_tro_t oper, json_t *where, json_t *row);
extern void CRT(cev_ovsdb_upsert, char *table, json_t *where, json_t *row);

#endif /* CEV_H_INCLUDED */
