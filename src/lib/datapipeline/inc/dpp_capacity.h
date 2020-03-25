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

#ifndef __HAVE_DPP_CAPACITY_H__
#define __HAVE_DPP_CAPACITY_H__

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

typedef struct
{
    uint64_t                        bytes_tx;
    uint32_t                        busy_tx;
    uint32_t                        samples;
    uint32_t                        queue[RADIO_QUEUE_MAX_QTY];
    uint64_t                        timestamp_ms;
} dpp_capacity_record_t;

typedef struct
{
    dpp_capacity_record_t           entry;
    ds_dlist_node_t                 node;
} dpp_capacity_record_list_t;

typedef ds_dlist_t                  dpp_capacity_list_t;

static inline dpp_capacity_record_list_t * dpp_capacity_record_alloc()
{
    dpp_capacity_record_list_t *record = NULL;

    record = malloc(sizeof(dpp_capacity_record_list_t));
    if (record)
    {
        memset(record, 0, sizeof(dpp_capacity_record_list_t));
    }

    return record;
}

static inline void dpp_capacity_record_free(dpp_capacity_record_list_t *record)
{
    if (NULL != record)
    {
        free(record);
    }
}

typedef struct
{
    radio_type_t                    radio_type;
    uint64_t                        timestamp_ms;
    dpp_capacity_list_t             list;
} dpp_capacity_report_data_t;

#endif /* __HAVE_DPP_CAPACITY_H__ */
