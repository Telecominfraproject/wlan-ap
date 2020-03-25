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

#ifndef INET_UNIT_H_INCLUDED
#define INET_UNIT_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct __inet_unit inet_unit_t;

typedef bool inet_unit_walk_fn_t(inet_unit_t *unit, void *ctx, bool descend);
typedef bool inet_unit_commit_fn_t(void *ctx, intptr_t unit_id, bool enable);

struct __inet_unit
{
    intptr_t                    un_id;          /* ID of the current unit */
    inet_unit_t                 *un_parent;     /* Parent unit */
    size_t                      un_ndeps;       /* Number of elements in un_deps */
    inet_unit_t                 **un_deps;      /* Dependency/child units */
    bool                        un_status:1;    /* true if current unit has been successfully started */
    bool                        un_enabled:1;   /* true if service is enabled */
    bool                        un_pending:1;   /* true if pending stop or restart */
    bool                        un_error:1;     /* true if in error state */
};

extern inet_unit_t *inet_unit(intptr_t unit_id, inet_unit_t *dep, ...);
extern inet_unit_t *inet_unit_s(intptr_t unit_id, inet_unit_t *dep, ...);

extern bool inet_unit_walk(inet_unit_t *unit, inet_unit_walk_fn_t *fn, void *ctx);
extern void inet_unit_free(inet_unit_t *unit);
extern inet_unit_t *inet_unit_find(inet_unit_t *unit, intptr_t id);
extern bool inet_unit_start(inet_unit_t *unit, intptr_t unit_id);
extern bool inet_unit_stop(inet_unit_t *unit, intptr_t unit_id);
extern bool inet_unit_restart(inet_unit_t *unit, intptr_t unit_id, bool start_if_stopped);
extern bool inet_unit_enable(inet_unit_t *unit, intptr_t unit_id, bool enable);
extern bool inet_unit_is_enabled(inet_unit_t *unit, intptr_t unit_id);
extern bool inet_unit_status(inet_unit_t *unit, intptr_t unit_id);
extern bool inet_unit_commit(inet_unit_t *unit, inet_unit_commit_fn_t *fn, void *fn_ctx);

#endif /* INET_UNIT_H_INCLUDED */
