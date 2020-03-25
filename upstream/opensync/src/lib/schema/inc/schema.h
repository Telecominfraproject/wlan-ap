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

#ifndef SCHEMA_H_INCLUDED
#define SCHEMA_H_INCLUDED

#include <stddef.h>
#include <jansson.h>
#include <stdbool.h>

#include "schema_gen.h"
#include "pjs_gen_h.h"
#include "util.h"


#define SCHEMA_TABLE(table)                 SCHEMA__ ## table
#define SCHEMA_COLUMN(table, column)        SCHEMA__ ## table ## __ ## column
#define SCHEMA_COLUMN_LISTX(table, COL)     SCHEMA_COLUMN__ ## table(COL)
#define _STR_COMMA(X) #X,
#define SCHEMA_COLUMNS_LIST(table)          SCHEMA_COLUMN_LISTX(table, _STR_COMMA)
#define SCHEMA_COLUMNS_ARRAY(table)         schema_columns_##table
#define _SCHEMA_COL_DECL(X) extern char *SCHEMA_COLUMNS_ARRAY(X)[];
#define _SCHEMA_COL_IMPL(X) char *SCHEMA_COLUMNS_ARRAY(X)[] = { SCHEMA_COLUMNS_LIST(X) NULL };
SCHEMA_LISTX(_SCHEMA_COL_DECL)

