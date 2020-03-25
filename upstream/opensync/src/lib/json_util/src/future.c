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

#define _GNU_SOURCE /* Needed for RTLD_NEXT */

#include <stdlib.h>
#include <jansson.h>
#include <dlfcn.h>

#include "log.h"

#if JSON_MAJOR_VERSION <= 2 && JANSSON_MINOR_VERSION < 8

static json_malloc_t    __json_malloc_fn    = malloc;
static json_free_t      __json_free_fn      = free;

void json_set_alloc_funcs(json_malloc_t malloc_fn, json_free_t free_fn)
{
    void (*set_func)(json_malloc_t, json_free_t);

    /* Resolve the *real* json_set_alloc_funcs() */
    set_func = dlsym(RTLD_NEXT, "json_set_alloc_funcs");
    if (set_func == NULL)
    {
        LOG(ERR, "Unable to resolve json_set_alloc_funcs(). Unable to override default allocation functions.");
        return;
    }

    __json_malloc_fn = malloc_fn;
    __json_free_fn = free_fn;

    set_func(malloc_fn, free_fn);
}

void json_get_alloc_funcs(json_malloc_t *malloc_fn, json_free_t *free_fn)
{
    *malloc_fn = __json_malloc_fn;
    *free_fn = __json_free_fn;
}

#endif
