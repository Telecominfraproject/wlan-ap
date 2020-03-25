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

/** Generate the C functions for parsing the schema file */
#include "schema.h"
#include "const.h"
#include "log.h"

#include "schema_gen.h"
#include "pjs_gen_c.h"

SCHEMA_LISTX(_SCHEMA_COL_IMPL)

SCHEMA_LISTX(_SCHEMA_IMPL_MARK_CHANGED)

SCHEMA_LISTX(_SCHEMA_IMPL_MARK_ALL_PRESENT)


void schema_filter_add(schema_filter_t *f, char *column)
{
    int idx = 0;
    if (f->num < 0 || f->num >= ARRAY_LEN(f->columns) - 1)
    {
        LOG(ERR, "Filter full %d/%d %s",
                f->num, ARRAY_LEN(f->columns) - 1, column);
        return;
    }
    // Does already exists
    for (idx=0; idx < f->num; idx++){
        if(!strcmp(f->columns[idx], column)){
            LOG(TRACE, "Filter already exists %s",
                    column);
            return;
        }
    }

    f->columns[f->num] = column;
    f->num++;
    f->columns[f->num] = NULL;
}

int schema_filter_get(schema_filter_t *f, char *column)
{
    int i;

    for (i = 0; i < f->num; i++)
        if (f->columns[i] && !strcmp(f->columns[i], column))
            return i;

    return -1;
}

void schema_filter_del(schema_filter_t *f, char *column)
{
    int i;
    int j;

    i = schema_filter_get(f, column);
    if (i < 0)
        return;

    /* The list is always terminated by NULL (see
     * schema_filter_add) at columns[f->num]. This loop
     * copies that over the last entry to maintain the
     * termination.
     */
    for (j = i; j < f->num; j++)
        f->columns[j] = f->columns[j+1];
}

void schema_filter_blacklist(schema_filter_t *f, char *column)
{
    if (!f)
        return;

    if (!f->columns[0])
        return;

    if (!strcmp("-", f->columns[0]) && schema_filter_get(f, column) < 0)
        schema_filter_add(f, column);

    if (!strcmp("+", f->columns[0]))
        schema_filter_del(f, column);
}

void schema_filter_init(schema_filter_t *f, char *op)
{
    memset(f, 0, sizeof(*f));
    schema_filter_add(f, op);
}

