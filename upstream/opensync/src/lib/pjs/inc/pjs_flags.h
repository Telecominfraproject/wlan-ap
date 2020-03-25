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

/**
 * This file is used to generate pjs _flags structures from PJS_* macros
 */
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <jansson.h>

#include "pjs_common.h"
#include "pjs_undef.h"

#define PJS(name, ...)                                                              \
struct name ## _flags                                                               \
{                                                                                   \
    __VA_ARGS__                                                                     \
};                                                                                  \


#define PJS_FLAG(name)                      bool name;

/*
 * =============================================================
 *  Standard types
 * =============================================================
 */
#define PJS_INT(name)                       PJS_FLAG(name)
#define PJS_BOOL(name)                      PJS_FLAG(name)
#define PJS_REAL(name)                      PJS_FLAG(name)
#define PJS_STRING(name, len)               PJS_FLAG(name)
#define PJS_SUB(name, sub)                  PJS_FLAG(name)

/*
 * =============================================================
 *  Optional types
 * =============================================================
 */
#define PJS_INT_Q(name)                     PJS_FLAG(name)
#define PJS_BOOL_Q(name)                    PJS_FLAG(name)
#define PJS_STRING_Q(name, sz)              PJS_FLAG(name)
#define PJS_REAL_Q(name)                    PJS_FLAG(name)
#define PJS_SUB_Q(name, sub)                PJS_FLAG(name)

/*
 * =============================================================
 *  Array types
 * =============================================================
 */
#define PJS_INT_A(name, sz)                 PJS_FLAG(name)
#define PJS_BOOL_A(name, sz)                PJS_FLAG(name)
#define PJS_STRING_A(name, len, sz)         PJS_FLAG(name)
#define PJS_REAL_A(name, sz)                PJS_FLAG(name)
#define PJS_SUB_A(name, sub, sz)            PJS_FLAG(name)

/*
 * =============================================================
 *  Optional Array types
 * =============================================================
 */
#define PJS_INT_QA(name, sz)                PJS_FLAG(name)
#define PJS_BOOL_QA(name, sz)               PJS_FLAG(name)
#define PJS_STRING_QA(name, len, sz)        PJS_FLAG(name)
#define PJS_REAL_QA(name, sz)               PJS_FLAG(name)
#define PJS_SUB_QA(name, sub, sz)           PJS_FLAG(name)

/*
 * =============================================================
 *  OVS Basic Types
 * =============================================================
 */
#define PJS_OVS_INT(name)                   PJS_FLAG(name)
#define PJS_OVS_BOOL(name)                  PJS_FLAG(name)
#define PJS_OVS_REAL(name)                  PJS_FLAG(name)
#define PJS_OVS_STRING(name, len)           PJS_FLAG(name)
#define PJS_OVS_UUID(name)                  PJS_FLAG(name)

/*
 * =============================================================
 *  OVS Basic Optional Types
 * =============================================================
 */
#define PJS_OVS_INT_Q(name)                 PJS_FLAG(name)
#define PJS_OVS_BOOL_Q(name)                PJS_FLAG(name)
#define PJS_OVS_STRING_Q(name, sz)          PJS_FLAG(name)
#define PJS_OVS_REAL_Q(name)                PJS_FLAG(name)
#define PJS_OVS_UUID_Q(name)                PJS_FLAG(name)

/*
 * =============================================================
 * OVSDB Set(array)
 * =============================================================
 */
#define PJS_OVS_SET_INT(name, sz)               PJS_FLAG(name)
#define PJS_OVS_SET_BOOL(name, sz)              PJS_FLAG(name)
#define PJS_OVS_SET_REAL(name, sz)              PJS_FLAG(name)
#define PJS_OVS_SET_STRING(name, len, sz)       PJS_FLAG(name)
#define PJS_OVS_SET_UUID(name, sz)              PJS_FLAG(name)


/*
 * =============================================================
 * OVSDB Map(dictionary), where the key is a string
 * =============================================================
 */
#define PJS_OVS_SMAP_INT(name, sz)              PJS_FLAG(name)
#define PJS_OVS_SMAP_BOOL(name, sz)             PJS_FLAG(name)
#define PJS_OVS_SMAP_REAL(name, sz)             PJS_FLAG(name)
#define PJS_OVS_SMAP_STRING(name, len, sz)      PJS_FLAG(name)
#define PJS_OVS_SMAP_UUID(name, sz)             PJS_FLAG(name)

/*
 * =============================================================
 * OVSDB Map(dictionary), where the key is an int
 * =============================================================
 */
#define PJS_OVS_DMAP_INT(name, sz)              PJS_FLAG(name)
#define PJS_OVS_DMAP_BOOL(name, sz)             PJS_FLAG(name)
#define PJS_OVS_DMAP_REAL(name, sz)             PJS_FLAG(name)
#define PJS_OVS_DMAP_STRING(name, len, sz)      PJS_FLAG(name)
#define PJS_OVS_DMAP_UUID(name, sz)             PJS_FLAG(name)
