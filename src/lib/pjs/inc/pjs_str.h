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
 * This file is used to a C structure string description from PJS_* macros
 */
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <jansson.h>

#include "pjs_common.h"
#include "pjs_undef.h"

#define PJS(name, ...)                      char name ## _str[] = "struct " #name "\n{\n"  __VA_ARGS__ "};\n";

/*
 * =============================================================
 *  Standard types
 * =============================================================
 */
#define PJS_INT(name)                       "    int        "#name";\n"
#define PJS_BOOL(name)                      "    bool       "#name";\n"
#define PJS_REAL(name)                      "    double     "#name";\n"
#define PJS_STRING(name, len)               "    char       "#name"["#len"];\n"
#define PJS_SUB(name, sub)                  "    struct     "#sub" "#name";\n"

/*
 * =============================================================
 *  Optional types
 * =============================================================
 */
#define PJS_INT_Q(name)                     "    int        "#name";\n"                     \
                                            "    bool       "#name"_exists;\n"
#define PJS_BOOL_Q(name)                    "    bool       "#name";\n"                     \
                                            "    bool       "#name"_exists;\n"
#define PJS_STRING_Q(name, sz)              "    char       "#name"["#sz"];\n"              \
                                            "    bool       "#name"_exists;\n"
#define PJS_REAL_Q(name)                    "    double     "#name";\n"                     \
                                            "    bool       "#name" _exists;\n"
#define PJS_SUB_Q(name, sub)                "    struct     "#sub" "#name";\n"              \
                                            "    bool       "#name"_exists;\n"

/*
 * =============================================================
 *  Array types
 * =============================================================
 */
#define PJS_INT_A(name, sz)                 "    int        "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_BOOL_A(name, sz)                "    bool       "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_STRING_A(name, len, sz)         "    char       "#name"["#len"]["#sz"];\n"      \
                                            "    int        "#name"_len;\n"
#define PJS_REAL_A(name, sz)                "    double     "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_SUB_A(name, sub, sz)            "    struct "#sub"   "#name"["#sz"];\n"         \
                                            "    int        "#name"_len;\n"

/*
 * =============================================================
 *  Optional Array types
 * =============================================================
 */
#define PJS_INT_QA(name, sz)                "    int        "#name"["#sz"];\n"              \
                                            "    bool       "#name"_exists;\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_BOOL_QA(name, sz)               "    bool       "#name"["#sz"];\n"              \
                                            "    bool       "#name"_exists;\n"              \
                                            "    int         "#name"_len;\n"
#define PJS_STRING_QA(name, len, sz)        "    char       "#name"["#len"]["#sz"];\n"      \
                                            "    bool       "#name"_exists;\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_REAL_QA(name, sz)               "    double     "#name"["#sz"];\n"              \
                                            "    bool       "#name"_exists;\n"              \
                                            "    int        "#name"_len;\n"
#define PJS_SUB_QA(name, sub, sz)           "    struct "#sub"   "#name"["#sz"];\n"         \
                                            "    bool       "#name"_exists;\n"              \
                                            "    int        "#name"_len;\n"

/*
 * =============================================================
 *  OVS Basic Types
 * =============================================================
 */
#define PJS_OVS_INT(name)                   "    int        "#name";\n"
#define PJS_OVS_BOOL(name)                  "    bool       "#name";\n"
#define PJS_OVS_REAL(name)                  "    double     "#name";\n"
#define PJS_OVS_STRING(name, len)           "    char       "#name"["#len"];\n"
#define PJS_OVS_UUID(name)                  "    ovs_uuid_t "#name";\n"

/*
 * =============================================================
 *  OVS Basic Optional Types
 * =============================================================
 */
#define PJS_OVS_INT_Q(name)                 "    int        "#name";\n"                     \
                                            "    bool       "#name"_exists;\n"
#define PJS_OVS_BOOL_Q(name)                "    bool       "#name";\n"                     \
                                            "    bool       "#name"_exists;\n"
#define PJS_OVS_STRING_Q(name, sz)          "    char       "#name"["#sz"];\n"              \
                                            "    bool       "#name"_exists;\n"
#define PJS_OVS_REAL_Q(name)                "    double     "#name";\n"                     \
                                            "    bool       "#name" _exists;\n"
#define PJS_OVS_UUID_Q(name)                "    ovs_uuid_t "#name";\n"                     \
                                            "    bool       "#name"_exists;\n"

/*
 * =============================================================
 *  OVS Set type
 * =============================================================
 */
#define PJS_OVS_SET_INT(name, sz)           "    /* OVS_SET */\n"                           \
                                            "    int        "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SET_BOOL(name, sz)          "    /* OVS_SET */\n"                           \
                                            "    bool       "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SET_REAL(name, sz)          "    /* OVS_SET */\n"                           \
                                            "    double     "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SET_STRING(name, len, sz)   "    /* OVS_SET */\n"                           \
                                            "    char       "#name"["#len"]["#sz"];\n"      \
                                            "    int        "#name"_len;\n"


#define PJS_OVS_SET_UUID(name, sz)          "    /* OVS_SET */\n"                           \
                                            "    ovs_uuid_t "#name"["#sz"];\n"              \
                                            "    int        "#name"_len;\n"

/*
 * =============================================================
 *  OVS Map type (string key)
 * =============================================================
 */
#define PJS_OVS_SMAP_INT(name, sz)          "    int        "#name"["#sz"];\n"              \
                                            "    char       "#name"_keys["PJS_STR(PJS_OVS_MAP_KEYSZ)"]["#sz"];\n"   \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SMAP_BOOL(name, sz)         "    bool       "#name"["#sz"];\n"              \
                                            "    char       "#name"_keys["PJS_STR(PJS_OVS_MAP_KEYSZ)"]["#sz"];\n"   \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SMAP_REAL(name, sz)         "    double     "#name"["#sz"];\n"              \
                                            "    char       "#name"_keys["PJS_STR(PJS_OVS_MAP_KEYSZ)"]["#sz"];\n"   \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SMAP_STRING(name, len, sz)  "    char       "#name"["#len"]["#sz"];\n"      \
                                            "    char       "#name"_keys["PJS_STR(PJS_OVS_MAP_KEYSZ)"]["#sz"];\n"   \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_SMAP_UUID(name, sz)         "    ovs_uuid_t "#name"["#sz"];\n"              \
                                            "    char       "#name"_keys["PJS_STR(PJS_OVS_MAP_KEYSZ)"]["#sz"];\n"   \
                                            "    int        "#name"_len;\n"

/*
 * =============================================================
 *  OVS Map type (integer key)
 * =============================================================
 */
#define PJS_OVS_DMAP_INT(name, sz)          "    int        "#name"["#sz"];\n"              \
                                            "    int        "#name"_keys["#sz"];\n"         \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_DMAP_BOOL(name, sz)         "    bool       "#name"["#sz"];\n"              \
                                            "    int        "#name"_keys["#sz"];\n"         \
                                            "    int         "#name"_len;\n"

#define PJS_OVS_DMAP_REAL(name, sz)         "    double     "#name"["#sz"];\n"              \
                                            "    int        "#name"_keys["#sz"];\n"         \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_DMAP_STRING(name, len, sz)  "    char       "#name"["#len"]["#sz"];\n"      \
                                            "    int        "#name"_keys["#sz"];\n"         \
                                            "    int        "#name"_len;\n"

#define PJS_OVS_DMAP_UUID(name, sz)         "    ovs_uuid_t "#name"["#sz"];\n"              \
                                            "    int        "#name"_keys["#sz"];\n"         \
                                            "    int        "#name"_len;\n"

