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

#ifndef JSUTIL_H_INCLUDED
#define JSUTIL_H_INCLUDED

#include <ev.h>
#include <jansson.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  Future compatibility
 * ===========================================================================
 */

#if JANSSON_MAJOR_VERSION <= 2 && JANSSON_MINOR_VERSION < 8
extern void json_get_alloc_funcs(json_malloc_t *malloc_fn, json_free_t *free_fn);
#endif

/*
 * Declarations
 */

extern char *JSON_SPLIT_ERROR;

extern char        *json_split(char *str);

extern const char  *json_dumps_static(const json_t *json, int flags);
extern bool         json_gets(const json_t *json, char *output, size_t output_sz, int flags);
extern bool         json_get_str(const json_t *json, char *output, size_t output_sz);

extern void         json_memdbg_init(struct ev_loop *loop);
extern void         json_memdbg_free(void *p);
extern void         json_memdbg_get_stats(size_t *total, size_t *count);
extern void         json_memdbg_report(bool diff_only);

// JSON_MEMDBG_TRACE: use for debugging of json mem leaks
#undef JSON_MEMDBG_TRACE
#ifdef JSON_MEMDBG_TRACE
#warning JSON_MEMDBG_TRACE enabled use this only for debug builds!
#define JSON_LOG_REFCOUNT(X) TRACE("JSON REFC %s: %d %p", #X, X ? X->refcount : 0, X)
#else
#define JSON_LOG_REFCOUNT(X)
#endif

/*
 * ===========================================================================
 *  Inlines
 * ===========================================================================
 */

/*
 * Use this to free buffers acquired by json_dumps()!
 */
static inline void json_free(void *p)
{
    json_malloc_t   __unused;
    json_free_t     free_func;

    json_get_alloc_funcs(&__unused, &free_func);

    if (free_func != NULL) free_func(p);
}

#endif /* JSUTIL_H_INCLUDED */