#define SCHEMA_KEY_VAL(A, KEY) \
        fsa_find_key_val(*A##_keys, sizeof(*A##_keys), *A, sizeof(*A), A##_len, KEY)

#define SCHEMA_KEY_VAL_APPEND(FIELD, KEY, VALUE) \
        do { \
            STRSCPY(FIELD##_keys[FIELD##_len], (KEY)); \
            STRSCPY(FIELD[FIELD##_len], (VALUE)); \
            FIELD##_present = true; \
            FIELD##_len++; \
        } while(0)

#define SCHEMA_VAL_APPEND(FIELD, VALUE) \
        do { \
            STRSCPY(FIELD[FIELD##_len], (VALUE)); \
            FIELD##_present = true; \
            FIELD##_len++; \
        } while(0)

#define SCHEMA_VAL_APPEND_INT(FIELD, VALUE) \
        do { \
            FIELD[FIELD##_len] = (VALUE); \
            FIELD##_present = true; \
            FIELD##_len++; \
        } while(0)

#define SCHEMA_SET_INT(FIELD, VALUE) \
        do { \
            FIELD = (VALUE); \
            FIELD##_exists  = true; \
            FIELD##_present = true; \
        } while (0)

#define SCHEMA_CPY_INT(DST, SRC) \
        do { \
            DST = SRC; \
            DST##_exists  = SRC##_exists; \
            DST##_present = true; \
        } while (0)

#define SCHEMA_SET_STR(FIELD, VALUE) \
        do { \
            STRSCPY(FIELD, (VALUE)); \
            FIELD##_exists  = true; \
            FIELD##_present = true; \
        } while (0)

#define SCHEMA_CPY_STR(DST, SRC) \
        do { \
            STRSCPY(DST, SRC); \
            DST##_exists  = SRC##_exists; \
            DST##_present = true; \
        } while (0)

#define SCHEMA_SET_UUID(FIELD, VALUE) \
        do { \
            STRSCPY(FIELD.uuid, (VALUE)); \
            FIELD##_exists  = true; \
            FIELD##_present = true; \
        } while (0)

#define SCHEMA_KEY_VAL_APPEND_INT(FIELD, KEY, VALUE) \
        do { \
            STRSCPY(FIELD##_keys[FIELD##_len], KEY); \
            FIELD[FIELD##_len] = VALUE; \
            FIELD##_present = true; \
            FIELD##_len++; \
        } while(0)

#define SCHEMA_FILTER_LEN                   (SCHEMA_MAX_COLUMNS + 2)


// void schema_TABLE_mark_changed(schema old*, schema *rec);
// This will sets all rec->*_changed flag for each old->*_present flag
// (ovsdb monitor update sends old values only for changed fields)
#define _SCHEMA_DECL_MARK_CHANGED(TABLE) \
void schema_ ## TABLE ## _mark_changed(struct schema_##TABLE *old, struct schema_##TABLE *rec);

#define _SCHEMA_MARK_CHANGED_FIELD(COLUMN) \
        rec->COLUMN ## _changed = old->COLUMN ## _present;

#define _SCHEMA_IMPL_MARK_CHANGED(TABLE)                 \
void schema_ ## TABLE ## _mark_changed(                  \
        struct schema_##TABLE *old,                      \
        struct schema_##TABLE *rec)                      \
{                                                        \
    SCHEMA_COLUMN__ ## TABLE(_SCHEMA_MARK_CHANGED_FIELD) \
    _SCHEMA_MARK_CHANGED_FIELD(_version) \
}

SCHEMA_LISTX(_SCHEMA_DECL_MARK_CHANGED)

// void schema_TABLE_mark_all_present(schema *rec);
#define _SCHEMA_DECL_MARK_ALL_PRESENT(TABLE) \
void schema_ ## TABLE ## _mark_all_present(struct schema_##TABLE *rec);

#define _SCHEMA_MARK_PRESENT_FIELD(COLUMN) \
        rec->COLUMN ## _present = true;

#define _SCHEMA_IMPL_MARK_ALL_PRESENT(TABLE)             \
void schema_ ## TABLE ## _mark_all_present(              \
        struct schema_##TABLE *rec)                      \
{                                                        \
    SCHEMA_COLUMN__ ## TABLE(_SCHEMA_MARK_PRESENT_FIELD) \
}

SCHEMA_LISTX(_SCHEMA_DECL_MARK_ALL_PRESENT)


typedef struct schema_filter
{
    int num;
    char *columns[SCHEMA_FILTER_LEN]; // +1 type, +1 null terminated
} schema_filter_t;

void schema_filter_init(schema_filter_t *f, char *op);
void schema_filter_add(schema_filter_t *f, char *column);
int schema_filter_get(schema_filter_t *f, char *column);
void schema_filter_del(schema_filter_t *f, char *column);
void schema_filter_blacklist(schema_filter_t *f, char *column);

// field and filter set

// SCHEMA_FF_MARK: mark filter and _exists for a field
#define SCHEMA_FF_MARK_REQ(FILTER, SP, FIELD) \
    do { \
        schema_filter_add(FILTER, #FIELD); \
    } while(0)

#define SCHEMA_FF_MARK(FILTER, SP, FIELD) \
    do { \
        (SP)->FIELD##_exists = true; \
        schema_filter_add(FILTER, #FIELD); \
    } while(0)

// INT: can be used for int or bool
#define SCHEMA_FF_SET_INT(FILTER, SP, FIELD, VAL) \
    do { \
        (SP)->FIELD = VAL; \
        SCHEMA_FF_MARK(FILTER, SP, FIELD); \
    } while(0)

#define SCHEMA_FF_SET_INT_REQ(FILTER, SP, FIELD, VAL) \
    do { \
        (SP)->FIELD = VAL; \
        SCHEMA_FF_MARK_REQ(FILTER, SP, FIELD); \
    } while(0)

#define SCHEMA_FF_SET_STR(FILTER, SP, FIELD, VAL) \
    do { \
        STRSCPY((SP)->FIELD, VAL); \
        SCHEMA_FF_MARK(FILTER, SP, FIELD); \
    } while(0)

#define SCHEMA_FF_SET_STR_REQ(FILTER, SP, FIELD, VAL) \
    do { \
        STRSCPY((SP)->FIELD, VAL); \
        SCHEMA_FF_MARK_REQ(FILTER, SP, FIELD); \
    } while(0)

// schema field compare

#define SCHEMA_FIELD_CMP_INT_Q(src, dest, field) \
    ((dest)->field == (src)->field)

#define SCHEMA_FIELD_CMP_INT(src, dest, field) \
    ( \
      LOGT("%s %d %d == %d %d", #field, (src)->field##_exists, (src)->field,(dest)->field##_exists, (dest)->field), \
      ((src)->field##_exists != (dest)->field##_exists) ? false : \
    ( !(src)->field##_exists ? true : (dest)->field == (src)->field))

#define SCHEMA_FIELD_CMP_STR(src, dest, field) \
    ( \
      LOGT("%s %d %s == %d %s", #field, (src)->field##_exists, (src)->field,(dest)->field##_exists, (dest)->field), \
      ((src)->field##_exists != (dest)->field##_exists) ? false : \
    ( !(src)->field##_exists ? true : strcmp((dest)->field, (src)->field) == 0))

#define SCHEMA_FIELD_CMP_MAP_INT_STR(src, dest, field, index) \
    ( \
      LOGT("%s %d %s == %d %s", #field, (src)->field##_keys[index], (src)->field[index],(dest)->field##_keys[index], (dest)->field[index]), \
    (strcmp((dest)->field[index], (src)->field[index]) == 0))

#define SCHEMA_FIELD_CMP_MAP_STR(src, dest, field, index) \
    ( \
      LOGT("%s %s %s == %s %s", #field, (src)->field##_keys[index], (src)->field[index],(dest)->field##_keys[index], (dest)->field[index]), \
    (strcmp((dest)->field[index], (src)->field[index]) == 0))

#define SCHEMA_FIELD_CMP_LIST_STR(src, dest, field, index) \
    ( \
      LOGT("%s %d %s == %s", #field, index, (src)->field[index], (dest)->field[index]), \
    (strcmp((dest)->field[index], (src)->field[index]) == 0))

#define SCHEMA_FIELD_COPY_SET(src, dest, field) \
    do { \
        fsa_copy(*(src)->field, sizeof((src)->field[0]), ARRAY_LEN((src)->field), (src)->field##_len, \
                *(dest)->field, sizeof((dest)->field[0]), ARRAY_LEN((dest)->field), &(dest)->field##_len); \
    } while(0)

typedef int (*smap_cmp_fn_t)(const void *, const void *, size_t n);

static inline bool
schema_changed_map(const void *a_key,
                   const void *a_val,
                   const void *b_key,
                   const void *b_val,
                   const int a_len,
                   const int b_len,
                   const int key_size,
                   const int val_size,
                   smap_cmp_fn_t keycmp,
                   smap_cmp_fn_t valcmp)
{
    int i;
    int j;
    if (a_len != b_len)
        return true;
    for (i=0; i<a_len; i++) {
        for (j=0; j<a_len; j++)
            if (!keycmp(a_key + (i * key_size),
                        b_key + (j * key_size),
                        key_size))
                break;
        if (j == a_len)
            return true;
        if (valcmp(a_val + (i * val_size),
                   b_val + (j * val_size),
                   val_size))
            return true;
    }
    return false;
}

static inline bool
schema_changed_set(const void *a,
                   const void *b,
                   const int a_len,
                   const int b_len,
                   const int size,
                   int (*cmp)(const char *s1, const char *s2, size_t n))
{
    int i;
    int j;
    if (a_len != b_len)
        return true;
    for (i=0; i<a_len; i++) {
        for (j=0; j<a_len; j++)
            if (!cmp(a + (i * size),
                     b + (j * size),
                     size))
                break;
        if (j == a_len)
            return true;
    }
    return false;
}

#endif
